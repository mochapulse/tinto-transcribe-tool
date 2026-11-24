// tinto CLI entry point.
//
// Daemon-oriented command surface (see the plan, §9):
//   tinto init | record | status | stop

#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "tinto/config.hpp"
#include "tinto/daemon.hpp"
#include "tinto/models.hpp"
#include "tinto/paths.hpp"
#include "tinto/pipeline.hpp"
#include "tinto/profile.hpp"
#include "tinto/version.hpp"
#include "tinto/whisper_info.hpp"

namespace {

int Usage(std::ostream& os) {
  os << "tinto " << tinto::kVersion << " — passive meeting recorder & transcriber\n"
     << "\n"
     << "Usage: tinto <command> [options]\n"
     << "\n"
     << "Commands:\n"
     << "  init      Profile this machine and write ~/.tinto/profile.json\n"
     << "  record    Start recording/transcribing in the background\n"
     << "  status    Show what the background worker is doing\n"
     << "  stop      Finalize outputs and stop the worker\n"
     << "\n"
     << "  --version Print version and exit\n"
     << "  --help    Print this help and exit\n";
  return 0;
}

int CmdInit() {
  tinto::paths::ensure_layout();
  const tinto::Profile p = tinto::DetectAndDecide();
  try {
    tinto::WriteProfile(p);
  } catch (const std::exception& e) {
    std::cerr << "init: " << e.what() << "\n";
    return 1;
  }

  std::cout << "=== Hardware profile written to " << tinto::paths::profile().string()
            << " ===\n"
            << "  CPU         : " << (p.cpu.empty() ? "unknown" : p.cpu) << " ("
            << p.cores_physical << "c / " << p.threads_logical
            << "t, AVX2=" << (p.avx2 ? "true" : "false") << ")\n"
            << "  RAM         : " << p.ram_gb << " GB\n"
            << "  GPU / accel : " << (p.gpu.empty() ? "none" : p.gpu) << " -> "
            << p.accel << "\n"
            << "  OS          : " << p.os << " / " << p.distro << "\n"
            << "--- chosen defaults ---\n"
            << "  live  : model=" << p.quality_live << " threads=" << p.threads_live
            << "\n"
            << "  batch : model=" << p.quality_batch << " threads=" << p.threads_batch
            << "\n"
            << "  window=" << p.window_sec << "s  lang=" << p.lang
            << "  backend=" << p.backend << "\n";
  return 0;
}

int CmdRecord(const tinto::Config& cfg) {
  tinto::paths::ensure_layout();

  const auto existing_pid = tinto::ReadPid();
  if (existing_pid > 0 && tinto::IsProcessAlive(existing_pid)) {
    std::cerr << "record: worker already running (PID " << existing_pid << ")\n";
    return 1;
  }

  tinto::Pipeline::ModelPaths model_paths;
  const bool needs_transcription =
      (cfg.mode == tinto::Mode::kRecordTranscribe ||
       cfg.mode == tinto::Mode::kTranscribeOnly);

  if (needs_transcription) {
    model_paths.asr_model = tinto::FindAsrModel(cfg.quality);
    model_paths.vad_model = tinto::FindVadModel();
    if (model_paths.asr_model.empty()) {
      std::cerr << "record: no ASR model found for quality="
                << tinto::ToString(cfg.quality)
                << " in " << tinto::paths::models_dir().string() << "\n";
      return 1;
    }
    if (model_paths.vad_model.empty()) {
      std::cerr << "record: no VAD model found in "
                << tinto::paths::models_dir().string() << "\n";
      return 1;
    }
  }

  if (!cfg.foreground) {
    const auto log = tinto::paths::logs_dir() / "tinto.log";
    if (!tinto::Daemonize(log)) {
      return 0;
    }
  }

  if (!tinto::WritePidFile()) {
    std::cerr << "record: failed to write PID file\n";
    return 1;
  }

  auto& stop_flag = tinto::InstallSignalHandlers();

  tinto::Pipeline pipeline(cfg, std::move(model_paths));
  if (!pipeline.Start()) {
    tinto::RemovePidFile();
    return 1;
  }

  pipeline.Run(stop_flag);
  pipeline.Finish();

  std::cout << "record: shutting down\n";
  tinto::RemovePidFile();
  return 0;
}

int CmdStatus() {
  const auto pid = tinto::ReadPid();
  const bool whisper = !tinto::WhisperBuildInfo().empty();

  if (pid <= 0 || !tinto::IsProcessAlive(pid)) {
    std::cout << "status: no worker running\n"
              << "  transcription backend: "
              << (whisper ? "whisper (available)" : "none (built without whisper)")
              << "\n";
    return 1;
  }

  std::cout << "status: worker running (PID " << pid << ")\n"
            << "  transcription backend: "
            << (whisper ? "whisper (available)" : "none (built without whisper)")
            << "\n";
  return 0;
}

int CmdStop() {
  const auto pid = tinto::ReadPid();
  if (pid <= 0 || !tinto::IsProcessAlive(pid)) {
    std::cout << "stop: no worker running\n";
    return 1;
  }

  if (!tinto::SendStop(pid)) {
    std::cerr << "stop: failed to send stop signal to PID " << pid << "\n";
    return 1;
  }

  std::cout << "stop: sent shutdown signal to PID " << pid << "\n";
  return 0;
}

bool ApplyFlags(tinto::Config& cfg, const std::vector<std::string>& args,
                std::string& error) {
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto next = [&](std::string& dst) -> bool {
      if (i + 1 >= args.size()) { error = "missing value for " + a; return false; }
      dst = args[++i];
      return true;
    };
    if (a == "-f" || a == "--foreground") {
      cfg.foreground = true;
    } else if (a == "--lang") {
      if (!next(cfg.lang)) return false;
    } else if (a == "--out") {
      if (!next(cfg.out)) return false;
    } else if (a == "--device") {
      if (!next(cfg.device)) return false;
    } else if (a == "--mode") {
      std::string val;
      if (!next(val)) return false;
      if (val == "record-only" || val == "1") cfg.mode = tinto::Mode::kRecordOnly;
      else if (val == "record+transcribe" || val == "2") cfg.mode = tinto::Mode::kRecordTranscribe;
      else if (val == "transcribe-only" || val == "3") cfg.mode = tinto::Mode::kTranscribeOnly;
      else { error = "unknown mode: " + val; return false; }
    } else if (a == "--quality") {
      std::string val;
      if (!next(val)) return false;
      if (val == "base") cfg.quality = tinto::Quality::kBase;
      else if (val == "small") cfg.quality = tinto::Quality::kSmall;
      else if (val == "medium") cfg.quality = tinto::Quality::kMedium;
      else if (val == "turbo") cfg.quality = tinto::Quality::kTurbo;
      else { error = "unknown quality: " + val; return false; }
    } else if (a == "--threads") {
      std::string val;
      if (!next(val)) return false;
      cfg.threads = std::atoi(val.c_str());
      if (cfg.threads < 1) { error = "threads must be >= 1"; return false; }
    } else {
      error = "unknown option: " + a;
      return false;
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);

  if (args.empty() || args[0] == "--help" || args[0] == "-h") {
    return Usage(std::cout);
  }
  if (args[0] == "--version" || args[0] == "-V") {
    std::cout << tinto::kVersion << "\n";
    return 0;
  }

  const std::string command = args[0];
  std::vector<std::string> rest(args.begin() + 1, args.end());

  tinto::Config cfg = tinto::DefaultConfig();
  if (auto profile = tinto::ReadProfile(); profile.has_value()) {
    tinto::MergeProfile(cfg, *profile);
  }
  std::string error;
  if (!ApplyFlags(cfg, rest, error)) {
    std::cerr << "tinto: " << error << "\n";
    return 2;
  }

  if (command == "init") return CmdInit();
  if (command == "record") return CmdRecord(cfg);
  if (command == "status") return CmdStatus();
  if (command == "stop") return CmdStop();

  std::cerr << "tinto: unknown command '" << command << "'\n";
  Usage(std::cerr);
  return 2;
}
