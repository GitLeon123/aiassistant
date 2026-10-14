#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace aia {

// A global shortcut in RegisterHotKey terms. Modifiers use the MOD_* bit values.
struct HotkeySpec {
  unsigned modifiers = 0;
  unsigned vk = 0;
  bool operator==(const HotkeySpec&) const = default;
};

// Parses strings such as "Alt+Space", "Ctrl+Shift+K", "Win+F9". Case-insensitive.
// Requires at least one modifier and exactly one non-modifier key.
std::optional<HotkeySpec> ParseHotkey(std::string_view text);

// Canonical form: modifiers in Ctrl, Alt, Shift, Win order, then the key name.
std::string FormatHotkey(const HotkeySpec& spec);

}  // namespace aia
