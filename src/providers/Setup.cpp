#include "providers/Setup.h"

#include <windows.h>
#include <winhttp.h>

#include <algorithm>

#include "net/Http.h"
#include "providers/Wire.h"

namespace aia {

bool IsLoopbackUrl(const std::string& url) {
  auto wide = Widen(url);
  URL_COMPONENTS parts{};
  parts.dwStructSize = sizeof(parts);
  parts.dwHostNameLength = parts.dwUserNameLength = parts.dwPasswordLength = static_cast<DWORD>(-1);
  if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts) || parts.dwUserNameLength || parts.dwPasswordLength) return false;
  if (parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) return false;
  auto host = ToLower(Narrow(std::wstring_view(parts.lpszHostName, parts.dwHostNameLength)));
  return host == "localhost" || host == "127.0.0.1" || host == "::1" || host == "[::1]";
}

ModelListResult DiscoverLocalModels(const std::string& providerId, const std::string& baseUrl) {
  if (!IsLoopbackUrl(baseUrl)) return {{}, "Use Test connection to check this endpoint."};
  HttpRequest request{"GET", TrimRight(baseUrl, "/ ") + "/models"};
  request.receiveTimeoutMs = request.connectTimeoutMs = request.sendTimeoutMs = 3000;
  request.followRedirects = false;
  request.useSystemProxy = false;
  auto response = HttpSend(request);
  if (!response.Ok()) return {{}, "Start the local server, then check again. " + DescribeHttpError(response)};
  auto body = json::parse(response.body, nullptr, false);
  if (!body.is_object() || !body.contains("data") || !body["data"].is_array()) {
    return {{}, "The server did not return a model list. Check its address."};
  }
  auto models = ParseOpenAIStyleModels(body, providerId);
  return {models, models.empty() ? "Load or download a model in the local app, then check again." : ""};
}

SetupAnswer TestSetupAnswer(Provider& provider, const ModelInfo& model, CancelToken& cancel) {
  ChatRequest request;
  request.model = model.id;
  request.system = "You are helping test an AI connection. Answer briefly in plain text.";
  request.messages.push_back({"user", "Say hello in one short sentence."});
  SetupAnswer answer;
  StreamSink sink;
  sink.text = [&](std::string_view text) {
    if (answer.answer.size() >= 4096) return;
    auto length = std::min(text.size(), 4096 - answer.answer.size());
    // Do not cut a multibyte UTF-8 sequence at the display limit.
    if (length < text.size()) {
      while (length && (static_cast<unsigned char>(text[length]) & 0xC0) == 0x80) --length;
    }
    answer.answer.append(text.substr(0, length));
  };
  auto result = provider.Stream(request, model, sink, cancel);
  answer.ok = result.ok && !result.cancelled && !cancel.IsCancelled() &&
              answer.answer.find_first_not_of(" \t\r\n") != std::string::npos;
  if (!answer.ok) answer.error = result.error.empty() ? "No answer received. Check the provider and try again." : result.error;
  return answer;
}

}  // namespace aia
