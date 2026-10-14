#include <doctest/doctest.h>

#include "core/ClipboardFormat.h"
#include "core/Settings.h"
#include "core/UsageBudget.h"
#include "providers/Wire.h"

using namespace aia;

TEST_CASE("CF_HTML offsets enclose the exact UTF-8 fragment") {
  std::string fragment = "<h2>Übersicht 🙂</h2><table><tr><td>42</td></tr></table>";
  auto payload = HtmlClipboardPayload(fragment);
  auto offset = [&](const std::string& field) {
    return static_cast<size_t>(std::stoull(payload.substr(payload.find(field) + field.size(), 10)));
  };
  auto start = offset("StartFragment:"), end = offset("EndFragment:");
  CHECK(payload.substr(start, end - start) == fragment);
  CHECK(payload.substr(offset("StartHTML:"), 6) == "<html>");
  CHECK(offset("EndHTML:") == payload.size());
  CHECK(payload.substr(end, 18) == "<!--EndFragment-->");
}

TEST_CASE("API allowance is reserved before requests and survives restart and midnight") {
  auto first = ReserveDailyAllowance(nullptr, "2026-10-08", 0.30, 0.10);
  REQUIRE(first.allowed);
  auto restored = json::parse(first.ledger.dump());
  auto second = ReserveDailyAllowance(restored, "2026-10-08", 0.30, 0.10);
  REQUIRE(second.allowed);
  auto third = ReserveDailyAllowance(second.ledger, "2026-10-08", 0.30, 0.10);
  REQUIRE(third.allowed);
  auto denied = ReserveDailyAllowance(third.ledger, "2026-10-08", 0.30, 0.10);
  CHECK_FALSE(denied.allowed);
  CHECK(denied.ledger == third.ledger);
  CHECK(DailyAllowanceUsed(third.ledger, "2026-10-08") == doctest::Approx(0.30));
  auto tomorrow = ReserveDailyAllowance(third.ledger, "2026-10-09", 0.30, 0.10);
  CHECK(tomorrow.allowed);
  CHECK(DailyAllowanceUsed(tomorrow.ledger, "2026-10-09") == doctest::Approx(0.10));
  CHECK_FALSE(ReserveDailyAllowance(json::object(), "today", 1, 0.1).allowed);
  CHECK_FALSE(ReserveDailyAllowance(nullptr, "today", 1, 0).allowed);
  CHECK_FALSE(ReserveDailyAllowance(nullptr, "today", 0.1, 0.2).allowed);
  CHECK(ReserveDailyAllowance(nullptr, "today", 0, 0).allowed);
  CHECK_FALSE(ReserveDailyAllowance({{"day", "today"}, {"reservedMicros", -1}}, "today", 1, 0.1).allowed);
}

TEST_CASE("accessibility and usage settings validate and retain preferences") {
  Settings settings;
  settings.Apply({{"solidSurfaces", true}, {"textScale", 200}, {"barPinned", true}, {"apiDailyBudgetUsd", 2}, {"apiRequestAllowanceUsd", 0.25}});
  CHECK(settings.Data()["solidSurfaces"] == true);
  CHECK(settings.Data()["textScale"] == 200);
  CHECK(settings.Data()["barPinned"] == true);
  CHECK(settings.Data()["apiDailyBudgetUsd"] == 2);
  settings.Apply({{"textScale", 201}, {"apiDailyBudgetUsd", -1}, {"solidSurfaces", "yes"}});
  CHECK(settings.Data()["textScale"] == 100);
  CHECK(settings.Data()["apiDailyBudgetUsd"] == 0);
  CHECK(settings.Data()["solidSurfaces"] == false);
  CHECK(Settings::DefaultSystemPrompt().find("Never invent menu paths") != std::string::npos);
  settings.Apply({{"systemPrompt", "My own prompt"}});
  CHECK(settings.SystemPrompt() == "My own prompt");
}

TEST_CASE("Claude subscription warnings are surfaced without invented quota numbers") {
  std::string warning;
  StreamSink sink;
  sink.limit = [&](std::string_view text) { warning = text; };
  ClaudeCliStream stream(sink);
  stream.OnLine(R"({"type":"rate_limit_event","rate_limit_info":{"status":"allowed_warning","utilization":0.93}})");
  CHECK(warning.find("nearing a limit") != std::string::npos);
  stream.OnLine(R"({"type":"rate_limit_event","rate_limit_info":{"status":"rejected"}})");
  CHECK(warning.find("account usage limit") != std::string::npos);
  stream.OnLine(R"({"type":"rate_limit_event","rate_limit_info":{"status":"future-status"}})");
  CHECK_FALSE(warning.empty());
  stream.OnLine(R"({"type":"rate_limit_event","rate_limit_info":{"status":"allowed"}})");
  CHECK(warning.empty());
}
