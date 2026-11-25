#include "tinto/profile.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

#include "json.hpp"
#include "tinto/paths.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

namespace tinto {
namespace {

namespace fs = std::filesystem;

// ordered_json keeps the key order of arch-09-profile.sh (plain json sorts keys).
using json = nlohmann::ordered_json;

#ifdef _WIN32
std::string DetectCpuName() {
  wchar_t name[256] = {};
  DWORD size = sizeof name;
  if (RegGetValueW(HKEY_LOCAL_MACHINE,
                   L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
                   L"ProcessorNameString", RRF_RT_REG_SZ, nullptr, name,
                   &size) != ERROR_SUCCESS) {
    return {};
  }

  const int length = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0,
                                         nullptr, nullptr);
  if (length <= 1) return {};
  std::string utf8(length, '\0');
  if (WideCharToMultiByte(CP_UTF8, 0, name, -1, utf8.data(), length, nullptr,
                          nullptr) == 0) {
    return {};
  }
  utf8.pop_back();  // The Windows conversion includes the terminating NUL.
  return utf8;
}

int DetectPhysicalCores() {
  DWORD bytes = 0;
  GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &bytes);
  if (bytes == 0) return 0;

  using Entry = SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX;
  std::vector<Entry> buffer((bytes + sizeof(Entry) - 1) / sizeof(Entry));
  if (!GetLogicalProcessorInformationEx(RelationProcessorCore, buffer.data(),
                                        &bytes)) {
    return 0;
  }

  int cores = 0;
  const auto* data = reinterpret_cast<const unsigned char*>(buffer.data());
  for (DWORD offset = 0; offset < bytes;) {
    const auto* entry = reinterpret_cast<const Entry*>(data + offset);
    if (entry->Size == 0 || entry->Size > bytes - offset) return 0;
    if (entry->Relationship == RelationProcessorCore) ++cores;
    offset += entry->Size;
  }
  return cores;
}

void DetectHardware(Profile& p) {
  p.os = "Windows";
  p.distro = "n/a";
  p.cpu = DetectCpuName();
  p.cores_physical = DetectPhysicalCores();
  p.threads_logical =
      static_cast<int>(GetActiveProcessorCount(ALL_PROCESSOR_GROUPS));
  MEMORYSTATUSEX memory{};
  memory.dwLength = sizeof memory;
  if (GlobalMemoryStatusEx(&memory)) {
    p.ram_gb = static_cast<int>(memory.ullTotalPhys / (1024ull * 1024 * 1024));
  }
  p.avx2 = IsProcessorFeaturePresent(PF_AVX2_INSTRUCTIONS_AVAILABLE) != 0;
  // This Windows build path uses CPU inference. A GPU alone does not imply
  // that the corresponding whisper backend is enabled.
  p.accel = "cpu";
}
#else
constexpr const char* kWhitespace = " \t\r\n";

std::string Trim(const std::string& s) {
  const auto begin = s.find_first_not_of(kWhitespace);
  if (begin == std::string::npos) return "";
  const auto end = s.find_last_not_of(kWhitespace);
  return s.substr(begin, end - begin + 1);
}

std::vector<std::string> SplitLines(const std::string& text) {
  std::vector<std::string> lines;
  std::istringstream in(text);
  for (std::string line; std::getline(in, line);) lines.push_back(line);
  return lines;
}

// Parses a non-negative integer; returns 0 when the text is not a number.
int ToInt(const std::string& text) {
  char* end = nullptr;
  const long value = std::strtol(text.c_str(), &end, 10);
  if (end == text.c_str() || value < 0) return 0;
  return static_cast<int>(value);
}

// Runs a command through the shell and returns its trimmed stdout.
// Commands are fixed strings in this file, never user input.
std::string RunCommand(const std::string& command) {
  std::string output;
  FILE* pipe = popen(command.c_str(), "r");
  if (pipe == nullptr) return output;
  char buffer[512];
  while (std::fgets(buffer, sizeof buffer, pipe) != nullptr) output += buffer;
  pclose(pipe);
  return Trim(output);
}

