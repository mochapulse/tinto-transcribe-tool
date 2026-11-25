#include "tinto/daemon.hpp"

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <vector>
#else
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#include "tinto/paths.hpp"

namespace tinto {

namespace {

std::atomic<unsigned> g_stop_requests{0};

#ifdef _WIN32
BOOL WINAPI OnConsoleControl(DWORD event) {
  if (event != CTRL_C_EVENT && event != CTRL_BREAK_EVENT) return FALSE;
  const auto previous = g_stop_requests.fetch_add(1, std::memory_order_relaxed);
  if (previous < 2) {
    // Acknowledge Ctrl+C on the console handler thread, even while Whisper is
    // inside an encoder step and cannot yet return to the pipeline.
    constexpr char stop[] = "record: stop requested; press Ctrl+C again to skip pending transcription\r\n";
    constexpr char cancel[] = "record: cancelling pending transcription\r\n";
    const HANDLE output = GetStdHandle(STD_ERROR_HANDLE);
    DWORD written = 0;
    if (output != nullptr && output != INVALID_HANDLE_VALUE) {
      WriteFile(output, previous == 0 ? stop : cancel,
                previous == 0 ? sizeof(stop) - 1 : sizeof(cancel) - 1, &written, nullptr);
    }
  }
  return TRUE;
}

unsigned long CurrentPid() { return GetCurrentProcessId(); }

class Handle {
 public:
  explicit Handle(HANDLE value = nullptr) : value_(value) {}
  ~Handle() { Reset(); }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  HANDLE Get() const { return value_; }
  explicit operator bool() const {
    return value_ != nullptr && value_ != INVALID_HANDLE_VALUE;
  }
  void Reset(HANDLE next = nullptr) {
    if (*this) CloseHandle(value_);
    value_ = next;
  }

 private:
  HANDLE value_;
};

std::wstring ControlPipe(unsigned long pid) {
  std::error_code error;
  auto directory = std::filesystem::weakly_canonical(paths::state_dir(), error);
  if (error) directory = paths::state_dir().lexically_normal();
  auto name = directory.native();
  if (!name.empty()) CharLowerBuffW(name.data(), static_cast<DWORD>(name.size()));
  std::uint64_t hash = 14695981039346656037ull;
  for (auto code_unit : name) {
    hash ^= code_unit;
    hash *= 1099511628211ull;
  }
  return L"\\\\.\\pipe\\tinto-" + std::to_wstring(hash) + L"-" +
         std::to_wstring(pid);
}

class HandleList {
 public:
  ~HandleList() {
    if (attributes != nullptr) DeleteProcThreadAttributeList(attributes);
  }

  bool Init(HANDLE (&handles)[2]) {
    SIZE_T size = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &size);
    if (size == 0) return false;
    buffer_.resize(size);
    auto* list = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(buffer_.data());
    if (!InitializeProcThreadAttributeList(list, 1, 0, &size)) return false;
    attributes = list;
    return UpdateProcThreadAttribute(attributes, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                     handles, sizeof handles, nullptr, nullptr) != 0;
  }

  LPPROC_THREAD_ATTRIBUTE_LIST attributes = nullptr;

 private:
  std::vector<unsigned char> buffer_;
};
#else
void OnStopSignal(int /*signum*/) { g_stop_requests.fetch_add(1, std::memory_order_relaxed); }

pid_t CurrentPid() { return getpid(); }
#endif

std::filesystem::path PidFilePath() { return paths::state_dir() / "tinto.pid"; }

}  // namespace

struct WorkerControl::State {
  bool published = false;
#ifdef _WIN32
  Handle lock;
  Handle connected;
  Handle pipe;
  OVERLAPPED connection{};

  ~State() {
    if (pipe && connection.hEvent != nullptr) {
      CancelIoEx(pipe.Get(), &connection);
      DWORD unused = 0;
      GetOverlappedResult(pipe.Get(), &connection, &unused, TRUE);
    }
  }
#else
  int lock = -1;
  ~State() {
    if (lock >= 0) close(lock);
  }
#endif
};

