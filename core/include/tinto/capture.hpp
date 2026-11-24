#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

// Passive audio capture contract. Implementations MUST be a passive sniffer:
// shared-mode inputs only (never exclusive), outputs via loopback / sink
// monitor, never altering the system's audio routing. See the plan, §3.
namespace tinto {

struct AudioFrame {
  const float* samples;        // interleaved, but core works in mono 16 kHz
  std::size_t count;           // sample count (not bytes)
  std::uint32_t sample_rate;
  std::uint32_t channels;
};

class ICapture {
 public:
  virtual ~ICapture() = default;

  using Callback = std::function<void(const AudioFrame&)>;

  // Open `device_substring` (empty = default) in SHARED mode and stream frames
  // to `cb`. Returns false if the device could not be opened. Non-blocking;
  // audio arrives on the capture thread.
  virtual bool Start(const std::string& device_substring, Callback cb) = 0;

  // Stop and release the device. Idempotent.
  virtual void Stop() = 0;
};

}  // namespace tinto
