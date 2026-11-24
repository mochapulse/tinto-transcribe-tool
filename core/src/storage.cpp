// WAV writer and retention pruner.
//
// The miniaudio implementation lives in capture_miniaudio.cpp; this file only
// needs the declarations, so MINIAUDIO_IMPLEMENTATION must NOT be defined here.

#include "../vendor/miniaudio.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "tinto/storage.hpp"

namespace tinto {

namespace fs = std::filesystem;

struct WavWriter::Impl {
  ma_encoder encoder{};
  bool open = false;
  std::uint32_t channels = 1;
};

WavWriter::WavWriter() : impl_(std::make_unique<Impl>()) {}

WavWriter::~WavWriter() { Close(); }

bool WavWriter::Open(const std::string& path, std::uint32_t sample_rate,
                     std::uint32_t channels) {
  Close();
  if (sample_rate == 0 || channels == 0) return false;

  ma_encoder_config config = ma_encoder_config_init(
      ma_encoding_format_wav, ma_format_f32, channels, sample_rate);

  ma_result result;
#ifdef _WIN32
  const std::wstring wpath = fs::path(path).wstring();
  result = ma_encoder_init_file_w(wpath.c_str(), &config, &impl_->encoder);
#else
  result = ma_encoder_init_file(path.c_str(), &config, &impl_->encoder);
#endif
  if (result != MA_SUCCESS) return false;

  impl_->open = true;
  impl_->channels = channels;
  return true;
}

bool WavWriter::Write(const float* samples, std::size_t frame_count) {
  if (!impl_->open) return false;
  if (frame_count == 0) return true;
  if (samples == nullptr) return false;

  ma_uint64 written = 0;
  const ma_result result =
      ma_encoder_write_pcm_frames(&impl_->encoder, samples,
                                  static_cast<ma_uint64>(frame_count), &written);
  return result == MA_SUCCESS && written == frame_count;
}

void WavWriter::Close() {
  if (!impl_ || !impl_->open) return;
  ma_encoder_uninit(&impl_->encoder);
  impl_->open = false;
}

std::uintmax_t PruneDirectory(const fs::path& dir,
                              std::uintmax_t max_total_bytes) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec) || ec) return 0;

  struct Entry {
    fs::path path;
    std::uintmax_t size;
    fs::file_time_type mtime;
  };

  std::vector<Entry> files;
  std::uintmax_t total = 0;

  for (fs::directory_iterator it(dir, ec), end; !ec && it != end;
       it.increment(ec)) {
    std::error_code entry_ec;
    if (!it->is_regular_file(entry_ec) || entry_ec) continue;

    const auto size = it->file_size(entry_ec);
    if (entry_ec) continue;
    const auto mtime = it->last_write_time(entry_ec);
    if (entry_ec) continue;

    files.push_back(Entry{it->path(), size, mtime});
    total += size;
  }

  if (total <= max_total_bytes) return 0;

  // Oldest first.
  std::sort(files.begin(), files.end(),
            [](const Entry& a, const Entry& b) { return a.mtime < b.mtime; });

  std::uintmax_t deleted = 0;
  for (const Entry& file : files) {
    if (total <= max_total_bytes) break;
    std::error_code remove_ec;
    if (!fs::remove(file.path, remove_ec) || remove_ec) continue;
    total -= file.size;
    deleted += file.size;
  }
  return deleted;
}

}  // namespace tinto
