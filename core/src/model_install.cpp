#include "tinto/model_install.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#include "tinto/paths.hpp"

namespace tinto {
namespace {

namespace fs = std::filesystem;

#ifdef _WIN32
std::wstring Quote(const std::wstring& arg) {
  std::wstring quoted = L"\"";
  std::size_t backslashes = 0;
  for (auto c : arg) {
    if (c == L'\\') {
      ++backslashes;
      continue;
    }
    quoted.append(backslashes * (c == L'\"' ? 2 : 1), L'\\');
    backslashes = 0;
    if (c == L'\"') quoted += L'\\';
    quoted += c;
  }
  quoted.append(backslashes * 2, L'\\');
  return quoted + L'\"';
}

fs::path CurlPath() {
  std::array<wchar_t, MAX_PATH + 1> folder{};
  const auto length = GetSystemDirectoryW(folder.data(), static_cast<UINT>(folder.size()));
  if (length == 0 || length >= folder.size()) return {};
  return fs::path(folder.data()) / L"curl.exe";
}

bool RunCurl(const std::vector<fs::path>& args) {
  const auto exe = CurlPath();
  if (exe.empty()) return false;
  auto command = Quote(exe.native());
  for (const auto& arg : args) command += L" " + Quote(arg.native());
  STARTUPINFOW startup{};
  startup.cb = sizeof startup;
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(exe.c_str(), command.data(), nullptr, nullptr, FALSE, 0,
                      nullptr, nullptr, &startup, &process)) return false;
  CloseHandle(process.hThread);
  DWORD result = 1;
  const bool finished = WaitForSingleObject(process.hProcess, INFINITE) == WAIT_OBJECT_0 &&
                        GetExitCodeProcess(process.hProcess, &result);
  CloseHandle(process.hProcess);
  return finished && result == 0;
}

std::string Sha256(const fs::path& file) {
  struct Handles {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    ~Handles() {
      if (hash) BCryptDestroyHash(hash);
      if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    }
  } handles;
  std::ifstream input(file, std::ios::binary);
  if (!input || BCryptOpenAlgorithmProvider(&handles.algorithm, BCRYPT_SHA256_ALGORITHM,
      nullptr, 0) < 0 || BCryptCreateHash(handles.algorithm, &handles.hash,
      nullptr, 0, nullptr, 0, 0) < 0) return {};
  std::array<unsigned char, 64 * 1024> buffer{};
  while (input) {
    input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
    const auto bytes = static_cast<ULONG>(input.gcount());
    if (bytes && BCryptHashData(handles.hash, buffer.data(), bytes, 0) < 0) return {};
  }
  std::array<unsigned char, 32> digest{};
  if (input.bad() || BCryptFinishHash(handles.hash, digest.data(),
      static_cast<ULONG>(digest.size()), 0) < 0) return {};
  constexpr char hex[] = "0123456789abcdef";
  std::string result;
  for (auto byte : digest) {
    result += hex[byte >> 4];
    result += hex[byte & 15];
  }
  return result;
}
#else
bool RunTool(std::vector<std::string> args, std::string* output = nullptr) {
  std::vector<char*> argv;
  for (auto& arg : args) argv.push_back(arg.data());
  argv.push_back(nullptr);
  int fds[2]{};
  if (output && pipe(fds) != 0) return false;
  posix_spawn_file_actions_t actions;
  const bool actions_ready = output && posix_spawn_file_actions_init(&actions) == 0;
  pid_t child{};
  int error = 0;
  if (output) {
    if (!actions_ready) error = EIO;
    if (!error) error = posix_spawn_file_actions_addclose(&actions, fds[0]);
    if (!error) error = posix_spawn_file_actions_adddup2(&actions, fds[1], STDOUT_FILENO);
    if (!error) error = posix_spawn_file_actions_addclose(&actions, fds[1]);
  }
  if (!error) {
    error = posix_spawnp(&child, argv.front(), output ? &actions : nullptr,
                         nullptr, argv.data(), environ);
  }
  if (actions_ready) posix_spawn_file_actions_destroy(&actions);
  if (output) {
    close(fds[1]);
    char buffer[4096];
    while (!error) {
      const auto count = read(fds[0], buffer, sizeof buffer);
      if (count > 0) output->append(buffer, static_cast<std::size_t>(count));
      else if (count == 0 || errno != EINTR) break;
    }
    close(fds[0]);
  }
  if (error) return false;
  int status = 0;
  pid_t waited;
  do { waited = waitpid(child, &status, 0); } while (waited < 0 && errno == EINTR);
  return waited == child && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool RunCurl(const std::vector<fs::path>& args) {
  std::vector<std::string> command{"curl"};
  for (const auto& arg : args) command.push_back(arg.string());
  return RunTool(std::move(command));
}

std::string Sha256(const fs::path& file) {
  std::string output;
  if (!RunTool({"sha256sum", "--", file.string()}, &output)) return {};
  // GNU sha256sum prefixes a backslash when the file name needs escaping.
  const std::size_t start = !output.empty() && output.front() == '\\' ? 1 : 0;
  return output.size() > start + 64 ? output.substr(start, 64) : std::string{};
}
#endif

}  // namespace

std::vector<ModelAsset> RequiredModels() {
  return {
#ifdef TINTO_WITH_WHISPER
    {"ggml-base-q5_1.bin", "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base-q5_1.bin", 59707625,
     "422f1ae452ade6f30a004d7e5c6a43195e4433bc370bf23fac9cc591f01a8898"},
    {"ggml-small-q5_1.bin", "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-small-q5_1.bin", 190085487,
     "ae85e4a935d7a567bd102fe55afc16bb595bdb618e11b2fc7591bc08120411bb"},
    {"ggml-medium-q5_0.bin", "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-medium-q5_0.bin", 539212467,
     "19fea4b380c3a618ec4723c3eef2eb785ffba0d0538cf43f8f235e7b3b34220f"},
    {"ggml-large-v3-turbo-q5_0.bin", "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-large-v3-turbo-q5_0.bin", 574041195,
     "394221709cd5ad1f40c46e6031ca61bce88931e6e088c188294c6d5a55ffa7e2"},
    {"ggml-silero-v5.1.2.bin", "https://huggingface.co/ggml-org/whisper-vad/resolve/main/ggml-silero-v5.1.2.bin", 885098,
     "29940d98d42b91fbd05ce489f3ecf7c72f0a42f027e4875919a28fb4c04ea2cf"},
#endif
#ifdef TINTO_WITH_DIARIZATION
    {"diarization/segmentation.int8.onnx", "https://huggingface.co/csukuangfj/sherpa-onnx-pyannote-segmentation-3-0/resolve/main/model.int8.onnx", 1540506,
     "d582f4b4c6b48205de7e0643c57df0df5615a3c176189be3fc461e9d18827b5d"},
    {"diarization/embedding.onnx", "https://github.com/k2-fsa/sherpa-onnx/releases/download/speaker-recongition-models/nemo_en_titanet_small.onnx", 40257283,
     "ad4a1802485d8b34c722d2a9d04249662f2ece5d28a7a039063ca22f515a789e"},
#endif
  };
}

ModelState InspectModel(const fs::path& file, const ModelAsset& asset, bool verify_hash) {
  std::error_code error;
  if (!fs::is_regular_file(file, error)) {
    if (!error || error == std::errc::no_such_file_or_directory) return ModelState::kMissing;
    return ModelState::kUnreadable;
  }
  const auto size = fs::file_size(file, error);
  if (error) return ModelState::kUnreadable;
  if (size != asset.bytes) return ModelState::kWrongSize;
  if (!verify_hash) return ModelState::kReady;
  const auto digest = Sha256(file);
  if (digest.empty()) return ModelState::kUnreadable;
  return digest == asset.sha256 ? ModelState::kReady : ModelState::kWrongHash;
}

const char* ToString(ModelState state) {
  switch (state) {
    case ModelState::kReady: return "ready";
    case ModelState::kMissing: return "missing";
    case ModelState::kWrongSize: return "wrong size";
    case ModelState::kWrongHash: return "checksum mismatch";
    case ModelState::kUnreadable: return "unreadable";
  }
  return "unknown";
}

bool ModelDownloaderAvailable() {
#ifdef _WIN32
  std::error_code error;
  return fs::is_regular_file(CurlPath(), error);
#else
  std::string ignored;
  return RunTool({"curl", "--version"}, &ignored);
#endif
}

void InstallModels(std::ostream& progress) {
  for (const auto& asset : RequiredModels()) {
    const auto file = paths::models_dir() / asset.name;
    const auto state = InspectModel(file, asset, true);
    if (state == ModelState::kReady) {
      progress << "init: verified " << asset.name << '\n';
      continue;
    }
    if (!ModelDownloaderAvailable()) throw std::runtime_error("curl is required to download models");
    fs::create_directories(file.parent_path());
    const auto nonce = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto staging = file.parent_path() / (".download-" + std::to_string(nonce));
    try {
      progress << "init: downloading " << asset.name << " (" << ToString(state) << ')' << std::endl;
      if (!RunCurl({"--fail", "--location", "--silent", "--show-error", "--retry", "3",
                    "--connect-timeout", "20", "--proto", "=https", "--proto-redir", "=https",
                    "--output", staging, asset.url})) {
        throw std::runtime_error("download failed: " + std::string(asset.name));
      }
      if (InspectModel(staging, asset, true) != ModelState::kReady) {
        throw std::runtime_error("integrity check failed: " + std::string(asset.name));
      }
      fs::rename(staging, file);
    } catch (...) {
      std::error_code ignored;
      fs::remove(staging, ignored);
      throw;
    }
  }
}

}  // namespace tinto
