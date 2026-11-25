#include <atomic>
#include <iostream>
#include <vector>

#include "tinto/capture.hpp"
#include "tinto/transcriber_factory.hpp"

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  auto transcriber = tinto::MakeWhisperTranscriber(argv[1], "es", 2);
  constexpr auto sample_rate = tinto::kCaptureSampleRate;
  const std::vector<float> audio(4 * sample_rate, 0);
  std::atomic<unsigned> checks{0};
  const auto result = transcriber->Transcribe(audio.data(), audio.size(), sample_rate,
      [&] { return ++checks >= 3; });
  // The first two checks are before Whisper and before its encoder. The third
  // must come from Whisper itself once the encoder has started.
  if (checks < 3 || !result.interrupted) {
    std::cerr << "Whisper did not report cancellation of active inference\n";
    return 1;
  }
}
