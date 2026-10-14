// AIAssistant: a background AI launcher for Windows.
//   AIAssistant.exe               start (or, if running, open Settings)
//   AIAssistant.exe --toggle      open/close the bar (bind this to a shortcut of your choice)
//   AIAssistant.exe --settings    open Settings
//   AIAssistant.exe --background  start quietly (used for "Start with Windows")

#include <windows.h>
#include <objbase.h>
#include <shellapi.h>

#include <string>

#include "app/App.h"
#include "core/Util.h"

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

  aia::LaunchOptions options;
  int argc = 0;
  LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  for (int i = 1; argv && i < argc; ++i) {
    std::wstring a = argv[i];
    if (a == L"--background") options.background = true;
    if (a == L"--toggle") options.toggle = true;
    if (a == L"--settings") options.settings = true;
  }
  if (argv) LocalFree(argv);

  // Single instance per user session. A second launch forwards its request to the first.
  HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\AIAssistant.SingleInstance");
  if (GetLastError() == ERROR_ALREADY_EXISTS) {
    if (!options.background) {
      HWND existing = nullptr;
      for (int i = 0; i < 20 && !existing; ++i) {
        existing = FindWindowW(aia::App::kHostClass, nullptr);
        if (!existing) Sleep(100);
      }
      if (existing) {
        DWORD pid = 0;
        GetWindowThreadProcessId(existing, &pid);
        AllowSetForegroundWindow(pid);
        std::string cmd = options.toggle ? "toggle" : "settings";
        COPYDATASTRUCT cds{aia::App::kCopyDataMagic, static_cast<DWORD>(cmd.size()), cmd.data()};
        DWORD_PTR result = 0;
        SendMessageTimeoutW(existing, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds), SMTO_ABORTIFHUNG, 3000,
                            &result);
      }
    }
    if (mutex) CloseHandle(mutex);
    return 0;
  }

  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED))) return 1;
  int code = 0;
  {
    aia::App app;
    code = app.Run(options);
  }
  CoUninitialize();
  if (mutex) CloseHandle(mutex);
  return code;
}
