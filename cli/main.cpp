// tinto CLI entry point.
//
// Daemon-oriented command surface (see the plan, §9):
//   tinto init | record | status | stop
//
// This is the scaffold: command dispatch, config/precedence wiring, and the
// ~/.tinto layout are in place; each command's real work is filled in by its
// feature module (capture, gate, transcriber, profiler, daemon).

#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#include "tinto/config.hpp"
#include "tinto/paths.hpp"
#include "tinto/version.hpp"

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

// --- command stubs (filled in by feature modules) ---

int CmdInit() {
  tinto::paths::ensure_layout();
  std::cout << "init: ~/.tinto layout ready at " << tinto::paths::home() << "\n"
            << "init: hardware profiling not implemented yet (feat/init-profile)\n";
  return 0;
}

int CmdRecord(const tinto::Config& cfg) {
  tinto::paths::ensure_layout();
  std::cout << "record: mode=" << tinto::ToString(cfg.mode)
            << " quality=" << tinto::ToString(cfg.quality)
            << " lang=" << cfg.lang << "\n"
            << "record: pipeline not implemented yet "
               "(feat/capture, feat/gate, feat/transcribe, feat/daemon)\n";
  return 0;
}

int CmdStatus() {
  std::cout << "status: no worker running (control plane not implemented yet, "
               "feat/daemon)\n";
  return 1;  // non-zero when nothing is running (plan, §9)
}

int CmdStop() {
  std::cout << "stop: nothing to stop (control plane not implemented yet, "
               "feat/daemon)\n";
  return 1;
}

// Minimal flag parsing for the scaffold. Full surface lands with the feature
// modules; unknown flags are reported, not silently ignored.
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
  // TODO(feat/init-profile): merge ~/.tinto/profile.json here (below flags).
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
