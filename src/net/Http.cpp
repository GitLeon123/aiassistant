#include "net/Http.h"

#include <windows.h>
#include <winhttp.h>

#include <atomic>
#include <map>
#include <mutex>

#include "core/Log.h"

namespace aia {
namespace {

HINTERNET Session(bool useSystemProxy = true) {
  static std::once_flag once[2];
  static HINTERNET sessions[2]{};
  const int index = useSystemProxy ? 1 : 0;
  std::call_once(once[index], [index, useSystemProxy] {
    auto& session = sessions[index];
    session = WinHttpOpen(L"AIAssistant/0.1", useSystemProxy ? WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY : WINHTTP_ACCESS_TYPE_NO_PROXY,
                          WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!session && useSystemProxy) {
      // Older systems lack automatic proxy; fall back to the default.
      session = WinHttpOpen(L"AIAssistant/0.1", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                            WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    }
    if (session) {
      DWORD http2 = WINHTTP_PROTOCOL_FLAG_HTTP2;
      WinHttpSetOption(session, WINHTTP_OPTION_ENABLE_HTTP_PROTOCOL, &http2, sizeof(http2));
      DWORD decompress = WINHTTP_DECOMPRESSION_FLAG_ALL;
      WinHttpSetOption(session, WINHTTP_OPTION_DECOMPRESSION, &decompress, sizeof(decompress));
    }
  });
  return sessions[index];
}

std::string WinHttpErrorText(DWORD code) {
  switch (code) {
    case ERROR_WINHTTP_TIMEOUT: return "The request timed out";
    case ERROR_WINHTTP_NAME_NOT_RESOLVED: return "Could not resolve host (are you offline?)";
    case ERROR_WINHTTP_CANNOT_CONNECT: return "Could not connect to the server";
    case ERROR_WINHTTP_CONNECTION_ERROR: return "The connection was reset";
    case ERROR_WINHTTP_SECURE_FAILURE: return "TLS/certificate error";
    case ERROR_WINHTTP_OPERATION_CANCELLED: return "Cancelled";
    case ERROR_WINHTTP_INVALID_URL:
    case ERROR_WINHTTP_UNRECOGNIZED_SCHEME: return "Invalid URL";
    default: return "Network error (WinHTTP " + std::to_string(code) + ")";
  }
}

struct Handle {
  HINTERNET h = nullptr;
  ~Handle() {
    if (h) WinHttpCloseHandle(h);
  }
};

}  // namespace

HttpResponse HttpSend(const HttpRequest& request, CancelToken* cancel, const ChunkHandler& onChunk) {
  HttpResponse res;
  HINTERNET session = Session(request.useSystemProxy);
  if (!session) {
    res.error = "WinHTTP is unavailable";
    return res;
  }

  std::wstring url = Widen(request.url);
  URL_COMPONENTS uc{};
  uc.dwStructSize = sizeof(uc);
  uc.dwSchemeLength = uc.dwHostNameLength = uc.dwUrlPathLength = uc.dwExtraInfoLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(url.c_str(), 0, 0, &uc)) {
    res.error = "Invalid URL: " + request.url;
    return res;
  }
  std::wstring host(uc.lpszHostName, uc.dwHostNameLength);
  std::wstring path(uc.lpszUrlPath, uc.dwUrlPathLength);
  if (uc.lpszExtraInfo) path.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);
  if (path.empty()) path = L"/";
  bool secure = uc.nScheme == INTERNET_SCHEME_HTTPS;

  Handle connect{WinHttpConnect(session, host.c_str(), uc.nPort, 0)};
  if (!connect.h) {
    res.error = WinHttpErrorText(GetLastError());
    return res;
  }

  std::wstring method = Widen(request.method);
  std::atomic<HINTERNET> req{WinHttpOpenRequest(connect.h, method.c_str(), path.c_str(), nullptr,
                                                WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                secure ? WINHTTP_FLAG_SECURE : 0)};
  if (!req.load()) {
    res.error = WinHttpErrorText(GetLastError());
    return res;
  }
  auto closeRequest = [&req] {
    if (HINTERNET h = req.exchange(nullptr)) WinHttpCloseHandle(h);
  };
  struct Cleanup {
    CancelToken* cancel;
    std::function<void()> close;
    ~Cleanup() {
      if (cancel) cancel->ClearOnCancel();
      close();
    }
  } cleanup{cancel, closeRequest};

  if (cancel && !cancel->SetOnCancel(closeRequest)) {
    res.error = "Cancelled";
    res.cancelled = true;
    return res;
  }

