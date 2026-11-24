#include "tinto/config.hpp"

namespace tinto {

Config DefaultConfig() { return Config{}; }

const char* ToString(Mode m) {
  switch (m) {
    case Mode::kRecordOnly:       return "record-only";
    case Mode::kRecordTranscribe: return "record+transcribe";
    case Mode::kTranscribeOnly:   return "transcribe-only";
  }
  return "unknown";
}

const char* ToString(Quality q) {
  switch (q) {
    case Quality::kBase:   return "base";
    case Quality::kSmall:  return "small";
    case Quality::kMedium: return "medium";
    case Quality::kTurbo:  return "turbo";
  }
  return "unknown";
}

}  // namespace tinto
