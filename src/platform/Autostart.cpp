#include "platform/Autostart.h"

#include <windows.h>

#include <string>

#include "core/Paths.h"

namespace aia {
namespace {

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValue[] = L"AIAssistant";

std::wstring Command() { return L"\"" + ExecutablePath() + L"\" --background"; }

}  // namespace

bool IsAutostartEnabled() {
  wchar_t buf[2048];
  DWORD size = sizeof(buf);
  if (RegGetValueW(HKEY_CURRENT_USER, kRunKey, kValue, RRF_RT_REG_SZ, nullptr, buf, &size) != ERROR_SUCCESS) {
    return false;
  }
  // Only count it as enabled if it points at this executable.
  return lstrcmpiW(buf, Command().c_str()) == 0;
}

bool SetAutostartEnabled(bool enabled) {
  HKEY key;
  if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) return false;
  LSTATUS status;
  if (enabled) {
    std::wstring cmd = Command();
    status = RegSetValueExW(key, kValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(cmd.c_str()),
                            static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)));
  } else {
    status = RegDeleteValueW(key, kValue);
    if (status == ERROR_FILE_NOT_FOUND) status = ERROR_SUCCESS;
  }
  RegCloseKey(key);
  return status == ERROR_SUCCESS;
}

}  // namespace aia
