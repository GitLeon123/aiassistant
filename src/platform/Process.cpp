#include <windows.h>

#include "platform/Process.h"

#include <algorithm>
#include <atomic>
#include <thread>

#include "core/Log.h"

namespace aia {
namespace {

struct OwnedHandle {
  HANDLE h = nullptr;
  OwnedHandle() = default;
  explicit OwnedHandle(HANDLE handle) : h(handle) {}
  OwnedHandle(const OwnedHandle&) = delete;
  OwnedHandle& operator=(const OwnedHandle&) = delete;
  ~OwnedHandle() { Reset(); }
  void Reset() {
    if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
    h = nullptr;
  }
};

void CloseOwned(HANDLE& h) {
  if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
  h = nullptr;
}

bool EndsWithI(const std::wstring& s, const wchar_t* suffix) {
  size_t n = wcslen(suffix);
  return s.size() >= n && _wcsicmp(s.c_str() + s.size() - n, suffix) == 0;
}

bool FileExists(const std::wstring& path) {
  DWORD attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

// The inherited environment with overrides applied, as a CREATE_UNICODE_ENVIRONMENT block.
std::wstring BuildEnvironment(const std::vector<std::pair<std::wstring, std::wstring>>& overrides) {
  std::wstring block;
  if (wchar_t* env = GetEnvironmentStringsW()) {
    for (const wchar_t* p = env; *p; p += wcslen(p) + 1) {
      std::wstring_view entry(p);
      size_t eq = entry.find(L'=', 1);  // Skip a leading '=' (per-drive entries such as "=C:=C:\")
      bool overridden = false;
      for (const auto& [name, value] : overrides) {
        if (eq == name.size() && _wcsnicmp(p, name.c_str(), name.size()) == 0) overridden = true;
      }
      if (!overridden) block.append(entry).push_back(L'\0');
    }
    FreeEnvironmentStringsW(env);
  }
  for (const auto& [name, value] : overrides) block.append(name + L"=" + value).push_back(L'\0');
  block.push_back(L'\0');
  return block;
}

}  // namespace

std::wstring QuoteArg(const std::wstring& arg) {
  if (!arg.empty() && arg.find_first_of(L" \t\n\v\"") == std::wstring::npos) return arg;
  std::wstring out = L"\"";
  for (size_t i = 0;; ++i) {
    size_t backslashes = 0;
    while (i < arg.size() && arg[i] == L'\\') {
      ++i;
      ++backslashes;
    }
    if (i == arg.size()) {
      out.append(backslashes * 2, L'\\');
      break;
    }
    if (arg[i] == L'"') {
      out.append(backslashes * 2 + 1, L'\\');
      out.push_back(L'"');
    } else {
      out.append(backslashes, L'\\');
      out.push_back(arg[i]);
    }
  }
  out.push_back(L'"');
  return out;
}

std::wstring FindExecutable(const std::wstring& name, const std::vector<std::wstring>& extraDirs) {
  static const wchar_t* kExts[] = {L".exe", L".cmd", L".bat"};
  auto tryDir = [&](const std::wstring& dir) -> std::wstring {
    if (dir.empty()) return {};
    std::wstring base = dir;
    if (base.back() != L'\\' && base.back() != L'/') base.push_back(L'\\');
    for (const wchar_t* ext : kExts) {
      std::wstring candidate = base + name + ext;
      if (FileExists(candidate)) return candidate;
    }
    return {};
  };

  DWORD n = GetEnvironmentVariableW(L"PATH", nullptr, 0);
  std::wstring path(n, L'\0');
  if (n) path.resize(GetEnvironmentVariableW(L"PATH", path.data(), n));
  size_t start = 0;
  while (start <= path.size()) {
    size_t end = path.find(L';', start);
    if (end == std::wstring::npos) end = path.size();
    std::wstring dir = path.substr(start, end - start);
    if (!dir.empty() && dir.front() == L'"' && dir.back() == L'"' && dir.size() >= 2) {
      dir = dir.substr(1, dir.size() - 2);
    }
    if (auto found = tryDir(dir); !found.empty()) return found;
    start = end + 1;
  }
  for (const auto& dir : extraDirs) {
    wchar_t expanded[MAX_PATH * 2];
    ExpandEnvironmentStringsW(dir.c_str(), expanded, static_cast<DWORD>(std::size(expanded)));
    if (auto found = tryDir(expanded); !found.empty()) return found;
  }
  return {};
}

// ---------------------------------------------------------------------------

ChildProcess::~ChildProcess() {
  Terminate();
  CloseOwned(stdinWrite_);
  CloseOwned(stdoutRead_);
  if (errReader_.joinable()) errReader_.join();
  CloseOwned(stderrRead_);
  CloseOwned(process_);
  CloseOwned(job_);
}

void ChildProcess::Terminate() {
  if (!job_ || !Running()) return;
  TerminateJobObject(job_, 1);
  WaitForSingleObject(process_, 2000);  // Let it release its working directory before callers clean up
}

bool ChildProcess::Running() const { return process_ && WaitForSingleObject(process_, 0) == WAIT_TIMEOUT; }

bool ChildProcess::Start(const ProcessOptions& options) {
  timeoutMs_ = options.timeoutMs;
  SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
  // 64 KB pipes so a child that writes before we read, or a large stdin (screenshots), doesn't stall.
  constexpr DWORD kPipeSize = 1 << 16;
  OwnedHandle stdinRead, stdoutWrite, stderrWrite;
  if (!CreatePipe(&stdinRead.h, &stdinWrite_, &sa, kPipeSize) ||
      !CreatePipe(&stdoutRead_, &stdoutWrite.h, &sa, kPipeSize) ||
      !CreatePipe(&stderrRead_, &stderrWrite.h, &sa, kPipeSize)) {
    startError_ = "Failed to create pipes";
    return false;
  }
  SetHandleInformation(stdinWrite_, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stdoutRead_, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stderrRead_, HANDLE_FLAG_INHERIT, 0);

  std::wstring cmdline;
  bool isBatch = EndsWithI(options.exe, L".cmd") || EndsWithI(options.exe, L".bat");
  if (isBatch) {
    // cmd.exe re-parses the line; args are validated by callers to contain no metacharacters.
    std::wstring inner = QuoteArg(options.exe);
    for (const auto& a : options.args) inner += L" " + QuoteArg(a);
    wchar_t comspec[MAX_PATH];
    if (!GetEnvironmentVariableW(L"ComSpec", comspec, MAX_PATH)) wcscpy_s(comspec, L"cmd.exe");
    cmdline = QuoteArg(comspec) + L" /d /s /c \"" + inner + L"\"";
  } else {
    cmdline = QuoteArg(options.exe);
    for (const auto& a : options.args) cmdline += L" " + QuoteArg(a);
  }
  std::wstring environment;
  if (!options.env.empty()) environment = BuildEnvironment(options.env);

  // Only the three pipe ends we intend to share are inherited.
  HANDLE inherit[] = {stdinRead.h, stdoutWrite.h, stderrWrite.h};
  SIZE_T attrSize = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
  std::vector<char> attrBuf(attrSize);
  auto attrs = reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attrBuf.data());
  InitializeProcThreadAttributeList(attrs, 1, 0, &attrSize);
  UpdateProcThreadAttribute(attrs, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, sizeof(inherit), nullptr, nullptr);

