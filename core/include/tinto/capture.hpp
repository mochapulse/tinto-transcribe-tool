#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

// Passive audio capture contract. Implementations MUST be a passive sniffer:
// shared-mode inputs only (never exclusive), outputs via loopback / sink
// monitor, never altering the system's audio routing. See the plan, §3.
namespace tinto {

inline constexpr std::uint32_t kCaptureSampleRate = 16000;
enum class AudioSource { kInput, kSystem };
enum class CaptureSelection { kDefault, kInput, kSystem, kBoth };

struct AudioFrame {
  const float* samples;        // interleaved, but core works in mono 16 kHz
  std::size_t count;           // sample count (not bytes)
  std::uint32_t sample_rate;
  std::uint32_t channels;
  AudioSource source;
  std::chrono::steady_clock::time_point captured_at;  // End of the block.
};

class ICapture {
 public:
  virtual ~ICapture() = default;

  using Callback = std::function<void(const AudioFrame&)>;

  // Open the selected sources in shared mode. Callbacks can run concurrently.
  virtual bool Start(CaptureSelection selection, const std::string& device_substring,
                     Callback cb) = 0;

  // Reconcile default endpoints on the pipeline thread, never in a callback.
  virtual void Refresh() = 0;

  // Stop and release the device. Idempotent.
  virtual void Stop() = 0;
};

}  // namespace tinto
