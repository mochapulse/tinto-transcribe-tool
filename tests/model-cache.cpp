#include <filesystem>
#include <fstream>
#include <iostream>

#include "tinto/model_install.hpp"

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const std::filesystem::path path(argv[1]);
  using tinto::ModelState;
  const tinto::ModelAsset asset{"test.bin", "", 3,
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"};
  std::filesystem::remove(path);
  if (tinto::InspectModel(path, asset, true) != ModelState::kMissing) return 1;

  std::ofstream(path, std::ios::binary) << "x";
  if (tinto::InspectModel(path, asset, true) != ModelState::kWrongSize) return 1;
  std::ofstream(path, std::ios::binary) << "abd";
  if (tinto::InspectModel(path, asset, false) != ModelState::kReady ||
      tinto::InspectModel(path, asset, true) != ModelState::kWrongHash) return 1;
  std::ofstream(path, std::ios::binary) << "abc";
  const auto state = tinto::InspectModel(path, asset, true);
  std::filesystem::remove(path);
  if (state != ModelState::kReady) {
    std::cerr << "Known SHA-256 digest did not match: " << tinto::ToString(state) << '\n';
    return 1;
  }
}