  STARTUPINFOEXW si{};
  si.StartupInfo.cb = sizeof(si);
  si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  si.StartupInfo.hStdInput = stdinRead.h;
  si.StartupInfo.hStdOutput = stdoutWrite.h;
  si.StartupInfo.hStdError = stderrWrite.h;
  si.lpAttributeList = attrs;

  job_ = CreateJobObjectW(nullptr, nullptr);
  JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
  limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
  SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits, sizeof(limits));

  PROCESS_INFORMATION pi{};
  BOOL ok = CreateProcessW(nullptr, cmdline.data(), nullptr, nullptr, TRUE,
                           CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT |
                               CREATE_UNICODE_ENVIRONMENT,
                           environment.empty() ? nullptr : environment.data(),
                           options.cwd.empty() ? nullptr : options.cwd.c_str(), &si.StartupInfo, &pi);
  DeleteProcThreadAttributeList(attrs);
  if (!ok) {
    startError_ = "Failed to start " + Narrow(options.exe) + " (error " + std::to_string(GetLastError()) + ")";
    return false;
  }
  process_ = pi.hProcess;
  OwnedHandle thread{pi.hThread};
  AssignProcessToJobObject(job_, process_);
  ResumeThread(thread.h);
  // Our copies of the child's pipe ends close when this scope ends, so reads see EOF when it exits.

