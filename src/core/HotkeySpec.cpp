#include "core/HotkeySpec.h"

#include <windows.h>

#include <vector>

#include "core/Util.h"

namespace aia {
namespace {

struct KeyName {
  const char* name;
  unsigned vk;
};

// Named (non-alphanumeric) keys. The first entry for a VK is its canonical name.
constexpr KeyName kNamedKeys[] = {
    {"Space", VK_SPACE},         {"Enter", VK_RETURN},       {"Return", VK_RETURN},
    {"Tab", VK_TAB},             {"Backspace", VK_BACK},     {"Delete", VK_DELETE},
    {"Del", VK_DELETE},          {"Insert", VK_INSERT},      {"Home", VK_HOME},
    {"End", VK_END},             {"PageUp", VK_PRIOR},       {"PageDown", VK_NEXT},
    {"Up", VK_UP},               {"Down", VK_DOWN},          {"Left", VK_LEFT},
    {"Right", VK_RIGHT},         {"Backquote", VK_OEM_3},    {"`", VK_OEM_3},
    {"Minus", VK_OEM_MINUS},     {"-", VK_OEM_MINUS},        {"Equal", VK_OEM_PLUS},
    {"=", VK_OEM_PLUS},          {"Comma", VK_OEM_COMMA},    {",", VK_OEM_COMMA},
    {"Period", VK_OEM_PERIOD},   {".", VK_OEM_PERIOD},       {"Slash", VK_OEM_2},
    {"/", VK_OEM_2},             {"Semicolon", VK_OEM_1},    {";", VK_OEM_1},
    {"Quote", VK_OEM_7},         {"'", VK_OEM_7},            {"BracketLeft", VK_OEM_4},
    {"[", VK_OEM_4},             {"BracketRight", VK_OEM_6}, {"]", VK_OEM_6},
    {"Backslash", VK_OEM_5},     {"\\", VK_OEM_5},           {"Pause", VK_PAUSE},
    {"PrintScreen", VK_SNAPSHOT}, {"ScrollLock", VK_SCROLL},
};

std::vector<std::string> Split(std::string_view text) {
  // '+' is a separator, except a trailing "++" means the key itself is '+' (unsupported anyway).
  std::vector<std::string> parts;
  std::string cur;
  for (char c : text) {
    if (c == '+') {
      parts.push_back(cur);
      cur.clear();
    } else if (c != ' ') {
      cur.push_back(c);
    }
  }
  parts.push_back(cur);
  return parts;
}

std::optional<unsigned> KeyFromName(const std::string& name) {
  std::string lower = ToLower(name);
  if (lower.size() == 1) {
    char c = lower[0];
    if (c >= 'a' && c <= 'z') return static_cast<unsigned>('A' + (c - 'a'));
    if (c >= '0' && c <= '9') return static_cast<unsigned>(c);
  }
  if (lower.size() >= 2 && lower[0] == 'f') {
    bool digits = true;
    for (size_t i = 1; i < lower.size(); ++i) digits = digits && std::isdigit(static_cast<unsigned char>(lower[i]));
    if (digits) {
      int n = std::stoi(lower.substr(1));
      if (n >= 1 && n <= 24) return static_cast<unsigned>(VK_F1 + n - 1);
    }
  }
  for (const auto& k : kNamedKeys) {
    if (ToLower(k.name) == lower) return k.vk;
  }
  return std::nullopt;
}

std::string NameFromKey(unsigned vk) {
  if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::string(1, static_cast<char>(vk));
  if (vk >= VK_F1 && vk <= VK_F24) return "F" + std::to_string(vk - VK_F1 + 1);
  for (const auto& k : kNamedKeys) {
    if (k.vk == vk) return k.name;
  }
  return "VK" + std::to_string(vk);
}

}  // namespace

std::optional<HotkeySpec> ParseHotkey(std::string_view text) {
  HotkeySpec spec;
  bool haveKey = false;
  for (const auto& part : Split(text)) {
    if (part.empty()) return std::nullopt;
    std::string p = ToLower(part);
    if (p == "ctrl" || p == "control") {
      spec.modifiers |= MOD_CONTROL;
    } else if (p == "alt" || p == "option") {
      spec.modifiers |= MOD_ALT;
    } else if (p == "shift") {
      spec.modifiers |= MOD_SHIFT;
    } else if (p == "win" || p == "windows" || p == "meta" || p == "super") {
      spec.modifiers |= MOD_WIN;
    } else {
      if (haveKey) return std::nullopt;
      auto vk = KeyFromName(part);
      if (!vk) return std::nullopt;
      spec.vk = *vk;
      haveKey = true;
    }
  }
  if (!haveKey || spec.modifiers == 0) return std::nullopt;
  return spec;
}

std::string FormatHotkey(const HotkeySpec& spec) {
  std::string out;
  if (spec.modifiers & MOD_CONTROL) out += "Ctrl+";
  if (spec.modifiers & MOD_ALT) out += "Alt+";
  if (spec.modifiers & MOD_SHIFT) out += "Shift+";
  if (spec.modifiers & MOD_WIN) out += "Win+";
  out += NameFromKey(spec.vk);
  return out;
}

}  // namespace aia
