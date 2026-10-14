#pragma once

#include <string>
#include <string_view>

namespace aia {
// CF_HTML offsets count UTF-8 bytes, including the header (not UTF-16 characters).
std::string HtmlClipboardPayload(std::string_view fragment);
}
