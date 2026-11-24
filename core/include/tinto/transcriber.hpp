#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Transcription contract. Implementations wrap whisper.cpp (local) or the Groq
// backend. Live mode must feed ~30 s windows, not tiny chunks (plan, §4).
namespace tinto {

struct Token {
  std::string text;
  double start_sec;  // word-level timing when available (DTW), else segment
  double end_sec;
};

struct Transcript {
  std::string text;            // full text of this window
  std::vector<Token> tokens;   // optional word-level timing
};

class ITranscriber {
 public:
  virtual ~ITranscriber() = default;

  // Transcribe a window of mono audio. `sample_rate` is normally 16000.
  virtual Transcript Transcribe(const float* samples, std::size_t count,
                                std::uint32_t sample_rate) = 0;
};

}  // namespace tinto
