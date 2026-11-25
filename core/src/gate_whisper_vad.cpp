#include "tinto/gate_factory.hpp"

#include <whisper.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tinto/gate.hpp"

namespace tinto {
namespace {

constexpr int kSampleRate = 16000;

// Run VAD only once this much audio is buffered (amortizes the full-buffer pass).
constexpr std::size_t kMinDrainSamples = 5 * kSampleRate;

// A segment is final only if it ends at least this far before the buffer end;
// otherwise speech may still be in progress and the audio is kept.
constexpr std::size_t kTailSamples = 1 * kSampleRate;

// Keep acoustic history before the stable boundary because each Silero pass
// starts with fresh recurrent state. Do not shift ordinary short pauses.
constexpr std::size_t kSilenceHistorySamples = 2 * kSampleRate;
constexpr std::size_t kSilenceTrimSamples = 5 * kSampleRate;

// Hard bound on buffered audio when no segment completes (e.g. VAD failure).
constexpr std::size_t kMaxBufferSamples = 60 * kSampleRate;

// Forces long speech runs to split so they complete and can be emitted.
constexpr float kMaxSpeechSec = 30.0f;

// whisper_vad_segments_get_segment_t0/t1 return centiseconds (10 ms units).
std::size_t CentisecondsToSamples(float cs) {
  if (cs <= 0.0f) return 0;
  return static_cast<std::size_t>(static_cast<double>(cs) * kSampleRate / 100.0);
}

double SamplesToSeconds(std::uint64_t samples) {
  return static_cast<double>(samples) / kSampleRate;
}

class WhisperVadGate final : public IGate {
 public:
  explicit WhisperVadGate(const std::string& vad_model_path)
      : ctx_(LoadContext(vad_model_path)), params_(whisper_vad_default_params()) {
    params_.max_speech_duration_s = kMaxSpeechSec;
    params_.min_silence_duration_ms = 300;
    params_.speech_pad_ms = 160;
  }

  ~WhisperVadGate() override {
    if (ctx_ != nullptr) whisper_vad_free(ctx_);
  }

  WhisperVadGate(const WhisperVadGate&) = delete;
  WhisperVadGate& operator=(const WhisperVadGate&) = delete;

  void Feed(const float* samples, std::size_t count) override {
    if (samples == nullptr || count == 0) return;
    buffer_.insert(buffer_.end(), samples, samples + count);
  }

  std::vector<SpeechSegment> Drain(bool final) override {
    std::vector<SpeechSegment> out;
    if (ctx_ == nullptr || buffer_.empty() ||
        (!final && buffer_.size() < kMinDrainSamples)) return out;

    std::size_t consumed = 0;
    whisper_vad_segments* segs = whisper_vad_segments_from_samples(
        ctx_, params_, buffer_.data(), static_cast<int>(buffer_.size()));
    if (segs != nullptr) {
      auto stable = final ? buffer_.size() : buffer_.size() - kTailSamples;
      const int n = whisper_vad_segments_n_segments(segs);
      for (int i = 0; i < n; ++i) {
        const std::size_t begin = std::min(
            CentisecondsToSamples(whisper_vad_segments_get_segment_t0(segs, i)),
            buffer_.size());
        const std::size_t end = std::min(
            CentisecondsToSamples(whisper_vad_segments_get_segment_t1(segs, i)),
            buffer_.size());
        if (end <= begin) continue;
        // Segments are ordered; once one is not final, later ones are not either.
        if (!final && end + kTailSamples > buffer_.size()) {
          stable = std::min(stable, begin);
          break;
        }

        SpeechSegment seg;
        seg.start_sec = SamplesToSeconds(offset_ + begin);
        seg.end_sec = SamplesToSeconds(offset_ + end);
        seg.samples.assign(buffer_.begin() + static_cast<std::ptrdiff_t>(begin),
                           buffer_.begin() + static_cast<std::ptrdiff_t>(end));
        out.push_back(std::move(seg));
        consumed = end;
      }
      // A long prefix without deferred speech need not be scanned on every
      // poll. Never compact into a speech segment that is still in progress.
      if (!final && stable >= consumed + kSilenceTrimSamples) {
        consumed = stable - kSilenceHistorySamples;
      }
      stable_through_ = std::max(stable_through_, offset_ + stable);
      whisper_vad_free_segments(segs);
    }

    if (buffer_.size() > kMaxBufferSamples) {
      consumed = std::max(consumed, buffer_.size() - kTailSamples);
    }

    if (consumed > 0) {
      buffer_.erase(buffer_.begin(),
                    buffer_.begin() + static_cast<std::ptrdiff_t>(consumed));
      offset_ += consumed;
    }
    return out;
  }

  double StableThrough() const override { return SamplesToSeconds(stable_through_); }

 private:
  static whisper_vad_context* LoadContext(const std::string& vad_model_path) {
    if (vad_model_path.empty()) return nullptr;
    const whisper_vad_context_params cparams = whisper_vad_default_context_params();
    return whisper_vad_init_from_file_with_params(vad_model_path.c_str(), cparams);
  }

  whisper_vad_context* ctx_;
  whisper_vad_params params_;
  std::vector<float> buffer_;  // unconsumed mono 16 kHz samples
  std::uint64_t offset_ = 0;   // absolute sample index of buffer_[0]
  std::uint64_t stable_through_ = 0;
};

}  // namespace

std::unique_ptr<IGate> MakeWhisperVadGate(const std::string& vad_model_path) {
  return std::make_unique<WhisperVadGate>(vad_model_path);
}

}  // namespace tinto
