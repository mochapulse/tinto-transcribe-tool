#pragma once

#include <string>

#include "tinto/profile.hpp"

// Runtime configuration. Precedence (resolved by the CLI):
//   CLI flag  >  machine profile (~/.tinto/profile.json)  >  built-in default.
namespace tinto {

enum class Mode {
  kRecordOnly,        // 1: keep audio, no transcript
  kRecordTranscribe,  // 2: keep audio + stream transcript to .md
  kTranscribeOnly,    // 3: transcript only, delete audio after processing
};

enum class Quality { kBase, kSmall, kMedium, kTurbo };
enum class Backend { kLocal, kGroq };
enum class Storage { kRam, kDisk };
enum class Format { kWav, kOpus };

struct Config {
  Mode mode = Mode::kRecordTranscribe;
  Quality quality = Quality::kSmall;  // live default; init may change per machine
  int threads = 0;                    // 0 = auto (physical cores, see profile)
  int cpu_budget = 0;                 // 0 = unset; else percentage
  std::string device;                 // capture device substring; empty = default
  Storage storage = Storage::kDisk;
  Format format = Format::kWav;
  int window_sec = 30;                // live transcription window
  bool diarize = false;
  bool word_timestamps = false;
  Backend backend = Backend::kLocal;
  std::string lang = "es";
  std::string out;                    // output path override; empty = ~/.tinto defaults
  bool foreground = false;            // record -f: do not daemonize
};

// Built-in defaults (lowest precedence).
Config DefaultConfig();

// Overlay profile defaults onto cfg. Only fields that cfg still has at their
// built-in default are overwritten; anything already changed (e.g. by a CLI
// flag applied before this call) is left alone.
void MergeProfile(Config& cfg, const Profile& profile);

// Human-readable names (for status output / logging).
const char* ToString(Mode m);
const char* ToString(Quality q);
const char* ToString(Backend b);

}  // namespace tinto