// True if `name` is an existing file in one of the PATH directories.
bool HasCommand(const std::string& name) {
  const char* path = std::getenv("PATH");
  if (path == nullptr) return false;
  std::istringstream dirs(path);
  for (std::string dir; std::getline(dirs, dir, ':');) {
    if (dir.empty()) continue;
    std::error_code ec;
    if (fs::is_regular_file(fs::path(dir) / name, ec)) return true;
  }
  return false;
}

// Value of a "Key: value" line from lscpu output; empty if the key is absent.
std::string LscpuField(const std::string& lscpu, const std::string& key) {
  for (const auto& line : SplitLines(lscpu)) {
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    if (Trim(line.substr(0, colon)) == key) return Trim(line.substr(colon + 1));
  }
  return "";
}

std::string DetectOs() {
  const std::string name = RunCommand("uname -s 2>/dev/null");
  return name.empty() ? "unknown" : name;
}

// ID= from /etc/os-release, without surrounding quotes.
std::string DetectDistro() {
  std::ifstream in("/etc/os-release");
  for (std::string line; std::getline(in, line);) {
    if (line.rfind("ID=", 0) != 0) continue;
    std::string value = Trim(line.substr(3));
    if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'')) {
      value = value.substr(1, value.size() - 2);
    }
    return value.empty() ? "unknown" : value;
  }
  return "unknown";
}

// Sets cpu, cores_physical and threads_logical.
void DetectCpu(Profile& p) {
  const std::string lscpu = RunCommand("lscpu 2>/dev/null");
  p.cpu = LscpuField(lscpu, "Model name");

  const int cores_per_socket = ToInt(LscpuField(lscpu, "Core(s) per socket"));
  const int sockets = ToInt(LscpuField(lscpu, "Socket(s)"));
  p.threads_logical = ToInt(RunCommand("nproc 2>/dev/null"));
  p.cores_physical = cores_per_socket * sockets;
}

// MemTotal from /proc/meminfo, in whole GB (0 when unknown).
int DetectRamGb() {
  std::ifstream in("/proc/meminfo");
  for (std::string line; std::getline(in, line);) {
    if (line.rfind("MemTotal:", 0) != 0) continue;
    const long long kb = std::atoll(line.substr(9).c_str());
    return static_cast<int>(kb / 1024 / 1024);
  }
  return 0;
}

bool DetectAvx2() {
  std::ifstream in("/proc/cpuinfo");
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str().find("avx2") != std::string::npos;
}

// Display controllers from lspci, joined with "; ".
std::string DetectGpu() {
  const std::string lines = RunCommand(
      "lspci 2>/dev/null | grep -iE 'vga|3d|display'");
  std::string joined;
  for (const auto& line : SplitLines(lines)) {
    const auto colon = line.rfind(": ");
    const std::string desc =
        Trim(colon == std::string::npos ? line : line.substr(colon + 2));
    if (desc.empty()) continue;
    if (!joined.empty()) joined += "; ";
    joined += desc;
  }
  return joined;
}

void DetectHardware(Profile& p) {
  p.os = DetectOs();
  p.distro = DetectDistro();
  DetectCpu(p);
  p.ram_gb = DetectRamGb();
  p.avx2 = DetectAvx2();
  p.gpu = DetectGpu();
  p.accel = "cpu";
  if (HasCommand("nvidia-smi")) p.accel = "cuda";
  if (HasCommand("rocminfo")) p.accel = "rocm";
}
#endif

// Same heuristic as examples/hardware-profiler/arch-09-profile.sh.
void Decide(Profile& p) {
  p.threads_batch = p.cores_physical > 0 ? p.cores_physical : 1;
  p.threads_live = p.cores_physical > 2 ? p.cores_physical - 2 : 1;

  if (p.accel != "cpu") {
    // GPU acceleration: turbo is fast enough for live use too.
    p.quality_live = "large-v3-turbo";
    p.quality_batch = "large-v3-turbo";
    p.threads_live = p.threads_batch;
  } else if (p.cores_physical >= 8) {
    p.quality_live = "small";
    p.quality_batch = "large-v3-turbo";
  } else if (p.cores_physical >= 4) {
    p.quality_live = "base";
    p.quality_batch = "small";
  } else {
    p.quality_live = "base";
    p.quality_batch = "base";
  }

  // Low RAM caps both models at base.
  if (p.ram_gb < 3) {
    p.quality_live = "base";
    p.quality_batch = "base";
  }
}

