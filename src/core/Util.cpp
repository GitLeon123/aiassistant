#include "core/Util.h"

#include <windows.h>

#include <algorithm>

namespace aia {

std::wstring Widen(std::string_view utf8) {
  if (utf8.empty()) return {};
  int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
  return out;
}

std::string Narrow(std::wstring_view wide) {
  if (wide.empty()) return {};
  int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0,
                              nullptr, nullptr);
  std::string out(static_cast<size_t>(n), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), n,
                      nullptr, nullptr);
  return out;
}

std::string Base64Encode(const uint8_t* data, size_t size) {
  static constexpr char kTable[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((size + 2) / 3) * 4);
  size_t i = 0;
  for (; i + 2 < size; i += 3) {
    uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8) | data[i + 2];
    out.push_back(kTable[(v >> 18) & 63]);
    out.push_back(kTable[(v >> 12) & 63]);
    out.push_back(kTable[(v >> 6) & 63]);
    out.push_back(kTable[v & 63]);
  }
  if (i + 1 == size) {
    uint32_t v = uint32_t(data[i]) << 16;
    out.push_back(kTable[(v >> 18) & 63]);
    out.push_back(kTable[(v >> 12) & 63]);
    out += "==";
  } else if (i + 2 == size) {
    uint32_t v = (uint32_t(data[i]) << 16) | (uint32_t(data[i + 1]) << 8);
    out.push_back(kTable[(v >> 18) & 63]);
    out.push_back(kTable[(v >> 12) & 63]);
    out.push_back(kTable[(v >> 6) & 63]);
    out.push_back('=');
  }
  return out;
}

std::vector<uint8_t> Base64Decode(std::string_view text) {
  auto value = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
  };
  std::vector<uint8_t> out;
  out.reserve(text.size() * 3 / 4);
  uint32_t acc = 0;
  int bits = 0;
  for (char c : text) {
    int v = value(c);
    if (v < 0) continue;  // skips '=', whitespace
    acc = (acc << 6) | static_cast<uint32_t>(v);
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back(static_cast<uint8_t>((acc >> bits) & 0xFF));
    }
  }
  return out;
}

std::string ToLower(std::string_view s) {
  std::string out(s);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool StartsWith(std::string_view s, std::string_view prefix) {
  return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

bool Contains(std::string_view s, std::string_view needle) {
  return s.find(needle) != std::string_view::npos;
}

std::string TrimRight(std::string_view s, std::string_view chars) {
  size_t end = s.find_last_not_of(chars);
  if (end == std::string_view::npos) return {};
  return std::string(s.substr(0, end + 1));
}

}  // namespace aia
