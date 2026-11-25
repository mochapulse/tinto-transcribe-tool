#include "tinto/audio_mixer.hpp"

#include <algorithm>

namespace tinto {
namespace {

using FrameDuration = std::chrono::duration<std::int64_t, std::ratio<1, kCaptureSampleRate>>;
constexpr std::int64_t kCapacity = 30 * kCaptureSampleRate;
constexpr auto kHoldback = std::chrono::milliseconds(150);
constexpr std::int64_t kDiscontinuity = kCaptureSampleRate / 5;

}  // namespace

void AudioMixer::Reset(Clock::time_point epoch) {
  std::lock_guard<std::mutex> lock(mutex_);
  epoch_ = epoch;
  ring_.assign(kCapacity, Slot{});
  next_ = {};
  read_ = end_ = 0;
}

std::int64_t AudioMixer::ToFrame(Clock::time_point time) const {
  return std::chrono::duration_cast<FrameDuration>(time - epoch_).count();
}

void AudioMixer::Push(const AudioFrame& frame) {
  if (!frame.samples || frame.count == 0) return;
  std::lock_guard<std::mutex> lock(mutex_);

  auto& next = next_[frame.source == AudioSource::kInput ? 0 : 1];
  const auto count = static_cast<std::int64_t>(frame.count);
  const auto observed = ToFrame(frame.captured_at) - count;
  auto start = observed;
  auto first = start;
  if (next) {
    const auto error = observed - *next;
    if (error > -kDiscontinuity && error < kDiscontinuity) {
      // Nudge the stream by at most one frame per block to track clock drift
      // without making packet-arrival jitter an audible gap.
      constexpr std::int64_t deadband = kCaptureSampleRate / 100;
      start = *next + (error > deadband ? 1 : error < -deadband ? -1 : 0);
    }
    first = std::max(start, *next);  // Never sum overlapping blocks of one input.
  }
  auto finish = start + count;
  next = std::max(next.value_or(finish), finish);
  end_ = std::max(end_, finish);
  read_ = std::max(read_, end_ - kCapacity);

  for (auto index = std::max(first, read_); index < finish; ++index) {
    auto& slot = ring_[static_cast<std::size_t>(index % kCapacity)];
    if (slot.frame != index) slot = {index, 0};
    slot.sum += frame.samples[static_cast<std::size_t>(index - start)];
  }
}

std::vector<float> AudioMixer::Drain(Clock::time_point now, bool final) {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto limit = final ? end_ : std::min(end_, ToFrame(now - kHoldback));
  if (limit <= read_) return {};

  std::vector<float> result;
  result.reserve(static_cast<std::size_t>(limit - read_));
  for (; read_ < limit; ++read_) {
    const auto& slot = ring_[static_cast<std::size_t>(read_ % kCapacity)];
    result.push_back(slot.frame == read_ ? std::clamp(slot.sum, -1.0f, 1.0f) : 0);
  }
  return result;
}

}  // namespace tinto
