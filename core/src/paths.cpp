#include "tinto/paths.hpp"

#include <cstdlib>

namespace tinto::paths {

namespace fs = std::filesystem;

fs::path home() {
  if (const char* h = std::getenv("TINTO_HOME"); h != nullptr && h[0] != '\0') {
    return fs::path(h);
  }
  const char* user_home = std::getenv("HOME");
#ifdef _WIN32
  if (user_home == nullptr) user_home = std::getenv("USERPROFILE");
#endif
  fs::path base = (user_home != nullptr) ? fs::path(user_home) : fs::current_path();
  return base / ".tinto";
}

fs::path profile() { return home() / "profile.json"; }
fs::path config() { return home() / "config.toml"; }
fs::path models_dir() { return home() / "models"; }
fs::path state_dir() { return home() / "state"; }
fs::path logs_dir() { return home() / "logs"; }
fs::path recordings_dir() { return home() / "recordings"; }
fs::path transcripts_dir() { return home() / "transcripts"; }

void ensure_layout() {
  std::error_code ec;
  for (const auto& dir : {home(), models_dir(), state_dir(), logs_dir(),
                          recordings_dir(), transcripts_dir()}) {
    fs::create_directories(dir, ec);
  }
}

}  // namespace tinto::paths
