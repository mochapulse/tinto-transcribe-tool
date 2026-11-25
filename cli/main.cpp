// tinto CLI entry point.
//
// Daemon-oriented command surface (see the plan, §9):
//   tinto init | record | status | stop

#include <algorithm>
#include <charconv>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "diagnostics.hpp"
#include "diarize.hpp"
#include "help.hpp"
#include "tinto/config.hpp"
#include "tinto/daemon.hpp"
#include "tinto/models.hpp"
#include "tinto/model_install.hpp"
#include "tinto/paths.hpp"
#include "tinto/pipeline.hpp"
#include "tinto/profile.hpp"
#include "tinto/version.hpp"
#include "tinto/whisper_info.hpp"

namespace {

int CmdInit(bool skip_models) {
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
            << "  GPU / accel : " << (p.gpu.empty() ? "unknown" : p.gpu) << " -> "
            << p.accel << "\n"
            << "  OS          : " << p.os << " / " << p.distro << "\n"
            << "--- chosen defaults ---\n"
            << "  live  : model=" << p.quality_live << " threads=" << p.threads_live
            << "\n"
            << "  batch : model=" << p.quality_batch << " threads=" << p.threads_batch
            << "\n"
            << "  window=" << p.window_sec << "s  lang=" << p.lang
            << "  backend=" << p.backend << "\n";
  if (!skip_models) {
    try {
      tinto::InstallModels(std::cout);
    } catch (const std::exception& error) {
      std::cerr << "init: " << error.what() << "\n";
      return 1;
    }
  }

  return 0;
}

int CmdRecord(const tinto::Config& cfg) {
  if (cfg.diarize && cfg.mode != tinto::Mode::kRecordTranscribe) {
    std::cerr << "record: --diarize requires --mode 2 to keep audio for speaker labeling\n";
    return 2;
  }
#ifndef TINTO_WITH_DIARIZATION
  if (cfg.diarize) {
    std::cerr << "record: this build has no speaker diarization support\n";
    return 1;
  }
#endif
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
    const auto result = tinto::Daemonize(log);
    if (result == tinto::DaemonizeResult::kParent) return 0;
    if (result == tinto::DaemonizeResult::kError) return 1;
  }

  tinto::WorkerControl worker;
  if (!worker.Acquire()) return 1;
  tinto::Pipeline pipeline(cfg, std::move(model_paths));
  if (!pipeline.Start() || !worker.Publish()) return 1;

  pipeline.Run([&worker] { return worker.StopRequested(); },
               [&worker] { return worker.CancelRequested(); });

  std::cout << "record: shutting down\n";
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

  if (!tinto::SendStop(pid)) return 1;

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
    } else if (a == "--diarize" || a == "-D") {
      cfg.diarize = true;
    } else if (a == "--lang" || a == "-l") {
      if (!next(cfg.lang)) return false;
    } else if (a == "--out" || a == "-o") {
      if (!next(cfg.out)) return false;
    } else if (a == "--device" || a == "-d") {
      if (!next(cfg.device)) return false;
    } else if (a == "--source" || a == "-s") {
      std::string value;
      if (!next(value)) return false;
      if (value == "input") cfg.source = tinto::CaptureSelection::kInput;
      else if (value == "system") cfg.source = tinto::CaptureSelection::kSystem;
      else if (value == "both") cfg.source = tinto::CaptureSelection::kBoth;
      else { error = "unknown source: " + value; return false; }
    } else if (a == "--mode" || a == "-m") {
      std::string val;
      if (!next(val)) return false;
      if (val == "record-only" || val == "1") cfg.mode = tinto::Mode::kRecordOnly;
      else if (val == "record+transcribe" || val == "2") cfg.mode = tinto::Mode::kRecordTranscribe;
      else if (val == "transcribe-only" || val == "3") cfg.mode = tinto::Mode::kTranscribeOnly;
      else { error = "unknown mode: " + val; return false; }
    } else if (a == "--quality" || a == "-q") {
      std::string val;
      if (!next(val)) return false;
      if (val == "base") cfg.quality = tinto::Quality::kBase;
      else if (val == "small") cfg.quality = tinto::Quality::kSmall;
      else if (val == "medium") cfg.quality = tinto::Quality::kMedium;
      else if (val == "turbo") cfg.quality = tinto::Quality::kTurbo;
      else { error = "unknown quality: " + val; return false; }
    } else if (a == "--threads" || a == "-t") {
      std::string val;
      if (!next(val)) return false;
      const auto* end = val.data() + val.size();
      const auto parsed = std::from_chars(val.data(), end, cfg.threads);
      if (parsed.ec != std::errc{} || parsed.ptr != end || cfg.threads < 1) {
        error = "threads must be a positive integer";
        return false;
      }
    } else {
      error = "unknown option: " + a;
      return false;
    }
  }
  if (cfg.source == tinto::CaptureSelection::kSystem && !cfg.device.empty()) {
    error = "--device selects an input and cannot be used with --source system";
    return false;
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> args(argv + 1, argv + argc);

  if (args.empty() || args[0] == "--help" || args[0] == "-h") {
    return PrintHelp(std::cout);
  }
  if (args[0] == "--version" || args[0] == "-v" || args[0] == "-V") {
    std::cout << tinto::kVersion << "\n";
    return 0;
  }

  const std::string command = args[0];
  std::vector<std::string> rest(args.begin() + 1, args.end());
  if (std::any_of(rest.begin(), rest.end(), [](const auto& arg) {
        return arg == "-h" || arg == "--help";
      })) {
    return PrintHelp(std::cout, command);
  }
  if (command == "help" && rest.size() <= 1) {
    return rest.empty() ? PrintHelp(std::cout) : PrintHelp(std::cout, rest[0]);
  }
  if (command == "init") {
    if (rest.empty()) return CmdInit(false);
    if (rest.size() == 1 && (rest[0] == "--skip-models" || rest[0] == "-n")) {
      return CmdInit(true);
    }
  } else if (command == "record") {
    tinto::Config cfg = tinto::DefaultConfig();
    if (auto profile = tinto::ReadProfile(); profile.has_value()) {
      tinto::MergeProfile(cfg, *profile);
    }
    std::string error;
    if (!ApplyFlags(cfg, rest, error)) {
      std::cerr << "record: " << error << "\n";
      return 2;
    }
    return CmdRecord(cfg);
  } else if (command == "diarize") {
    return CmdDiarize(rest);
  } else if (command == "status" || command == "stop" ||
             command == "health" || command == "doctor") {
    if (rest.empty()) {
      if (command == "status") return CmdStatus();
      if (command == "stop") return CmdStop();
      return CmdDiagnostics(command == "doctor");
    }
  } else {
    return PrintHelp(std::cerr, command);
  }

  std::cerr << command << ": unexpected argument; see 'tinto help " << command << "'\n";
  return 2;
}
