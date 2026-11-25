#pragma once

#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <vector>

namespace tinto {

struct ModelAsset {
  const char* name;
  const char* url;
  std::uintmax_t bytes;
  const char* sha256;
};

enum class ModelState { kReady, kMissing, kWrongSize, kWrongHash, kUnreadable };

std::vector<ModelAsset> RequiredModels();
ModelState InspectModel(const std::filesystem::path& file, const ModelAsset& asset,
                        bool verify_hash);
const char* ToString(ModelState state);
bool ModelDownloaderAvailable();

// Populate the model cache for the features compiled into this binary.
// Downloads only absent or invalid files and never publishes a partial file.
void InstallModels(std::ostream& progress);

}  // namespace tinto
