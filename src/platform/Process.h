#pragma once

#include <windows.h>

// shellapi.h defines a Win32 FindExecutable macro; this helper has a different signature.
#ifdef FindExecutable
#undef FindExecutable
#endif

#include <functional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "core/Util.h"

namespace aia {

struct ProcessOptions {
  std::wstring exe;                 // Full path (.exe, or .cmd/.bat which run through cmd.exe)
  std::vector<std::wstring> args;
  std::wstring cwd;                 // Empty = inherit
  std::vector<std::pair<std::wstring, std::wstring>> env;  // Added to / overriding the inherited environment
  std::string stdinData;            // Written to stdin, which is then closed
  unsigned long timeoutMs = 10 * 60 * 1000;
};

struct ProcessResult {
  int exitCode = -1;
  std::string stderrText;  // First 64 KB of stderr
  std::string error;       // Launch failure / timeout / cancellation
  bool cancelled = false;
  bool stoppedEarly = false;  // The stdout handler asked to stop; the process was terminated
};

// Receives stdout as it arrives. Return false to stop reading and terminate the process
// (e.g. once the final event has been parsed, rather than waiting for the CLI to wind down).
using StdoutHandler = std::function<bool(std::string_view)>;

// A child process with redirected pipes inside a kill-on-close Job object (so cancelling
// terminates the whole process tree). It can be started ahead of time with stdin left open,
// so its startup cost is paid before the input is known.
class ChildProcess {
 public:
  ChildProcess() = default;
  ChildProcess(const ChildProcess&) = delete;
  ChildProcess& operator=(const ChildProcess&) = delete;
  ~ChildProcess();

  // Spawns the process. options.stdinData is ignored; pass the input to Run.
  bool Start(const ProcessOptions& options);
  const std::string& StartError() const { return startError_; }
  bool Running() const;

  // Writes stdinData, closes stdin and streams stdout until EOF (or the handler returns false).
  // Call at most once.
  ProcessResult Run(const std::string& stdinData, CancelToken* cancel, const StdoutHandler& onStdout);

 private:
  void Terminate();

  HANDLE job_ = nullptr;
  HANDLE process_ = nullptr;
  HANDLE stdinWrite_ = nullptr;
  HANDLE stdoutRead_ = nullptr;
  HANDLE stderrRead_ = nullptr;
  std::thread errReader_;
  std::string stderrText_;
  std::string startError_;
  unsigned long timeoutMs_ = 0;
};

// Starts a process, writes options.stdinData and streams stdout to onStdout.
ProcessResult RunProcess(const ProcessOptions& options, CancelToken* cancel, const StdoutHandler& onStdout);

// Convenience: runs and returns all stdout (for short detection commands).
ProcessResult RunAndCapture(const ProcessOptions& options, std::string& out);

// Looks for name (+ .exe/.cmd/.bat) on PATH and then in extraDirs. Returns empty if not found.
std::wstring FindExecutable(const std::wstring& name, const std::vector<std::wstring>& extraDirs = {});

// Quotes one argument following the MSVCRT CommandLineToArgvW rules.
std::wstring QuoteArg(const std::wstring& arg);

}  // namespace aia
