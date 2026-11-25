#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <vector>

#include "tinto/capture.hpp"

namespace tinto {

// Place blocks from independently clocked devices on one monotonic timeline.
// Callbacks only add to the bounded ring. The pipeline thread drains the mix.
class AudioMixer {
 public:
  using Clock = std::chrono::steady_clock;

  void Reset(Clock::time_point epoch);
  void Push(const AudioFrame& frame);
  std::vector<float> Drain(Clock::time_point now, bool final);

 private:
  struct Slot {
    std::int64_t frame = -1;
    float sum = 0;
  };

  std::int64_t ToFrame(Clock::time_point time) const;

  std::mutex mutex_;
  Clock::time_point epoch_{};
  std::vector<Slot> ring_;
  std::array<std::optional<std::int64_t>, 2> next_{};
  std::int64_t read_ = 0;
  std::int64_t end_ = 0;
};

}  // namespace tinto
