#include <doctest/doctest.h>

#include "core/CapturePolicy.h"
#include "core/Settings.h"

using namespace aia;

TEST_CASE("Sensitive-window protection covers default categories and leaves ordinary apps usable") {
  const auto rules = DefaultCaptureBlocklist();
  CHECK_FALSE(CaptureBlockReason(true, rules, "Code.exe", "roadmap.md - Visual Studio Code").size());
  CHECK(CaptureBlockReason(true, rules, "1Password.exe", "Vault").find("Password manager") != std::string::npos);
  CHECK(CaptureBlockReason(true, rules, "KeePassXC.exe", "Passwords").find("Password manager") != std::string::npos);
  CHECK(CaptureBlockReason(true, rules, "mstsc.exe", "Remote Desktop").find("Remote desktop") != std::string::npos);
  CHECK(CaptureBlockReason(true, rules, "chrome.exe", "Online Banking - Chrome").find("Banking window") != std::string::npos);
  CHECK(CaptureBlockReason(true, rules, "msedge.exe", "New tab - InPrivate").find("Private browser") != std::string::npos);
  CHECK(CaptureBlockReason(true, rules, "firefox.exe", "Privates Fenster").find("Private browser") != std::string::npos);
  CHECK_FALSE(CaptureBlockReason(false, rules, "1Password.exe", "Vault").size());
  CHECK_FALSE(CaptureBlockReason(false, rules, "", "").size());
  CHECK(CaptureBlockReason(true, rules, "", "Unknown app").find("could not be identified") != std::string::npos);
}

TEST_CASE("Custom capture rules combine process globs and literal title substrings") {
  const json rules = {{{"process", "browser?.exe"}, {"title", "Privat ÄÖÜ"}, {"label", "Private tab"}}};
  CHECK_FALSE(CaptureBlockReason(true, rules, "browser2.exe", "Public tab").size());
  CHECK_FALSE(CaptureBlockReason(true, rules, "editor.exe", "Privat ÄÖÜ").size());
  CHECK_FALSE(CaptureBlockReason(true, rules, "browser22.exe", "Privat ÄÖÜ").size());
  CHECK(CaptureBlockReason(true, rules, "BROWSER2.EXE", "Start — PRIVAT äöü").find("Private tab") != std::string::npos);
  CHECK(CaptureBlockReason(true, json::array({{{"process", "*"}, {"title", "[secret].*"}}}),
                           "browser.exe", "literal [secret].* title").size() > 0);
  CHECK_FALSE(CaptureBlockReason(true, json::array({{{"process", "*"}, {"title", "[secret].*"}}}),
                                 "browser.exe", "secret text").size());
  CHECK_FALSE(CaptureBlockReason(true, json::array(), "browser.exe", "Incognito").size());
}

TEST_CASE("Invalid blocklists restore protection; intentional empty lists remain empty") {
  const auto defaults = DefaultCaptureBlocklist();
  CHECK(NormalizeCaptureBlocklist(nullptr) == defaults);
  CHECK(NormalizeCaptureBlocklist(json::array({42})) == defaults);
  CHECK(NormalizeCaptureBlocklist(json::array({{{"process", false}}})) == defaults);
  CHECK(NormalizeCaptureBlocklist(json::array({json::object()})) == defaults);
  CHECK(NormalizeCaptureBlocklist(json::array()).empty());
  CHECK(NormalizeCaptureBlocklist(json::array({{{"title", "Payroll"}}}))[0]["process"] == "*");
  json tooMany = json::array();
  for (int i = 0; i < 129; ++i) tooMany.push_back({{"process", "app.exe"}});
  CHECK(NormalizeCaptureBlocklist(tooMany) == defaults);
}

TEST_CASE("Existing settings receive safe defaults and normalize privacy edits") {
  Settings settings;
  settings.Apply({{"hotkey", "Ctrl+Alt+Space"}, {"screenMode", "window"}});
  CHECK(settings.Data()["screenCaptureProtection"] == true);
  CHECK(settings.Data()["screenCaptureBlocklist"] == DefaultCaptureBlocklist());
  settings.Apply({{"screenCaptureBlocklist", json::array()}, {"screenCaptureProtection", false}});
  CHECK(settings.Data()["screenCaptureBlocklist"].empty());
  CHECK(settings.Data()["screenCaptureProtection"] == false);
  settings.Apply({{"screenCaptureProtection", "no"}, {"screenCaptureBlocklist", {false}}});
  CHECK(settings.Data()["screenCaptureProtection"] == true);
  CHECK(settings.Data()["screenCaptureBlocklist"] == DefaultCaptureBlocklist());
}
