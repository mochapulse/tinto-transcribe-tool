#include "tinto/models.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "tinto/paths.hpp"

namespace tinto {
namespace {

namespace fs = std::filesystem;

std::string FindModel(const fs::path& dir,
                      const std::vector<std::string>& patterns) {
  std::error_code ec;
  if (!fs::is_directory(dir, ec)) return {};

  for (const auto& pattern : patterns) {
    for (fs::directory_iterator it(dir, ec), end; !ec && it != end;
         it.increment(ec)) {
      std::error_code entry_ec;
      if (!it->is_regular_file(entry_ec) && !it->is_symlink(entry_ec)) continue;
      const std::string name = it->path().filename().string();
      if (name.find(pattern) != std::string::npos &&
          name.find(".bin") != std::string::npos) {
        return it->path().string();
      }
    }
  }
  return {};
}

}  // namespace

std::string FindAsrModel(Quality quality) {
  const fs::path dir = paths::models_dir();

  switch (quality) {
    case Quality::kTurbo:
      return FindModel(dir, {"large-v3-turbo", "turbo"});
    case Quality::kMedium:
      return FindModel(dir, {"medium"});
    case Quality::kSmall:
      return FindModel(dir, {"small"});
    case Quality::kBase:
      return FindModel(dir, {"base"});
  }
  return {};
}

std::string FindVadModel() {
  return FindModel(paths::models_dir(), {"silero"});
}

}  // namespace tinto
