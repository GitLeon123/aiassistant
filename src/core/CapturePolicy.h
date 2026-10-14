#pragma once

#include <string>

#include "core/Util.h"

namespace aia {

// Process names use * and ? wildcards. Titles are case-insensitive substrings.
// If both are supplied they must both match. An empty title matches any title.
json DefaultCaptureBlocklist();
json NormalizeCaptureBlocklist(const json& rules);
std::string CaptureBlockReason(bool enabled, const json& rules, const std::string& process,
                               const std::string& title);

}  // namespace aia
