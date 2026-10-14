#include "core/UsageBudget.h"

#include <cmath>

namespace aia {
namespace {
constexpr int64_t kMaxMicros = 100000LL * 1000000LL;
bool ValidLedger(const json& ledger) {
  if (ledger.is_null()) return true;
  if (!ledger.is_object() || !ledger.contains("day") || !ledger["day"].is_string() ||
      !ledger.contains("reservedMicros") || !ledger["reservedMicros"].is_number_integer()) return false;
  auto value = ledger["reservedMicros"].get<int64_t>();
  return value >= 0 && value <= kMaxMicros;
}
}

double DailyAllowanceUsed(const json& ledger, const std::string& day) {
  if (!ValidLedger(ledger)) return -1;
  if (ledger.is_null() || ledger["day"] != day) return 0;
  return ledger["reservedMicros"].get<int64_t>() / 1000000.0;
}

BudgetReservation ReserveDailyAllowance(const json& ledger, const std::string& day,
                                        double dailyUsd, double perRequestUsd) {
  if (!std::isfinite(dailyUsd) || dailyUsd < 0 || dailyUsd > 100000) return {false, ledger, "Set a valid daily API allowance in Settings → Usage."};
  if (dailyUsd == 0) return {true, ledger, ""};
  if (!std::isfinite(perRequestUsd) || perRequestUsd <= 0 || perRequestUsd > 100000)
    return {false, ledger, "Set a positive allowance per API request in Settings → Usage."};
  if (!ValidLedger(ledger)) return {false, ledger, "The API allowance record is unreadable. Check usage-budget.json in the data folder before sending."};
  int64_t spent = ledger.is_null() || ledger["day"] != day ? 0 : ledger["reservedMicros"].get<int64_t>();
  int64_t limit = static_cast<int64_t>(std::floor(dailyUsd * 1000000 + 1e-7));
  int64_t charge = static_cast<int64_t>(std::ceil(perRequestUsd * 1000000 - 1e-7));
  if (charge > limit - spent) return {false, ledger, "Daily API allowance reached. Wait until tomorrow or change the guard in Settings → Usage."};
  return {true, {{"day", day}, {"reservedMicros", spent + charge}}, ""};
}
}
