#include "tinto/config.hpp"

namespace tinto {

Config DefaultConfig() { return Config{}; }

namespace {

Quality ParseQuality(const std::string& s) {
  if (s == "base") return Quality::kBase;
  if (s == "small") return Quality::kSmall;
  if (s == "medium") return Quality::kMedium;
  if (s.find("turbo") != std::string::npos) return Quality::kTurbo;
  return Quality::kSmall;  // unknown: keep the live default
}

}  // namespace

void MergeProfile(Config& cfg, const Profile& profile) {
  if (cfg.quality == Config{}.quality) {
    cfg.quality = ParseQuality(profile.quality_live);
  }
  if (cfg.threads == 0) {
    cfg.threads = profile.threads_live;
  }
  if (cfg.window_sec == Config{}.window_sec) {
    cfg.window_sec = profile.window_sec;
  }
  if (cfg.lang == Config{}.lang) {
    cfg.lang = profile.lang;
  }
  if (cfg.backend == Backend::kLocal) {
    if (profile.backend == "groq") cfg.backend = Backend::kGroq;
    else if (profile.backend == "local") cfg.backend = Backend::kLocal;
  }
}

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

const char* ToString(Backend b) {
  switch (b) {
    case Backend::kLocal: return "local";
    case Backend::kGroq:  return "groq";
  }
  return "unknown";
}

}  // namespace tinto
