#include <iostream>
#include <vector>

#include "tinto/capture.hpp"
#include "tinto/speech_window.hpp"

int main() {
  constexpr auto rate = tinto::kCaptureSampleRate;
  constexpr auto quarter = rate / 4;
  tinto::SpeechWindowBuffer joined(30);
  const tinto::SpeechSegment first{1.0, 1.5, std::vector<float>(2 * quarter, 1)};
  const tinto::SpeechSegment next{1.75, 2.25, std::vector<float>(2 * quarter, 2)};
  const tinto::SpeechSegment overlap{2.0, 2.5, std::vector<float>(2 * quarter, 3)};
  if (!joined.TakeReady({first}, 2.0, false).empty() ||
      !joined.TakeReady({next, overlap}, 6.25, false).empty()) {
    std::cerr << "Short regions were released before their surrounding context\n";
    return 1;
  }
  auto ready = joined.TakeReady({}, 6.5, false);
  if (ready.size() != 1 || ready[0].start_sec != 1.0 || ready[0].end_sec != 2.5 ||
      ready[0].samples.size() != 6 * quarter || ready[0].samples[0] != 1 ||
      ready[0].samples[2 * quarter] != 0 || ready[0].samples[3 * quarter] != 2 ||
      ready[0].samples[4 * quarter] != 2 || ready[0].samples[5 * quarter] != 3) {
    std::cerr << "Speech did not retain its timeline, silence, and overlap\n";
    return 1;
  }

  tinto::SpeechWindowBuffer limited(5);
  const tinto::SpeechSegment long_speech{2.0, 8.0, std::vector<float>(6 * rate, 1)};
  ready = limited.TakeReady({long_speech}, 8.0, false);
  if (ready.size() != 1 || ready[0].start_sec != 2.0 || ready[0].end_sec != 7.0 ||
      ready[0].samples.size() != 5 * rate) {
    std::cerr << "An ASR window exceeded the configured audio limit\n";
    return 1;
  }
  ready = limited.TakeReady({}, 8.0, true);
  if (ready.size() != 1 || ready[0].start_sec != 7.0 || ready[0].end_sec != 8.0 ||
      ready[0].samples.size() != rate) {
    std::cerr << "Finalization lost untranscribed audio at the end of the window\n";
    return 1;
  }
}