  // Drain stderr from the start: a process started ahead of time may log while it waits for input.
  errReader_ = std::thread([this] {
    char buf[4096];
    DWORD n = 0;
    while (ReadFile(stderrRead_, buf, sizeof(buf), &n, nullptr) && n > 0) {
      if (stderrText_.size() < 65536) stderrText_.append(buf, n);
    }
  });
  return true;
}

ProcessResult ChildProcess::Run(const std::string& stdinData, CancelToken* cancel, const StdoutHandler& onStdout) {
  ProcessResult result;
  if (!process_) {
    result.error = startError_.empty() ? "Process not started" : startError_;
    return result;
  }

  HANDLE jobHandle = job_;
  if (cancel && !cancel->SetOnCancel([jobHandle] { TerminateJobObject(jobHandle, 1); })) {
    TerminateJobObject(jobHandle, 1);
  }

  std::thread writer([this, &stdinData] {
    size_t off = 0;
    while (off < stdinData.size()) {
      DWORD written = 0;
      DWORD chunk = static_cast<DWORD>(std::min<size_t>(stdinData.size() - off, 1 << 16));
      if (!WriteFile(stdinWrite_, stdinData.data() + off, chunk, &written, nullptr)) break;
      off += written;
    }
    CloseOwned(stdinWrite_);
  });

  // The watchdog enforces the timeout and wakes as soon as the process exits.
  std::atomic<bool> timedOut{false};
  std::thread watchdog([this, jobHandle, &timedOut] {
    if (WaitForSingleObject(process_, timeoutMs_) == WAIT_TIMEOUT) {
      timedOut = true;
      TerminateJobObject(jobHandle, 1);
    }
  });

  char buf[8192];
  DWORD n = 0;
  while (ReadFile(stdoutRead_, buf, sizeof(buf), &n, nullptr) && n > 0) {
    if (onStdout && !onStdout(std::string_view(buf, n))) {
      // The answer is complete; don't wait for the process to wind down on its own.
      result.stoppedEarly = true;
      TerminateJobObject(jobHandle, 0);
      break;
    }
  }

  WaitForSingleObject(process_, INFINITE);
  writer.join();
  watchdog.join();
  if (errReader_.joinable()) errReader_.join();
  if (cancel) cancel->ClearOnCancel();

  DWORD code = 0;
  GetExitCodeProcess(process_, &code);
  result.exitCode = result.stoppedEarly ? 0 : static_cast<int>(code);
  result.stderrText = std::move(stderrText_);
  if (cancel && cancel->IsCancelled()) {
    result.cancelled = true;
    result.error = "Cancelled";
  } else if (timedOut) {
    result.error = "Timed out";
  }
  return result;
}

ProcessResult RunProcess(const ProcessOptions& options, CancelToken* cancel, const StdoutHandler& onStdout) {
  ChildProcess child;
  if (!child.Start(options)) {
    ProcessResult result;
    result.error = child.StartError();
    return result;
  }
  return child.Run(options.stdinData, cancel, onStdout);
}

ProcessResult RunAndCapture(const ProcessOptions& options, std::string& out) {
  out.clear();
  return RunProcess(options, nullptr, [&out](std::string_view chunk) {
    out.append(chunk);
    return true;
  });
}

}  // namespace aia
