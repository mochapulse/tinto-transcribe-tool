// WAV writer and retention pruner.
//
// The miniaudio implementation lives in capture_miniaudio.cpp; this file only
// needs the declarations, so MINIAUDIO_IMPLEMENTATION must NOT be defined here.

#include "../vendor/miniaudio.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "tinto/storage.hpp"
#include "tinto/capture.hpp"

namespace tinto {

namespace fs = std::filesystem;

struct WavWriter::Impl {
  ma_encoder encoder{};
  bool open = false;
  std::uint32_t channels = 1;
  std::uint64_t frames = 0;
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
  impl_->frames = 0;
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
  impl_->frames += written;
  return result == MA_SUCCESS && written == frame_count;
}

bool WavWriter::WriteAligned(std::uint64_t start_frame, const float* samples,
                             std::size_t frame_count) {
  if (!impl_->open || impl_->channels != 1 || !samples) return false;
  if (frame_count == 0) return true;
  constexpr std::array<float, 4096> silence{};
  while (impl_->frames < start_frame) {
    const auto gap = std::min<std::uint64_t>(start_frame - impl_->frames, silence.size());
    if (!Write(silence.data(), static_cast<std::size_t>(gap))) return false;
  }
  const auto overlap = std::min<std::uint64_t>(impl_->frames - start_frame, frame_count);
  return Write(samples + overlap, frame_count - static_cast<std::size_t>(overlap));
}

void WavWriter::Close() {
  if (!impl_ || !impl_->open) return;
  ma_encoder_uninit(&impl_->encoder);
  impl_->open = false;
}

std::vector<float> ReadMonoWav(const fs::path& path) {
  ma_decoder decoder{};
  const auto cfg = ma_decoder_config_init(ma_format_f32, 1, kCaptureSampleRate);
#ifdef _WIN32
  const auto result = ma_decoder_init_file_w(path.c_str(), &cfg, &decoder);
#else
  const auto result = ma_decoder_init_file(path.c_str(), &cfg, &decoder);
#endif
  if (result != MA_SUCCESS) throw std::runtime_error("cannot read audio file");
  struct DecoderGuard {
    ma_decoder* decoder;
    ~DecoderGuard() { ma_decoder_uninit(decoder); }
  } guard{&decoder};

  ma_uint64 frames = 0;
  if (ma_decoder_get_length_in_pcm_frames(&decoder, &frames) != MA_SUCCESS ||
      frames == 0 || frames > std::numeric_limits<std::int32_t>::max()) {
    throw std::runtime_error("empty or unsupported audio length");
  }
  std::vector<float> samples(static_cast<std::size_t>(frames));
  ma_uint64 read = 0;
  const auto decoded = ma_decoder_read_pcm_frames(&decoder, samples.data(), frames, &read);
  if ((decoded != MA_SUCCESS && decoded != MA_AT_END) || read == 0) {
    throw std::runtime_error("cannot decode audio file");
  }
  samples.resize(static_cast<std::size_t>(read));
  return samples;
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
