#include "platform/TrayIcon.h"

#include <shellapi.h>

namespace aia {
namespace {

// Undocumented but stable uxtheme exports that make Win32 popup menus follow the dark theme.
enum class PreferredAppMode { Default, AllowDark, ForceDark, ForceLight };
using SetPreferredAppModeFn = PreferredAppMode(WINAPI*)(PreferredAppMode);
using FlushMenuThemesFn = void(WINAPI*)();

void ApplyMenuTheme(bool dark) {
  static HMODULE ux = LoadLibraryExW(L"uxtheme.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
  if (!ux) return;
  static auto setMode = reinterpret_cast<SetPreferredAppModeFn>(GetProcAddress(ux, MAKEINTRESOURCEA(135)));
  static auto flush = reinterpret_cast<FlushMenuThemesFn>(GetProcAddress(ux, MAKEINTRESOURCEA(136)));
  if (setMode) setMode(dark ? PreferredAppMode::ForceDark : PreferredAppMode::ForceLight);
  if (flush) flush();
}

}  // namespace

bool TrayIcon::Create(HWND owner, UINT callbackMessage, HICON icon, const std::wstring& tooltip) {
  data_ = {};
  data_.cbSize = sizeof(data_);
  data_.hWnd = owner;
  data_.uID = 1;
  data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
  data_.uCallbackMessage = callbackMessage;
  data_.hIcon = icon;
  lstrcpynW(data_.szTip, tooltip.c_str(), ARRAYSIZE(data_.szTip));
  added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
  if (added_) {
    data_.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &data_);
  }
  return added_;
}

void TrayIcon::Remove() {
  if (added_) Shell_NotifyIconW(NIM_DELETE, &data_);
  added_ = false;
}

void TrayIcon::Recreate() {
  added_ = Shell_NotifyIconW(NIM_ADD, &data_) != FALSE;
  if (added_) Shell_NotifyIconW(NIM_SETVERSION, &data_);
}

void TrayIcon::SetTooltip(const std::wstring& tooltip) {
  lstrcpynW(data_.szTip, tooltip.c_str(), ARRAYSIZE(data_.szTip));
  if (added_) Shell_NotifyIconW(NIM_MODIFY, &data_);
}

UINT TrayIcon::ShowMenu(bool autostartChecked, const std::wstring& hotkeyLabel, bool dark) {
  ApplyMenuTheme(dark);
  HMENU menu = CreatePopupMenu();
  std::wstring ask = L"Ask AI…\t" + hotkeyLabel;
  AppendMenuW(menu, MF_STRING, kTrayAsk, ask.c_str());
  AppendMenuW(menu, MF_STRING, kTraySettings, L"Settings…");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING | (autostartChecked ? MF_CHECKED : 0), kTrayAutostart, L"Start with Windows");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, kTrayQuit, L"Quit AIAssistant");
  SetMenuDefaultItem(menu, kTrayAsk, FALSE);

  POINT pt;
  GetCursorPos(&pt);
  // Required so the menu closes when the user clicks elsewhere.
  SetForegroundWindow(data_.hWnd);
  UINT cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON |
                                        (GetSystemMetrics(SM_MENUDROPALIGNMENT) ? TPM_RIGHTALIGN : TPM_LEFTALIGN),
                              pt.x, pt.y, data_.hWnd, nullptr);
  PostMessageW(data_.hWnd, WM_NULL, 0, 0);
  DestroyMenu(menu);
  return cmd;
}

}  // namespace aia
