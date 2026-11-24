#pragma once

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>

namespace tinto {

// Daemonize the current process (fork + setsid + redirect stdio to log).
// Returns true in the child (now a daemon), false in the parent.
// On failure, prints to stderr and returns false.
// `log_path` receives stdout+stderr of the daemon.
bool Daemonize(const std::filesystem::path& log_path);

// Write PID to ~/.tinto/state/tinto.pid. Returns true on success.
bool WritePidFile();

// Read the PID from the PID file. Returns 0 if missing or malformed.
std::int32_t ReadPid();

// Remove the PID file. Idempotent.
void RemovePidFile();

// Check if a process with the given PID is alive.
bool IsProcessAlive(std::int32_t pid);

// Send SIGTERM to the given PID. Returns true if the signal was sent.
bool SendStop(std::int32_t pid);

// Install signal handlers (SIGTERM, SIGINT) that set a stop flag.
// Returns a reference to the atomic flag; when it becomes true, the
// caller should shut down gracefully.
std::atomic<bool>& InstallSignalHandlers();

}  // namespace tinto
