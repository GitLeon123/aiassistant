#pragma once

#include "core/Util.h"

namespace aia {
struct BudgetReservation {
  bool allowed = false;
  json ledger;
  std::string error;
};
// A local request-allowance guard, not a measurement of a provider's actual bill.
BudgetReservation ReserveDailyAllowance(const json& ledger, const std::string& day,
                                        double dailyUsd, double perRequestUsd);
double DailyAllowanceUsed(const json& ledger, const std::string& day);
}
