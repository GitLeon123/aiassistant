#include <doctest/doctest.h>

#include <windows.h>

#include "core/HotkeySpec.h"
#include "core/Util.h"

using namespace aia;

TEST_CASE("Hotkey: parse common shortcuts") {
  auto a = ParseHotkey("Alt+Space");
  REQUIRE(a);
  CHECK(a->modifiers == MOD_ALT);
  CHECK(a->vk == VK_SPACE);

  auto b = ParseHotkey("ctrl + shift + k");
  REQUIRE(b);
  CHECK(b->modifiers == (MOD_CONTROL | MOD_SHIFT));
  CHECK(b->vk == 'K');

  auto c = ParseHotkey("Win+F9");
  REQUIRE(c);
  CHECK(c->modifiers == MOD_WIN);
  CHECK(c->vk == VK_F9);
}

TEST_CASE("Hotkey: rejects invalid shortcuts") {
  CHECK_FALSE(ParseHotkey(""));
  CHECK_FALSE(ParseHotkey("Space"));           // no modifier
  CHECK_FALSE(ParseHotkey("Ctrl+Alt"));        // no key
  CHECK_FALSE(ParseHotkey("Ctrl+A+B"));        // two keys
  CHECK_FALSE(ParseHotkey("Ctrl+Nonsense"));
}

TEST_CASE("Hotkey: format is canonical and round-trips") {
  auto spec = ParseHotkey("shift+win+ctrl+alt+j");
  REQUIRE(spec);
  CHECK(FormatHotkey(*spec) == "Ctrl+Alt+Shift+Win+J");
  for (const char* s : {"Alt+Space", "Ctrl+Shift+K", "Win+F9", "Ctrl+Alt+1", "Alt+Enter", "Ctrl+Backquote"}) {
    auto p = ParseHotkey(s);
    REQUIRE_MESSAGE(p, s);
    CHECK(FormatHotkey(*p) == s);
  }
}

TEST_CASE("Base64 round trip") {
  std::vector<uint8_t> data;
  for (int i = 0; i < 300; ++i) data.push_back(static_cast<uint8_t>(i * 7));
  for (size_t n : {0u, 1u, 2u, 3u, 4u, 299u}) {
    std::vector<uint8_t> slice(data.begin(), data.begin() + n);
    CHECK(Base64Decode(Base64Encode(slice)) == slice);
  }
  CHECK(Base64Encode(reinterpret_cast<const uint8_t*>("Man"), 3) == "TWFu");
}

TEST_CASE("JsonGet walks paths with fallbacks") {
  json j = {{"a", {{"b", {{"c", 5}}}}}};
  CHECK(JsonGet<int>(j, "a/b/c", 0) == 5);
  CHECK(JsonGet<int>(j, "a/x/c", 7) == 7);
  CHECK(JsonGet<std::string>(j, "a/b/c", "no") == "no");
}
