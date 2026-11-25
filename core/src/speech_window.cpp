#include "tinto/speech_window.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "tinto/capture.hpp"

namespace tinto {
namespace {

constexpr std::int64_t kPauseFrames = 4 * kCaptureSampleRate;

std::int64_t ToFrame(double seconds) {
  return std::llround(seconds * kCaptureSampleRate);
}

}  // namespace

SpeechWindowBuffer::SpeechWindowBuffer(int seconds)
    : capacity_(std::clamp(seconds, 5, 30) * kCaptureSampleRate) {}

std::vector<SpeechSegment> SpeechWindowBuffer::TakeReady(
    const std::vector<SpeechSegment>& segments, double stable_through_sec, bool final) {
  std::vector<SpeechSegment> ready;
  for (const auto& segment : segments) Add(segment, ready);
  const auto end = start_ + static_cast<std::int64_t>(pending_.size());
  if (final || ToFrame(stable_through_sec) - end >= kPauseFrames) Release(ready);
  return ready;
}

void SpeechWindowBuffer::Add(const SpeechSegment& segment, std::vector<SpeechSegment>& ready) {
  const auto first = ToFrame(segment.start_sec);
  const auto last = first + static_cast<std::int64_t>(segment.samples.size());
  auto cursor = first;
  while (cursor < last) {
    if (pending_.empty()) start_ = cursor;
    const auto end = start_ + static_cast<std::int64_t>(pending_.size());
    cursor = std::max(cursor, end);  // Do not duplicate overlapping VAD padding.
    if (cursor >= last) break;
    const auto gap = cursor - end;
    if (gap > kPauseFrames || cursor >= start_ + capacity_) {
      Release(ready);
      continue;
    }
    pending_.insert(pending_.end(), static_cast<std::size_t>(gap), 0.0f);
    const auto count = std::min(last, start_ + capacity_) - cursor;
    const auto begin = segment.samples.begin() + (cursor - first);
    pending_.insert(pending_.end(), begin, begin + count);
    cursor += count;
    if (cursor == start_ + capacity_) Release(ready);
  }
}

void SpeechWindowBuffer::Release(std::vector<SpeechSegment>& ready) {
  if (pending_.empty()) return;
  auto end = start_ + static_cast<std::int64_t>(pending_.size());
  ready.push_back({static_cast<double>(start_) / kCaptureSampleRate,
                   static_cast<double>(end) / kCaptureSampleRate, std::move(pending_)});
  pending_.clear();
}

}  // namespace tinto
