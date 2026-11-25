#pragma once

#include <cstdint>
#include <vector>

#include "tinto/gate.hpp"

namespace tinto {

// Join adjacent speech regions for ASR without losing their time alignment.
class SpeechWindowBuffer {
 public:
  explicit SpeechWindowBuffer(int seconds);

  std::vector<SpeechSegment> TakeReady(const std::vector<SpeechSegment>& segments,
                                       double stable_through_sec, bool final);

 private:
  void Add(const SpeechSegment& segment, std::vector<SpeechSegment>& ready);
  void Release(std::vector<SpeechSegment>& ready);

  std::int64_t capacity_;
  std::int64_t start_ = 0;
  std::vector<float> pending_;
};

}  // namespace tinto
