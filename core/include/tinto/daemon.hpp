#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>

namespace tinto {

enum class DaemonizeResult { kChild, kParent, kError };

// Detach the worker from the terminal. On failure print an error and return kError.
// `log_path` receives stdout+stderr of the daemon.
DaemonizeResult Daemonize(const std::filesystem::path& log_path);

// Own the state for one recording process. Destruction unpublishes the worker
// and releases the instance lock, including on startup failure.
class WorkerControl {
 public:
  WorkerControl();
  ~WorkerControl();
  WorkerControl(const WorkerControl&) = delete;
  WorkerControl& operator=(const WorkerControl&) = delete;

  bool Acquire();
  bool Publish();
  bool StopRequested() const;
  bool CancelRequested() const;

 private:
  struct State;
  std::unique_ptr<State> state_;
};

// Read the PID from the PID file. Returns 0 if missing or malformed.
std::int32_t ReadPid();

// Check if the published worker with this PID is active.
bool IsProcessAlive(std::int32_t pid);

// Request graceful shutdown. Prints an error and returns false on failure.
bool SendStop(std::int32_t pid);

}  // namespace tinto
