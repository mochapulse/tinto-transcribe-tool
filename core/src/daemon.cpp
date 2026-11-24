#include "tinto/daemon.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <sys/types.h>
#include <system_error>
#include <unistd.h>

#include "tinto/paths.hpp"

namespace tinto {

namespace {

std::atomic<bool> g_stop_requested{false};

void OnStopSignal(int /*signum*/) { g_stop_requested.store(true); }

std::filesystem::path PidFilePath() { return paths::state_dir() / "tinto.pid"; }

}  // namespace

bool Daemonize(const std::filesystem::path& log_path) {
  // Flush so buffered output is not duplicated in the child.
  std::cout.flush();
  std::cerr.flush();

  const pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "daemon: fork failed: " << std::strerror(errno) << "\n";
    return false;
  }
  if (pid > 0) {
    // Parent: report the daemon PID and let the caller exit.
    std::cout << "record: worker started (PID " << pid << ")\n";
    return false;
  }

  // Child: detach from the controlling terminal.
  if (setsid() < 0) {
    std::cerr << "daemon: setsid failed: " << std::strerror(errno) << "\n";
    return false;
  }

  const int log_fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (log_fd < 0) {
    std::cerr << "daemon: cannot open log " << log_path << ": "
              << std::strerror(errno) << "\n";
    return false;
  }
  const int null_fd = open("/dev/null", O_RDONLY);
  if (null_fd < 0) {
    std::cerr << "daemon: cannot open /dev/null: " << std::strerror(errno) << "\n";
    return false;
  }

  dup2(null_fd, STDIN_FILENO);
  dup2(log_fd, STDOUT_FILENO);
  dup2(log_fd, STDERR_FILENO);
  close(null_fd);
  close(log_fd);
  return true;
}

bool WritePidFile() {
  std::error_code ec;
  std::filesystem::create_directories(paths::state_dir(), ec);

  std::ofstream out(PidFilePath(), std::ios::trunc);
  if (!out) return false;
  out << getpid() << "\n";
  return static_cast<bool>(out);
}

std::int32_t ReadPid() {
  std::ifstream in(PidFilePath());
  if (!in) return 0;

  long long value = 0;
  if (!(in >> value)) return 0;
  if (value <= 0 || value > INT32_MAX) return 0;
  return static_cast<std::int32_t>(value);
}

void RemovePidFile() {
  std::error_code ec;
  std::filesystem::remove(PidFilePath(), ec);
}

bool IsProcessAlive(std::int32_t pid) {
  // pid <= 0 would address process groups; never treat it as a live worker.
  if (pid <= 0) return false;
  if (kill(pid, 0) == 0) return true;
  return errno == EPERM;
}

bool SendStop(std::int32_t pid) {
  if (pid <= 0) return false;
  return kill(pid, SIGTERM) == 0;
}

std::atomic<bool>& InstallSignalHandlers() {
  struct sigaction sa {};
  sa.sa_handler = OnStopSignal;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction(SIGTERM, &sa, nullptr);
  sigaction(SIGINT, &sa, nullptr);
  return g_stop_requested;
}

}  // namespace tinto
