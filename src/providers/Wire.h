#pragma once

// Pure request builders and stream parsers for every provider wire format.
// No I/O here, so everything is unit-testable with recorded fixtures.

#include <string>
#include <vector>

#include "providers/Types.h"

namespace aia {

// Base for incremental stream parsers: forwards deltas to the sink and accumulates a result.
class StreamParser {
 public:
  explicit StreamParser(const StreamSink& sink) : sink_(sink) {}
  virtual ~StreamParser() = default;
  const StreamResult& Result() const { return result_; }
  bool SawOutput() const { return sawText_; }
  std::string Text() const { return text_; }

 protected:
  void EmitText(std::string_view s);
  void EmitReasoning(std::string_view s);
  void Fail(std::string message) {
    if (result_.error.empty()) result_.error = std::move(message);
  }

  const StreamSink& sink_;
  StreamResult result_;
  std::string text_;
  bool sawText_ = false;
};

// ---- Anthropic Messages API -------------------------------------------------
json BuildAnthropicBody(const ChatRequest& req, const ModelInfo& model, bool serverFallbacks);
bool AnthropicSupportsDefaultFallback(const std::string& model);
std::vector<ModelInfo> ParseAnthropicModels(const json& listing);

class AnthropicStream : public StreamParser {
 public:
  using StreamParser::StreamParser;
  void OnEvent(const json& data);
  StreamResult Finish();

 private:
  std::vector<json> blocks_;
  json stopDetails_;
};

// ---- OpenAI Responses API ---------------------------------------------------
json BuildOpenAIResponsesBody(const ChatRequest& req, const ModelInfo& model);

class OpenAIResponsesStream : public StreamParser {
 public:
  using StreamParser::StreamParser;
  void OnEvent(const json& data);
  StreamResult Finish();

 private:
  bool reasoningStarted_ = false;
};

// ---- OpenAI-compatible Chat Completions -------------------------------------
json BuildChatCompletionsBody(const ChatRequest& req, const ModelInfo& model, const std::string& providerId);
std::vector<ModelInfo> ParseOpenAIStyleModels(const json& listing, const std::string& providerId);

class ChatCompletionsStream : public StreamParser {
 public:
  using StreamParser::StreamParser;
  // data is the raw SSE data payload ("[DONE]" or JSON)
  void OnData(const std::string& data);
  StreamResult Finish();

 private:
  void RouteContent(std::string_view content);
  // Splits inline <think>...</think> (local reasoning models) into reasoning.
  enum class ThinkState { Unknown, Inside, Outside } think_ = ThinkState::Unknown;
  std::string pending_;
};

// ---- Google Gemini ----------------------------------------------------------
json BuildGeminiBody(const ChatRequest& req, const ModelInfo& model);
std::vector<ModelInfo> ParseGeminiModels(const json& listing);

class GeminiStream : public StreamParser {
 public:
  using StreamParser::StreamParser;
  void OnEvent(const json& data);
  StreamResult Finish();

 private:
  std::string finishReason_;
  std::string blockReason_;
};

// ---- CLI bridges ------------------------------------------------------------
// Flattens multi-turn history into one prompt for stateless CLI invocations.
std::string FlattenConversation(const ChatRequest& req, bool includeSystem);

std::string BuildClaudeCliInput(const ChatRequest& req);

class ClaudeCliStream : public StreamParser {
 public:
  explicit ClaudeCliStream(const StreamSink& sink) : StreamParser(sink), inner_(sink) {}
  void OnLine(std::string_view line);
  bool Done() const { return gotResult_; }  // The final "result" event arrived
  StreamResult Finish();

 private:
  AnthropicStream inner_;
  bool gotResult_ = false;
  std::string resultText_;
};

class CodexCliStream : public StreamParser {
 public:
  using StreamParser::StreamParser;
  void OnLine(std::string_view line);
  bool Done() const { return done_; }  // turn.completed / turn.failed arrived
  StreamResult Finish();

 private:
  void Item(const json& item);
  bool done_ = false;
  json emitted_ = json::object();  // item id -> characters already emitted
  bool reasoningStarted_ = false;
};

class GeminiCliStream : public StreamParser {
 public:
  using StreamParser::StreamParser;
  void OnLine(std::string_view line);
  bool Done() const { return done_; }  // The final "result" event arrived
  StreamResult Finish();

 private:
  bool done_ = false;
};

}  // namespace aia
