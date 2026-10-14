#pragma once

#include <string>

namespace aia {

std::wstring RoamingDataDir();  // %APPDATA%\AIAssistant (settings)
std::wstring LocalDataDir();    // %LOCALAPPDATA%\AIAssistant (WebView2 data, logs, caches)
std::wstring ExecutablePath();
std::wstring ExecutableDir();
std::wstring TempDir();          // %TEMP%\AIAssistant (scratch files for CLI bridges)

bool ReadFileUtf8(const std::wstring& path, std::string& out);
// Writes via a temp file + rename so a crash never leaves a truncated file behind.
bool WriteFileAtomic(const std::wstring& path, const std::string& data);

}  // namespace aia
