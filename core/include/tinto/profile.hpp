#pragma once

#include <optional>
#include <string>

namespace tinto {

// Machine profile written by `tinto init` to ~/.tinto/profile.json.
// Precedence at runtime: CLI flag > machine profile > built-in default.
// Field names mirror examples/hardware-profiler/arch-09-profile.sh.
struct Profile {
  // hardware
  std::string os;
  std::string distro;
  std::string cpu;
  int cores_physical = 0;
  int threads_logical = 0;
  int ram_gb = 0;
  bool avx2 = false;
  std::string gpu;
  std::string accel;  // "cuda", "rocm" or "cpu"

  // chosen defaults
  std::string quality_live;
  std::string quality_batch;
  int threads_live = 1;
  int threads_batch = 1;
  int window_sec = 30;
  std::string lang = "es";
  std::string backend = "local";
};

// Detects hardware on the current platform and decides the default parameters.
Profile DetectAndDecide();

// Writes the profile to paths::profile(), creating the ~/.tinto layout first.
// Throws std::runtime_error if the file cannot be written.
void WriteProfile(const Profile& profile);

// Reads the profile back; std::nullopt if it is missing or malformed.
std::optional<Profile> ReadProfile();

}  // namespace tinto
