#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/Util.h"

namespace aia {

struct HttpRequest {
  std::string method = "GET";
  std::string url;
  std::vector<std::pair<std::string, std::string>> headers;
  std::string body;
  int receiveTimeoutMs = 5 * 60 * 1000;
  int connectTimeoutMs = 15000;
  int sendTimeoutMs = 60000;
  bool followRedirects = true;
  bool useSystemProxy = true;
};

struct HttpResponse {
  int status = 0;
  std::string body;   // Full body when no chunk handler was given, or for non-2xx responses.
  std::string error;  // Transport error (DNS, TLS, cancelled...). Empty on success.
  bool cancelled = false;
  bool Ok() const { return error.empty() && status >= 200 && status < 300; }
};

using ChunkHandler = std::function<void(std::string_view)>;

// Blocking HTTP(S) request via WinHTTP; call from a worker thread.
// With onChunk set, 2xx response bodies are streamed to it as they arrive.
// The cancel token (optional) aborts the request from another thread.
HttpResponse HttpSend(const HttpRequest& request, CancelToken* cancel = nullptr,
                      const ChunkHandler& onChunk = nullptr);

// Opens a pooled keep-alive connection to url's server so the next request skips DNS, TCP and
// the TLS handshake. Blocking; repeated calls for the same server within a short window are no-ops.
void HttpPreconnect(const std::string& url);

// A human-readable error for a failed response ("401: invalid x-api-key").
std::string DescribeHttpError(const HttpResponse& response);

}  // namespace aia
