#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

// Storage helpers: a WAV writer for captured audio and a size-based retention
// pruner for the recordings directory. Audio is files, never a database.
namespace tinto {

// Writes PCM float32 audio to a WAV file. Mono 16 kHz is the normal case.
class WavWriter {
 public:
  WavWriter();
  ~WavWriter();

  WavWriter(const WavWriter&) = delete;
  WavWriter& operator=(const WavWriter&) = delete;

  // Create (or truncate) `path` and prepare it for interleaved f32 frames.
  // Returns false if the file could not be created. Reopening closes first.
  bool Open(const std::string& path, std::uint32_t sample_rate = 16000,
            std::uint32_t channels = 1);

  // Append `frame_count` frames (frame_count * channels samples) from
  // `samples`. Returns false if the writer is not open or the write failed.
  bool Write(const float* samples, std::size_t frame_count);

  // Write mono audio on the session timeline. Fill gaps with silence and trim
  // overlap so offsets in the file continue to match transcript timestamps.
  bool WriteAligned(std::uint64_t start_frame, const float* samples,
                    std::size_t frame_count);

  // Finalize the WAV header and release the file. Idempotent.
  void Close();

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Read a local audio file as 16 kHz mono float PCM. Throws on invalid input.
std::vector<float> ReadMonoWav(const std::filesystem::path& path);

// Deletes the oldest regular files directly inside `dir` (by last_write_time)
// until their combined size is at most `max_total_bytes`. Subdirectories are
// ignored. Returns the number of bytes deleted. A missing directory is a no-op
// that returns 0.
std::uintmax_t PruneDirectory(const std::filesystem::path& dir,
                              std::uintmax_t max_total_bytes);

}  // namespace tinto
