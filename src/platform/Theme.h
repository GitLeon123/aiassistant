#pragma once

#include <string>

#include "core/Util.h"

namespace aia {

struct ThemeInfo {
  bool dark = false;
  std::string accent = "#0078d4";  // #rrggbb
  bool reducedMotion = false;
  bool operator==(const ThemeInfo&) const = default;
};

// Reads the Windows app theme (light/dark), accent colour and the "animation effects" setting.
ThemeInfo QueryTheme();
json ToJson(const ThemeInfo& theme);

// True when a WM_SETTINGCHANGE lParam indicates a theme/colour change worth re-querying.
bool IsThemeChangeMessage(const wchar_t* setting);

}  // namespace aia
