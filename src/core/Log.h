#pragma once

#include <string_view>

namespace aia {

// Appends a timestamped line to %LOCALAPPDATA%\AIAssistant\aiassistant.log (and the debugger).
// Never log secrets or message contents.
void Log(std::string_view message);

}  // namespace aia
