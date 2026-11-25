#pragma once

#include <cstddef>
#include <vector>

// Speech gate (VAD) contract. Fed mono 16 kHz float samples; emits speech
// segments and discards silence/noise. See the plan, §4.
namespace tinto {

struct SpeechSegment {
  double start_sec;           // start time relative to the capture session
  double end_sec;
  std::vector<float> samples;  // mono 16 kHz
};

class IGate {
 public:
  virtual ~IGate() = default;

  // Feed mono 16 kHz samples (any chunk size).
  virtual void Feed(const float* samples, std::size_t count) = 0;

  // Return ready speech segments. On the final call, flush trailing speech.
  virtual std::vector<SpeechSegment> Drain(bool final = false) = 0;

  // Latest session time before which no deferred speech remains.
  virtual double StableThrough() const = 0;
};

}  // namespace tinto
