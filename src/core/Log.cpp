#include "core/Log.h"

#include <windows.h>

#include <cstdio>
#include <mutex>
#include <string>

#include "core/Paths.h"
#include "core/Util.h"

namespace aia {

void Log(std::string_view message) {
  static std::mutex mutex;
  SYSTEMTIME st;
  GetLocalTime(&st);
  char prefix[64];
  snprintf(prefix, sizeof(prefix), "%04d-%02d-%02d %02d:%02d:%02d.%03d [%lu] ", st.wYear,
           st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
           GetCurrentThreadId());
  std::string line = std::string(prefix) + std::string(message) + "\n";

  OutputDebugStringW(Widen(line).c_str());

  std::lock_guard lock(mutex);
  static std::wstring path = LocalDataDir() + L"\\aiassistant.log";
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"ab") == 0 && f) {
    fwrite(line.data(), 1, line.size(), f);
    fclose(f);
  }
}

}  // namespace aia
