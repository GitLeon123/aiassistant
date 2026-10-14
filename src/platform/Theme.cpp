#include "platform/Theme.h"

#include <windows.h>

#include <cstdio>

namespace aia {
namespace {

bool ReadDword(HKEY root, const wchar_t* key, const wchar_t* value, DWORD& out) {
  DWORD size = sizeof(out);
  return RegGetValueW(root, key, value, RRF_RT_REG_DWORD, nullptr, &out, &size) == ERROR_SUCCESS;
}

}  // namespace

ThemeInfo QueryTheme() {
  ThemeInfo t;
  DWORD light = 1;
  if (ReadDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                L"AppsUseLightTheme", light)) {
    t.dark = light == 0;
  }
  // AccentColor is stored as 0xAABBGGRR.
  DWORD abgr = 0;
  if (ReadDword(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\DWM", L"AccentColor", abgr)) {
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", abgr & 0xFF, (abgr >> 8) & 0xFF, (abgr >> 16) & 0xFF);
    t.accent = buf;
  }
  BOOL animations = TRUE;
  if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0)) t.reducedMotion = !animations;
  return t;
}

json ToJson(const ThemeInfo& theme) {
  return {{"dark", theme.dark}, {"accent", theme.accent}, {"reducedMotion", theme.reducedMotion}};
}

bool IsThemeChangeMessage(const wchar_t* setting) {
  if (!setting) return true;  // SPI changes (e.g. animation effects) arrive without a name
  return lstrcmpiW(setting, L"ImmersiveColorSet") == 0 || lstrcmpiW(setting, L"WindowsThemeElement") == 0;
}

}  // namespace aia