  HINTERNET h = req.load();
  WinHttpSetTimeouts(h, request.connectTimeoutMs, request.connectTimeoutMs, request.sendTimeoutMs, request.receiveTimeoutMs);
  if (!request.followRedirects) {
    DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!WinHttpSetOption(h, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy))) {
      res.error = "Couldn't disable redirects for local discovery";
      return res;
    }
  }

  std::wstring headers;
  for (const auto& [k, v] : request.headers) headers += Widen(k) + L": " + Widen(v) + L"\r\n";
  if (!request.body.empty() && headers.find(L"Content-Type") == std::wstring::npos) {
    headers += L"Content-Type: application/json\r\n";
  }

  auto fail = [&](DWORD code) {
    if (cancel && cancel->IsCancelled()) {
      res.error = "Cancelled";
      res.cancelled = true;
    } else {
      res.error = WinHttpErrorText(code);
    }
    return res;
  };

  if (!WinHttpSendRequest(h, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(),
                          headers.empty() ? 0 : static_cast<DWORD>(-1L),
                          request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(request.body.data()),
                          static_cast<DWORD>(request.body.size()), static_cast<DWORD>(request.body.size()), 0)) {
    return fail(GetLastError());
  }
  if (!WinHttpReceiveResponse(h, nullptr)) return fail(GetLastError());

  DWORD status = 0, size = sizeof(status);
  WinHttpQueryHeaders(h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX,
                      &status, &size, WINHTTP_NO_HEADER_INDEX);
  res.status = static_cast<int>(status);
  bool stream = onChunk && res.status >= 200 && res.status < 300;

  std::string buf;
  for (;;) {
    DWORD avail = 0;
    if (!WinHttpQueryDataAvailable(h, &avail)) return fail(GetLastError());
    if (avail == 0) break;
    buf.resize(avail);
    DWORD read = 0;
    if (!WinHttpReadData(h, buf.data(), avail, &read)) return fail(GetLastError());
    if (read == 0) break;
    if (stream) {
      onChunk(std::string_view(buf.data(), read));
    } else {
      res.body.append(buf.data(), read);
    }
    if (cancel && cancel->IsCancelled()) return fail(ERROR_WINHTTP_OPERATION_CANCELLED);
  }
  return res;
}

void HttpPreconnect(const std::string& url) {
  std::wstring wide = Widen(url);
  URL_COMPONENTS uc{};
  uc.dwStructSize = sizeof(uc);
  uc.dwSchemeLength = uc.dwHostNameLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &uc) || !uc.lpszHostName) return;
  std::string origin = Narrow(std::wstring(uc.lpszScheme, uc.dwSchemeLength) + L"://" +
                              std::wstring(uc.lpszHostName, uc.dwHostNameLength) + L":" + std::to_wstring(uc.nPort));
  // WinHTTP keeps idle connections for about a minute; don't reconnect more often than that.
  static std::mutex mutex;
  static std::map<std::string, ULONGLONG> recent;
  {
    std::lock_guard lock(mutex);
    ULONGLONG now = GetTickCount64();
    auto& last = recent[origin];
    if (last && now - last < 45000) return;
    last = now;
  }
  HttpRequest req{"HEAD", origin + "/"};
  req.receiveTimeoutMs = 10000;
  HttpSend(req);  // Any response will do; it's the pooled connection we want
}

std::string DescribeHttpError(const HttpResponse& response) {
  if (!response.error.empty()) return response.error;
  std::string detail;
  try {
    json j = json::parse(response.body);
    // Common shapes: {error:{message}}, {error:"..."}, {message:"..."}, [{error:{message}}] (Gemini)
    if (j.is_array() && !j.empty()) j = j[0];
    if (j.contains("error")) {
      const json& e = j["error"];
      if (e.is_string()) detail = e.get<std::string>();
      else if (e.is_object()) detail = e.value("message", e.dump());
    } else if (j.contains("message") && j["message"].is_string()) {
      detail = j["message"].get<std::string>();
    } else if (j.contains("detail") && j["detail"].is_string()) {
      detail = j["detail"].get<std::string>();
    }
  } catch (...) {
    detail = response.body.substr(0, 300);
  }
  std::string prefix = "HTTP " + std::to_string(response.status);
  switch (response.status) {
    case 401: prefix = "Invalid or missing API key (401)"; break;
    case 403: prefix = "Access denied (403)"; break;
    case 404: prefix = "Not found (404)"; break;
    case 429: prefix = "Rate limited or out of credits (429)"; break;
    case 529: prefix = "Provider overloaded (529)"; break;
    default: break;
  }
  return detail.empty() ? prefix : prefix + ": " + detail;
}

}  // namespace aia
