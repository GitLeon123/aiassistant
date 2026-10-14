#include "core/CapturePolicy.h"

#include <windows.h>

#include <algorithm>

namespace aia {
namespace {

std::wstring Fold(const std::string& text) {
  auto wide = Widen(text);
  if (wide.empty()) return wide;
  std::wstring out(wide.size(), L'\0');
  if (!LCMapStringEx(LOCALE_NAME_INVARIANT, LCMAP_LOWERCASE, wide.data(), static_cast<int>(wide.size()),
                     out.data(), static_cast<int>(out.size()), nullptr, nullptr, 0)) return wide;
  return out;
}

bool Glob(const std::wstring& pattern, const std::wstring& value) {
  size_t p = 0, v = 0, star = std::wstring::npos, retry = 0;
  while (v < value.size()) {
    if (p < pattern.size() && (pattern[p] == L'?' || pattern[p] == value[v])) { ++p; ++v; }
    else if (p < pattern.size() && pattern[p] == L'*') { star = p++; retry = v; }
    else if (star != std::wstring::npos) { p = star + 1; v = ++retry; }
    else return false;
  }
  while (p < pattern.size() && pattern[p] == L'*') ++p;
  return p == pattern.size();
}

}  // namespace

json DefaultCaptureBlocklist() {
  json rules = json::array();
  auto add = [&](const char* process, const char* title, const char* label) {
    rules.push_back({{"process", process}, {"title", title}, {"label", label}});
  };
  for (const char* process : {"1password.exe", "1password*.exe", "bitwarden.exe", "keepass*.exe",
                              "dashlane*.exe", "enpass.exe", "nordpass.exe", "keeper*.exe"}) {
    add(process, "", "Password manager");
  }
  for (const char* process : {"mstsc.exe", "msrdc.exe", "rdclient*.exe", "windowsapp.exe",
                              "teamviewer*.exe", "anydesk.exe", "rustdesk.exe"}) {
    add(process, "", "Remote desktop");
  }
  for (const char* title : {"banking", "online bank", "internet bank", "ebanking", "e-banking",
                            "bank of", "sparkasse", "raiffeisen", "erste bank"}) {
    add("*", title, "Banking window");
  }
  for (const char* title : {"incognito", "inprivate", "private browsing", "private window",
                            "inkognito", "privates fenster", "privater modus"}) {
    add("*", title, "Private browser window");
  }
  return rules;
}

json NormalizeCaptureBlocklist(const json& rules) {
  if (!rules.is_array() || rules.size() > 128) return DefaultCaptureBlocklist();
  json out = json::array();
  for (const auto& rule : rules) {
    if (!rule.is_object()) return DefaultCaptureBlocklist();
    for (const char* key : {"process", "title", "label"}) {
      if (rule.contains(key) && !rule[key].is_string()) return DefaultCaptureBlocklist();
    }
    std::string process = rule.value("process", ""), title = rule.value("title", "");
    std::string label = rule.value("label", "Blocked window");
    if ((process.empty() && title.empty()) || process.size() > 260 || title.size() > 256 || label.size() > 120) {
      return DefaultCaptureBlocklist();
    }
    out.push_back({{"process", process.empty() ? "*" : process}, {"title", title},
                   {"label", label.empty() ? "Blocked window" : label}});
  }
  return out;
}

std::string CaptureBlockReason(bool enabled, const json& rules, const std::string& process,
                               const std::string& title) {
  if (!enabled) return {};
  if (process.empty()) return "Screen capture blocked: the active app could not be identified. Reopen the bar from another app.";
  const auto exe = Fold(process), caption = Fold(title);
  for (const auto& rule : NormalizeCaptureBlocklist(rules)) {
    if (Glob(Fold(rule["process"].get<std::string>()), exe) &&
        caption.find(Fold(rule["title"].get<std::string>())) != std::wstring::npos) {
      return "Screen capture blocked: " + rule["label"].get<std::string>() +
             ". Review the blocklist in Settings > Privacy.";
    }
  }
  return {};
}

}  // namespace aia
