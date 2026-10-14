#include <doctest/doctest.h>

#include <windows.h>

#include <string>

#include "platform/Process.h"

using namespace aia;

namespace {

std::wstring Cmd() {
  wchar_t comspec[MAX_PATH];
  if (!GetEnvironmentVariableW(L"ComSpec", comspec, MAX_PATH)) return L"cmd.exe";
  return comspec;
}

}  // namespace

TEST_CASE("a process started ahead of time receives its input later") {
  ProcessOptions opts;
  opts.exe = Cmd();
  opts.args = {L"/d", L"/c", L"sort"};
  ChildProcess child;
  REQUIRE(child.Start(opts));
  Sleep(200);
  CHECK(child.Running());  // Waiting on stdin
  std::string out;
  ProcessResult r = child.Run("b\r\na\r\n", nullptr, [&](std::string_view chunk) {
    out.append(chunk);
    return true;
  });
  CHECK(r.error.empty());
  CHECK(r.exitCode == 0);
  CHECK(out == "a\r\nb\r\n");
}

TEST_CASE("the stdout handler can stop a process early") {
  ProcessOptions opts;
  opts.exe = Cmd();
  opts.args = {L"/d", L"/c", L"ping -n 30 127.0.0.1"};  // Prints for ~30 s
  ULONGLONG start = GetTickCount64();
  ProcessResult r = RunProcess(opts, nullptr, [](std::string_view) { return false; });
  CHECK(r.stoppedEarly);
  CHECK(r.exitCode == 0);
  CHECK(r.error.empty());
  CHECK(GetTickCount64() - start < 5000);
}

TEST_CASE("environment overrides reach the child") {
  ProcessOptions opts;
  opts.exe = Cmd();
  opts.args = {L"/d", L"/c", L"echo %AIA_TEST_VALUE%"};
  opts.env = {{L"AIA_TEST_VALUE", L"hello"}};
  std::string out;
  ProcessResult r = RunAndCapture(opts, out);
  CHECK(r.exitCode == 0);
  CHECK(out.find("hello") != std::string::npos);
  CHECK(out.find("%AIA_TEST_VALUE%") == std::string::npos);
}

TEST_CASE("an unused started process is terminated on destruction") {
  ProcessOptions opts;
  opts.exe = Cmd();
  opts.args = {L"/d", L"/c", L"sort"};
  ULONGLONG start = GetTickCount64();
  {
    ChildProcess child;
    REQUIRE(child.Start(opts));
  }
  CHECK(GetTickCount64() - start < 3000);
}
