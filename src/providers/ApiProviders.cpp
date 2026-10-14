// HTTP API providers: Anthropic, OpenAI (Responses), OpenAI-compatible (Chat Completions), Gemini.

#include "core/Log.h"
#include "net/Http.h"
#include "net/Sse.h"
#include "providers/Catalog.h"
#include "providers/Registry.h"
#include "providers/Wire.h"

namespace aia {
namespace {

using Headers = std::vector<std::pair<std::string, std::string>>;

// Runs an SSE request, feeding each event's JSON payload to onJson (or raw data to onData).
HttpResponse StreamSse(HttpRequest req, CancelToken& cancel,
                       const std::function<void(const std::string& event, const std::string& data)>& onEvent) {
  req.headers.push_back({"Accept", "text/event-stream"});
  SseParser parser(onEvent);
  HttpResponse res = HttpSend(req, &cancel, [&](std::string_view chunk) { parser.Feed(chunk); });
  if (res.Ok()) parser.Finish();
  return res;
}

void FeedJson(const std::string& data, const std::function<void(const json&)>& fn) {
  try {
    fn(json::parse(data));
  } catch (const json::exception&) {
    // Ignore keep-alives and non-JSON payloads.
  }
}

StreamResult HttpFailure(const HttpResponse& res) {
  StreamResult r;
  r.cancelled = res.cancelled;
  r.error = DescribeHttpError(res);
  return r;
}

StreamResult Finalize(StreamResult r, const HttpResponse& res, const char* providerName) {
  if (res.cancelled) {
    r.cancelled = true;
    r.ok = false;
    if (r.error.empty()) r.error = "Cancelled";
  } else if (!res.error.empty() && r.error.empty()) {
    r.ok = false;
    r.error = res.error;
  }
  if (!r.ok) Log(std::string(providerName) + " stream failed: " + r.error);
  return r;
}

// ---------------------------------------------------------------------------

class AnthropicProvider : public Provider {
 public:
  AnthropicProvider(std::string base, std::string key) : base_(std::move(base)), key_(std::move(key)) {}

  Headers Auth() const { return {{"x-api-key", key_}, {"anthropic-version", "2023-06-01"}}; }

  ModelListResult ListModels(CancelToken& cancel) override {
    ModelListResult out;
    std::string after;
    for (int page = 0; page < 10; ++page) {
      HttpRequest req{"GET", base_ + "/v1/models?limit=1000" + (after.empty() ? "" : "&after_id=" + after), Auth()};
      HttpResponse res = HttpSend(req, &cancel);
      if (!res.Ok()) {
        out.error = DescribeHttpError(res);
        return out;
      }
      json j = json::parse(res.body, nullptr, false);
      for (auto& m : ParseAnthropicModels(j)) out.models.push_back(std::move(m));
      if (!j.value("has_more", false)) break;
      after = j.value("last_id", "");
      if (after.empty()) break;
    }
    return out;
  }

  void Prepare(const ChatRequest&, const ModelInfo&) override {
    if (!base_.empty()) HttpPreconnect(base_);
  }

  StreamResult Stream(const ChatRequest& request, const ModelInfo& model, const StreamSink& sink,
                      CancelToken& cancel) override {
    bool fallbacks = AnthropicSupportsDefaultFallback(request.model);
    for (int attempt = 0; attempt < 2; ++attempt) {
      json body = BuildAnthropicBody(request, model, fallbacks);
      HttpRequest req{"POST", base_ + "/v1/messages", Auth(), body.dump()};
      if (fallbacks) req.headers.push_back({"anthropic-beta", "server-side-fallback-2026-07-01"});
      Log("anthropic: POST /v1/messages model=" + request.model + " bytes=" + std::to_string(req.body.size()));
      AnthropicStream parser(sink);
      HttpResponse res = StreamSse(req, cancel, [&](const std::string&, const std::string& data) {
        FeedJson(data, [&](const json& j) { parser.OnEvent(j); });
      });
      if (!res.Ok() && !res.cancelled && res.error.empty()) {
        // Endpoints that don't know the fallback beta (proxies, older gateways): retry without it.
        if (fallbacks && res.status == 400 && (Contains(res.body, "fallback") || Contains(res.body, "beta"))) {
          fallbacks = false;
          continue;
        }
        return HttpFailure(res);
      }
      return Finalize(parser.Finish(), res, "anthropic");
    }
    return StreamResult{false, false, "Request failed"};
  }

 private:
  std::string base_, key_;
};

// ---------------------------------------------------------------------------

class OpenAIProvider : public Provider {
 public:
  OpenAIProvider(std::string base, std::string key) : base_(std::move(base)), key_(std::move(key)) {}

  ModelListResult ListModels(CancelToken& cancel) override {
    ModelListResult out;
    HttpResponse res = HttpSend({"GET", base_ + "/models", {{"Authorization", "Bearer " + key_}}}, &cancel);
    if (!res.Ok()) {
      out.error = DescribeHttpError(res);
      return out;
    }
    out.models = ParseOpenAIStyleModels(json::parse(res.body, nullptr, false), "openai");
    return out;
  }

  void Prepare(const ChatRequest&, const ModelInfo&) override {
    if (!base_.empty()) HttpPreconnect(base_);
  }

