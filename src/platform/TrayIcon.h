#pragma once

#include <windows.h>
#include <shellapi.h>

#include <string>

namespace aia {

enum TrayCommand : UINT {
  kTrayAsk = 1001,
  kTraySettings,
  kTrayAutostart,
  kTrayQuit,
};

class TrayIcon {
 public:
  // callbackMessage is sent to owner for mouse events (lParam = mouse message).
  bool Create(HWND owner, UINT callbackMessage, HICON icon, const std::wstring& tooltip);
  void Remove();
  // Explorer restarted: re-add the icon.
  void Recreate();
  void SetTooltip(const std::wstring& tooltip);
  // Shows the context menu at the cursor; returns the chosen TrayCommand or 0.
  UINT ShowMenu(bool autostartChecked, const std::wstring& hotkeyLabel, bool dark);

 private:
  NOTIFYICONDATAW data_{};
  bool added_ = false;
};

}  // namespace aia