// ISO 8601 timestamp with a UTC marker or local offset.
std::string IsoTimestampNow() {
  const std::time_t now = std::time(nullptr);
#ifdef _WIN32
  // UTC avoids platform-specific time-zone offset formatting.
  std::tm utc{};
  if (gmtime_s(&utc, &now) != 0) return {};
  std::ostringstream out;
  out << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
#else
  const std::tm* local = std::localtime(&now);
  std::ostringstream out;
  out << std::put_time(local, "%Y-%m-%dT%H:%M:%S");
  char zone[16] = {};
  std::strftime(zone, sizeof zone, "%z", local);  // "+0100"
  std::string offset = zone;
  if (offset.size() == 5) offset.insert(3, ":");
  return out.str() + offset;
#endif
}

}  // namespace

Profile DetectAndDecide() {
  Profile p;
  DetectHardware(p);
  if (p.threads_logical < 1) {
    p.threads_logical = static_cast<int>(std::thread::hardware_concurrency());
  }
  if (p.threads_logical < 1) p.threads_logical = 1;
  if (p.cores_physical < 1) p.cores_physical = p.threads_logical;
  Decide(p);
  return p;
}

void WriteProfile(const Profile& p) {
  paths::ensure_layout();

  // Same schema as arch-09-profile.sh; the tuning fields below are fixed
  // defaults that the profiler does not decide yet.
  const json doc = {
      {"schema", 1},
      {"generated", IsoTimestampNow()},
      {"hardware",
       {
           {"os", p.os},
           {"distro", p.distro},
           {"cpu", p.cpu},
           {"cores_physical", p.cores_physical},
           {"threads_logical", p.threads_logical},
           {"ram_gb", p.ram_gb},
           {"avx2", p.avx2},
           {"gpu", p.gpu},
           {"accel", p.accel},
       }},
      {"defaults",
       {
           {"quality_live", p.quality_live},
           {"quality_batch", p.quality_batch},
           {"threads_live", p.threads_live},
           {"threads_batch", p.threads_batch},
           {"window_sec", p.window_sec},
           {"overlap_sec", 5},
           {"storage_mode3", "ram"},
           {"storage_persist", "disk"},
           {"format", "wav"},
           {"backend", p.backend},
           {"diarize", false},
           {"word_timestamps", false},
           {"lang", p.lang},
       }},
  };

  const fs::path path = paths::profile();
  std::ofstream out(path, std::ios::trunc);
  if (!out) throw std::runtime_error("cannot write " + path.string());
  out << doc.dump(2) << "\n";
  if (!out) throw std::runtime_error("failed writing " + path.string());
}

std::optional<Profile> ReadProfile() {
  std::ifstream in(paths::profile());
  if (!in) return std::nullopt;
  try {
    const json doc = json::parse(in);
    const json& hw = doc.at("hardware");
    const json& df = doc.at("defaults");

    Profile p;
    p.os = hw.at("os").get<std::string>();
    p.distro = hw.at("distro").get<std::string>();
    p.cpu = hw.at("cpu").get<std::string>();
    p.cores_physical = hw.at("cores_physical").get<int>();
    p.threads_logical = hw.at("threads_logical").get<int>();
    p.ram_gb = hw.at("ram_gb").get<int>();
    p.avx2 = hw.at("avx2").get<bool>();
    p.gpu = hw.at("gpu").get<std::string>();
    p.accel = hw.at("accel").get<std::string>();

    p.quality_live = df.at("quality_live").get<std::string>();
    p.quality_batch = df.at("quality_batch").get<std::string>();
    p.threads_live = df.at("threads_live").get<int>();
    p.threads_batch = df.at("threads_batch").get<int>();
    p.window_sec = df.at("window_sec").get<int>();
    p.lang = df.at("lang").get<std::string>();
    p.backend = df.at("backend").get<std::string>();
    return p;
  } catch (const std::exception&) {
    return std::nullopt;
  }
}

}  // namespace tinto
