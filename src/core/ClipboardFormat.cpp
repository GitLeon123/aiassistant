#include "core/ClipboardFormat.h"

#include <cstdio>

namespace aia {
std::string HtmlClipboardPayload(std::string_view fragment) {
  const std::string prefix = "<html><body><!--StartFragment-->";
  const std::string suffix = "<!--EndFragment--></body></html>";
  const char* format = "Version:1.0\r\nStartHTML:%010llu\r\nEndHTML:%010llu\r\nStartFragment:%010llu\r\nEndFragment:%010llu\r\n";
  char header[256];
  int length = std::snprintf(header, sizeof(header), format, 0ULL, 0ULL, 0ULL, 0ULL);
  auto start = static_cast<unsigned long long>(length);
  auto startFragment = start + prefix.size();
  auto endFragment = startFragment + fragment.size();
  std::snprintf(header, sizeof(header), format, start, endFragment + suffix.size(), startFragment, endFragment);
  return std::string(header) + prefix + std::string(fragment) + suffix;
}
}
