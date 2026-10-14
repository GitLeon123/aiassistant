#include "core/Paths.h"

#include <windows.h>
#include <shlobj.h>

#include <cstdio>

namespace aia {
namespace {

std::wstring KnownFolder(REFKNOWNFOLDERID id, const wchar_t* sub) {
  PWSTR raw = nullptr;
  std::wstring base;
  if (SUCCEEDED(SHGetKnownFolderPath(id, KF_FLAG_CREATE, nullptr, &raw))) base = raw;
  CoTaskMemFree(raw);
  std::wstring dir = base + L"\\" + sub;
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir;
}

}  // namespace

std::wstring RoamingDataDir() {
  static std::wstring dir = KnownFolder(FOLDERID_RoamingAppData, L"AIAssistant");
  return dir;
}

std::wstring LocalDataDir() {
  static std::wstring dir = KnownFolder(FOLDERID_LocalAppData, L"AIAssistant");
  return dir;
}

std::wstring ExecutablePath() {
  wchar_t buf[MAX_PATH * 4];
  DWORD n = GetModuleFileNameW(nullptr, buf, static_cast<DWORD>(std::size(buf)));
  return std::wstring(buf, n);
}

std::wstring ExecutableDir() {
  std::wstring p = ExecutablePath();
  return p.substr(0, p.find_last_of(L"\\/"));
}

std::wstring TempDir() {
  wchar_t buf[MAX_PATH + 1];
  DWORD n = GetTempPathW(static_cast<DWORD>(std::size(buf)), buf);
  std::wstring dir = std::wstring(buf, n) + L"AIAssistant";
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir;
}

bool ReadFileUtf8(const std::wstring& path, std::string& out) {
  FILE* f = nullptr;
  if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return false;
  out.clear();
  char buf[16384];
  size_t n;
  while ((n = fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
  fclose(f);
  if (out.size() >= 3 && static_cast<unsigned char>(out[0]) == 0xEF &&
      static_cast<unsigned char>(out[1]) == 0xBB && static_cast<unsigned char>(out[2]) == 0xBF) {
    out.erase(0, 3);
  }
  return true;
}

bool WriteFileAtomic(const std::wstring& path, const std::string& data) {
  std::wstring tmp = path + L".tmp";
  FILE* f = nullptr;
  if (_wfopen_s(&f, tmp.c_str(), L"wb") != 0 || !f) return false;
  bool ok = fwrite(data.data(), 1, data.size(), f) == data.size();
  ok = (fclose(f) == 0) && ok;
  if (!ok) {
    DeleteFileW(tmp.c_str());
    return false;
  }
  return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

}  // namespace aia
