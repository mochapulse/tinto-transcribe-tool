#pragma once

#include <filesystem>

// All tinto runtime data lives under a single root: $TINTO_HOME, or ~/.tinto.
// See the implementation plan ("Storage, CPU & data layout").
namespace tinto::paths {

// The root directory (~/.tinto by default, or $TINTO_HOME).
std::filesystem::path home();

std::filesystem::path profile();      // profile.json (machine profile from init)
std::filesystem::path config();       // config.toml (user config)
std::filesystem::path models_dir();   // models/  (GGML ASR + VAD cache)
std::filesystem::path state_dir();    // state/   (PID file + control socket)
std::filesystem::path logs_dir();     // logs/
std::filesystem::path recordings_dir();  // recordings/ (persisted audio)
std::filesystem::path transcripts_dir();  // transcripts/ (.md outputs)

// Create the full ~/.tinto layout if missing. Idempotent.
void ensure_layout();

}  // namespace tinto::paths
