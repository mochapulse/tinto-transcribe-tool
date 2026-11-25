#include <whisper.h>

#include <algorithm>
#include <iostream>
#include <utility>
#include <vector>

#include "tinto/capture.hpp"
#include "tinto/gate_factory.hpp"

struct whisper_vad_context {};
struct whisper_vad_segments {
  std::vector<std::pair<int, int>> ranges;
};

namespace {

constexpr int kRate = tinto::kCaptureSampleRate;
int longest_scan = 0;

}  // namespace

whisper_vad_params whisper_vad_default_params() { return {}; }
whisper_vad_context_params whisper_vad_default_context_params() { return {}; }
whisper_vad_context* whisper_vad_init_from_file_with_params(
    const char*, whisper_vad_context_params) {
  return new whisper_vad_context;
}

void whisper_vad_free(whisper_vad_context* context) { delete context; }
void whisper_vad_free_segments(whisper_vad_segments* segments) { delete segments; }

whisper_vad_segments* whisper_vad_segments_from_samples(
    whisper_vad_context*, whisper_vad_params, const float* pcm, int count) {
  longest_scan = std::max(longest_scan, count);
  auto* segments = new whisper_vad_segments;
  int i = 0;
  while (i < count) {
    if (pcm[i] == 0) {
      ++i;
      continue;
    }
    const int start = i++;
    while (i < count && pcm[i] != 0) ++i;
    segments->ranges.emplace_back(start, i);
  }
  return segments;
}

int whisper_vad_segments_n_segments(whisper_vad_segments* s) {
  return static_cast<int>(s->ranges.size());
}

float whisper_vad_segments_get_segment_t0(whisper_vad_segments* s, int i) {
  return 100.0f * s->ranges[i].first / kRate;
}

float whisper_vad_segments_get_segment_t1(whisper_vad_segments* s, int i) {
  return 100.0f * s->ranges[i].second / kRate;
}

int main() {
  const std::vector<float> silence(kRate / 5, 0.0f);
  const std::vector<float> voice(kRate, 1.0f);
  auto gate = tinto::MakeWhisperVadGate("fixture");
  double stable = 0.0;
  for (int i = 0; i < 450; ++i) {
    gate->Feed(silence.data(), silence.size());
    const auto segments = gate->Drain();
    if (!segments.empty() || gate->StableThrough() < stable) {
      std::cerr << "Quiet compaction emitted speech or moved the watermark backwards\n";
      return 1;
    }
    stable = gate->StableThrough();
  }
  if (longest_scan > 7 * kRate || stable < 86.0) {
    std::cerr << "Long silence was repeatedly rescanned or held back the watermark\n";
    return 1;
  }
  gate->Feed(voice.data(), voice.size());
  auto segments = gate->Drain(true);
  if (segments.size() != 1 || segments[0].start_sec != 90.0 ||
      segments[0].end_sec != 91.0 || segments[0].samples != voice) {
    std::cerr << "The first utterance after long silence lost samples or timestamps\n";
    return 1;
  }

  // An unfinished utterance must keep its entire prefix even when another
  // scan would otherwise be able to trim a long span of audio.
  gate = tinto::MakeWhisperVadGate("fixture");
  for (int i = 0; i < 15; ++i) gate->Feed(silence.data(), silence.size());
  for (int i = 0; i < 8; ++i) {
    gate->Feed(voice.data(), voice.size());
    if (!gate->Drain().empty() || gate->StableThrough() > 3.0) {
      std::cerr << "The stable watermark passed an unfinished utterance\n";
      return 1;
    }
  }
  segments = gate->Drain(true);
  if (segments.size() != 1 || segments[0].start_sec != 3.0 ||
      segments[0].end_sec != 11.0 || segments[0].samples.size() != 8 * voice.size() ||
      !std::all_of(segments[0].samples.begin(), segments[0].samples.end(),
                   [](float sample) { return sample == 1.0f; })) {
    std::cerr << "An unfinished utterance was clipped during compaction\n";
    return 1;
  }
}