DaemonizeResult Daemonize(const std::filesystem::path& log_path) {
#ifdef _WIN32
  SECURITY_ATTRIBUTES inherit{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
  Handle log(CreateFileW(log_path.c_str(), FILE_APPEND_DATA,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit,
                         OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  Handle input(CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                           &inherit, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  HANDLE inherited[] = {log.Get(), input.Get()};
  HandleList handles;
  if (!log || !input || !handles.Init(inherited)) {
    std::cerr << "record: cannot set up background logging\n";
    return DaemonizeResult::kError;
  }

  std::wstring executable(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, executable.data(),
                                          static_cast<DWORD>(executable.size()));
  if (length == 0 || length >= executable.size()) {
    std::cerr << "record: cannot locate the current executable\n";
    return DaemonizeResult::kError;
  }
  executable.resize(length);
  std::wstring command = GetCommandLineW();
  command += L" --foreground";

  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof startup;
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = input.Get();
  startup.StartupInfo.hStdOutput = log.Get();
  startup.StartupInfo.hStdError = log.Get();
  startup.lpAttributeList = handles.attributes;
  PROCESS_INFORMATION info{};
  if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
                      DETACHED_PROCESS | EXTENDED_STARTUPINFO_PRESENT, nullptr,
                      nullptr, &startup.StartupInfo, &info)) {
    std::cerr << "record: cannot launch the worker (Windows error "
              << GetLastError() << ")\n";
    return DaemonizeResult::kError;
  }
  Handle process(info.hProcess);
  Handle thread(info.hThread);
  constexpr DWORD kStartupWaitMs = 30000;
  const auto deadline = GetTickCount64() + kStartupWaitMs;
  do {
    if (WaitForSingleObject(process.Get(), 50) != WAIT_TIMEOUT) {
      std::cerr << "record: worker exited during startup; see " << log_path << "\n";
      return DaemonizeResult::kError;
    }
    if (ReadPid() == static_cast<std::int32_t>(info.dwProcessId)) {
      std::cout << "record: worker started (PID " << info.dwProcessId << ")\n";
      return DaemonizeResult::kParent;
    }
  } while (GetTickCount64() < deadline);
  std::cout << "record: worker still starting (PID " << info.dwProcessId
            << "); see " << log_path << "\n";
  return DaemonizeResult::kParent;
#else
  // Flush so buffered output is not duplicated in the child.
  std::cout.flush();
  std::cerr.flush();

  const pid_t pid = fork();
  if (pid < 0) {
    std::cerr << "daemon: fork failed: " << std::strerror(errno) << "\n";
    return DaemonizeResult::kError;
  }
  if (pid > 0) {
    // Parent: report the daemon PID and let the caller exit.
    std::cout << "record: worker started (PID " << pid << ")\n";
    return DaemonizeResult::kParent;
  }

  // Child: detach from the controlling terminal.
  if (setsid() < 0) {
    std::cerr << "daemon: setsid failed: " << std::strerror(errno) << "\n";
    return DaemonizeResult::kError;
  }

  const int log_fd = open(log_path.c_str(), O_WRONLY | O_CREAT | O_APPEND, 0644);
  if (log_fd < 0) {
    std::cerr << "daemon: cannot open log " << log_path << ": "
              << std::strerror(errno) << "\n";
    return DaemonizeResult::kError;
  }
  const int null_fd = open("/dev/null", O_RDONLY);
  if (null_fd < 0) {
    std::cerr << "daemon: cannot open /dev/null: " << std::strerror(errno) << "\n";
    close(log_fd);
    return DaemonizeResult::kError;
  }

  dup2(null_fd, STDIN_FILENO);
  dup2(log_fd, STDOUT_FILENO);
  dup2(log_fd, STDERR_FILENO);
  close(null_fd);
  close(log_fd);
  return DaemonizeResult::kChild;
#endif
}

WorkerControl::WorkerControl() = default;

WorkerControl::~WorkerControl() {
  if (state_ && state_->published) {
    std::error_code error;
    std::filesystem::remove(PidFilePath(), error);
  }
}

bool WorkerControl::Acquire() {
  std::error_code ec;
  std::filesystem::create_directories(paths::state_dir(), ec);
  const auto lock_path = paths::state_dir() / "worker.lock";
  state_ = std::make_unique<State>();
#ifdef _WIN32
  state_->lock.Reset(CreateFileW(lock_path.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                                 nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  const bool locked = static_cast<bool>(state_->lock);
#else
  state_->lock = open(lock_path.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
  const bool locked = state_->lock >= 0 && flock(state_->lock, LOCK_EX | LOCK_NB) == 0;
#endif
  if (!locked) {
    std::cerr << "record: cannot acquire the instance lock; another worker may be starting\n";
    return false;
  }
#ifdef _WIN32
  const bool handlers = SetConsoleCtrlHandler(OnConsoleControl, TRUE) != 0;
#else
  struct sigaction sa {};
  sa.sa_handler = OnStopSignal;
  sigemptyset(&sa.sa_mask);
  const bool handlers = sigaction(SIGTERM, &sa, nullptr) == 0 &&
                        sigaction(SIGINT, &sa, nullptr) == 0;
#endif
  if (!handlers) std::cerr << "record: failed to install shutdown handlers\n";
  return handlers;
}

bool WorkerControl::Publish() {
#ifdef _WIN32
  const auto name = ControlPipe(CurrentPid());
  state_->connected.Reset(CreateEventW(nullptr, TRUE, FALSE, nullptr));
  state_->pipe.Reset(CreateNamedPipeW(
      name.c_str(), PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED | FILE_FLAG_FIRST_PIPE_INSTANCE,
      PIPE_TYPE_BYTE | PIPE_WAIT | PIPE_REJECT_REMOTE_CLIENTS, 1, 0, 0, 50, nullptr));
  if (!state_->connected || !state_->pipe) {
    std::cerr << "record: cannot create the shutdown pipe\n";
    return false;
  }
  state_->connection.hEvent = state_->connected.Get();
  if (ConnectNamedPipe(state_->pipe.Get(), &state_->connection)) {
    SetEvent(state_->connected.Get());
  } else {
    const DWORD error = GetLastError();
    if (error == ERROR_PIPE_CONNECTED) {
      SetEvent(state_->connected.Get());
    } else if (error != ERROR_IO_PENDING) {
      std::cerr << "record: cannot listen for shutdown requests\n";
      return false;
    }
  }
#endif
  std::ofstream out(PidFilePath(), std::ios::trunc);
  if (out) {
    state_->published = true;
    out << CurrentPid() << "\n";
    out.close();
  }
  if (!out) std::cerr << "record: failed to write PID file\n";
  return static_cast<bool>(out);
}

bool WorkerControl::StopRequested() const {
  if (g_stop_requests.load(std::memory_order_relaxed) != 0) return true;
#ifdef _WIN32
  // The local pipe has one purpose: a successful connection requests shutdown.
  return WaitForSingleObject(state_->connected.Get(), 0) == WAIT_OBJECT_0;
#else
  return false;
#endif
}

bool WorkerControl::CancelRequested() const {
  return g_stop_requests.load(std::memory_order_relaxed) >= 2;
}

std::int32_t ReadPid() {
  std::ifstream in(PidFilePath());
  if (!in) return 0;

  long long value = 0;
  if (!(in >> value)) return 0;
  if (value <= 0 || value > INT32_MAX) return 0;
  return static_cast<std::int32_t>(value);
}

bool IsProcessAlive(std::int32_t pid) {
  if (pid <= 0) return false;
#ifdef _WIN32
  const auto pipe = ControlPipe(static_cast<DWORD>(pid));
  // A busy pipe is still the published worker, already processing shutdown.
  return WaitNamedPipeW(pipe.c_str(), NMPWAIT_NOWAIT) ||
         GetLastError() == ERROR_SEM_TIMEOUT;
#else
  if (kill(pid, 0) == 0) return true;
  return errno == EPERM;
#endif
}

bool SendStop(std::int32_t pid) {
#ifdef _WIN32
  if (pid <= 0) return false;
  Handle process(OpenProcess(SYNCHRONIZE, FALSE, static_cast<DWORD>(pid)));
  if (!process) {
    std::cerr << "stop: cannot open the worker process\n";
    return false;
  }
  const auto pipe = ControlPipe(static_cast<DWORD>(pid));
  Handle request(CreateFileW(pipe.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                             SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION, nullptr));
  if (!request && GetLastError() != ERROR_PIPE_BUSY) {
    std::cerr << "stop: cannot connect to the worker shutdown pipe\n";
    return false;
  }
  request.Reset();
  if (WaitForSingleObject(process.Get(), 30000) == WAIT_OBJECT_0) return true;
  std::cerr << "stop: worker did not finish within 30 seconds; check status or the log\n";
#else
  if (pid > 0 && kill(pid, SIGTERM) == 0) return true;
  std::cerr << "stop: failed to send stop signal to PID " << pid << "\n";
#endif
  return false;
}

}  // namespace tinto