  StreamResult Stream(const ChatRequest& request, const ModelInfo& model, const StreamSink& sink,
                      CancelToken& cancel) override {
    json body = BuildOpenAIResponsesBody(request, model);
    HttpRequest req{"POST", base_ + "/responses", {{"Authorization", "Bearer " + key_}}, body.dump()};
    Log("openai: POST /responses model=" + request.model + " bytes=" + std::to_string(req.body.size()));
    OpenAIResponsesStream parser(sink);
    HttpResponse res = StreamSse(req, cancel, [&](const std::string&, const std::string& data) {
      FeedJson(data, [&](const json& j) { parser.OnEvent(j); });
    });
    if (!res.Ok() && !res.cancelled && res.error.empty()) return HttpFailure(res);
    return Finalize(parser.Finish(), res, "openai");
  }

 private:
  std::string base_, key_;
};

// ---------------------------------------------------------------------------

class CompatProvider : public Provider {
 public:
  CompatProvider(std::string id, std::string base, std::string key)
      : id_(std::move(id)), base_(std::move(base)), key_(std::move(key)) {}

  Headers BaseHeaders() const {
    Headers h;
    if (!key_.empty()) h.push_back({"Authorization", "Bearer " + key_});
    if (id_ == "openrouter") {
      h.push_back({"HTTP-Referer", "https://github.com/aiassistant"});
      h.push_back({"X-Title", "AIAssistant"});
    }
    return h;
  }

  ModelListResult ListModels(CancelToken& cancel) override {
    ModelListResult out;
    if (base_.empty()) {
      out.error = "Set a base URL for this endpoint first";
      return out;
    }
    HttpRequest listRequest{"GET", base_ + "/models", BaseHeaders()};
    listRequest.receiveTimeoutMs = listRequest.connectTimeoutMs = listRequest.sendTimeoutMs = 10000;
    HttpResponse res = HttpSend(listRequest, &cancel);
    if (!res.Ok()) {
      out.error = DescribeHttpError(res);
      if (id_ == "ollama" || id_ == "lmstudio") {
        if (!res.error.empty()) out.error = "Can't reach " + base_ + " — is the server running?";
      }
      return out;
    }
    out.models = ParseOpenAIStyleModels(json::parse(res.body, nullptr, false), id_);
    return out;
  }

  void Prepare(const ChatRequest&, const ModelInfo&) override {
    if (!base_.empty()) HttpPreconnect(base_);
  }

  StreamResult Stream(const ChatRequest& request, const ModelInfo& model, const StreamSink& sink,
                      CancelToken& cancel) override {
    if (base_.empty()) return StreamResult{false, false, "Set a base URL for this endpoint in Settings"};
    json body = BuildChatCompletionsBody(request, model, id_);
    HttpRequest req{"POST", base_ + "/chat/completions", BaseHeaders(), body.dump()};
    Log(id_ + ": POST /chat/completions model=" + request.model + " bytes=" + std::to_string(req.body.size()));
    ChatCompletionsStream parser(sink);
    HttpResponse res = StreamSse(req, cancel, [&](const std::string&, const std::string& data) { parser.OnData(data); });
    if (!res.Ok() && !res.cancelled && res.error.empty()) return HttpFailure(res);
    return Finalize(parser.Finish(), res, id_.c_str());
  }

 private:
  std::string id_, base_, key_;
};

// ---------------------------------------------------------------------------

class GeminiProvider : public Provider {
 public:
  GeminiProvider(std::string base, std::string key) : base_(std::move(base)), key_(std::move(key)) {}

  ModelListResult ListModels(CancelToken& cancel) override {
    ModelListResult out;
    HttpResponse res = HttpSend({"GET", base_ + "/models?pageSize=1000", {{"x-goog-api-key", key_}}}, &cancel);
    if (!res.Ok()) {
      out.error = DescribeHttpError(res);
      return out;
    }
    out.models = ParseGeminiModels(json::parse(res.body, nullptr, false));
    return out;
  }

  void Prepare(const ChatRequest&, const ModelInfo&) override {
    if (!base_.empty()) HttpPreconnect(base_);
  }

  StreamResult Stream(const ChatRequest& request, const ModelInfo& model, const StreamSink& sink,
                      CancelToken& cancel) override {
    json body = BuildGeminiBody(request, model);
    HttpRequest req{"POST", base_ + "/models/" + request.model + ":streamGenerateContent?alt=sse",
                    {{"x-goog-api-key", key_}}, body.dump()};
    Log("gemini: POST streamGenerateContent model=" + request.model + " bytes=" + std::to_string(req.body.size()));
    GeminiStream parser(sink);
    HttpResponse res = StreamSse(req, cancel, [&](const std::string&, const std::string& data) {
      FeedJson(data, [&](const json& j) { parser.OnEvent(j); });
    });
    if (!res.Ok() && !res.cancelled && res.error.empty()) return HttpFailure(res);
    return Finalize(parser.Finish(), res, "gemini");
  }

 private:
  std::string base_, key_;
};

}  // namespace

std::unique_ptr<Provider> MakeAnthropicProvider(std::string baseUrl, std::string key) {
  return std::make_unique<AnthropicProvider>(std::move(baseUrl), std::move(key));
}
std::unique_ptr<Provider> MakeOpenAIProvider(std::string baseUrl, std::string key) {
  return std::make_unique<OpenAIProvider>(std::move(baseUrl), std::move(key));
}
std::unique_ptr<Provider> MakeCompatProvider(std::string providerId, std::string baseUrl, std::string key) {
  return std::make_unique<CompatProvider>(std::move(providerId), std::move(baseUrl), std::move(key));
}
std::unique_ptr<Provider> MakeGeminiProvider(std::string baseUrl, std::string key) {
  return std::make_unique<GeminiProvider>(std::move(baseUrl), std::move(key));
}

}  // namespace aia
