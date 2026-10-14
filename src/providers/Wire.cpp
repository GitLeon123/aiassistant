#include "providers/Wire.h"

#include <algorithm>

#include "providers/Catalog.h"

namespace aia {
namespace {

int BudgetFor(const std::string& level) {
  if (level == "low") return 2048;
  if (level == "medium") return 8192;
  if (level == "high") return 24576;
  return 0;
}

std::string EffectiveLevel(const ChatRequest& req, const ModelInfo& model) {
  const auto& levels = model.reasoning.levels;
  if (!req.reasoning.empty() && std::find(levels.begin(), levels.end(), req.reasoning) != levels.end()) {
    return req.reasoning;
  }
  return model.reasoning.defaultLevel;
}

std::string DataUrl(const ImageData& img) { return "data:" + img.mime + ";base64," + img.base64; }

}  // namespace

void StreamParser::EmitText(std::string_view s) {
  if (s.empty()) return;
  sawText_ = true;
  text_.append(s);
  if (sink_.text) sink_.text(s);
}

void StreamParser::EmitReasoning(std::string_view s) {
  if (s.empty()) return;
  if (sink_.reasoning) sink_.reasoning(s);
}

// ============================================================================
// Anthropic
// ============================================================================

bool AnthropicSupportsDefaultFallback(const std::string& model) {
  for (const char* m : {"claude-fable-5-1", "claude-opus-5-5", "claude-opus-5", "claude-sonnet-5-5"}) {
    if (model == m) return true;
  }
  return false;
}

json BuildAnthropicBody(const ChatRequest& req, const ModelInfo& model, bool serverFallbacks) {
  json messages = json::array();
  for (const auto& m : req.messages) {
    if (m.role == "assistant") {
      if (m.rawProvider == "anthropic" && m.raw.is_array() && !m.raw.empty()) {
        messages.push_back({{"role", "assistant"}, {"content", m.raw}});
      } else if (!m.text.empty()) {
        messages.push_back({{"role", "assistant"}, {"content", json::array({{{"type", "text"}, {"text", m.text}}})}});
      }
      continue;
    }
    json content = json::array();
    if (m.image) {
      content.push_back({{"type", "image"},
                         {"source", {{"type", "base64"}, {"media_type", m.image->mime}, {"data", m.image->base64}}}});
    }
    content.push_back({{"type", "text"}, {"text", m.text.empty() ? std::string("(see screenshot)") : m.text}});
    messages.push_back({{"role", "user"}, {"content", content}});
  }

  json body = {{"model", req.model}, {"stream", true}, {"messages", messages}};
  if (!req.system.empty()) body["system"] = req.system;

  std::string level = EffectiveLevel(req, model);
  int maxTokens = 8192;
  if (model.reasoning.mode == "effort") {
    body["thinking"] = {{"type", "adaptive"}, {"display", "summarized"}};
    if (!level.empty()) body["output_config"] = {{"effort", level}};
    maxTokens = 32000;
  } else if (model.reasoning.mode == "budget") {
    int budget = BudgetFor(level);
    if (budget > 0) {
      body["thinking"] = {{"type", "enabled"}, {"budget_tokens", budget}};
      maxTokens = budget + 16000;
    }
  }
  body["max_tokens"] = maxTokens;
  if (serverFallbacks && AnthropicSupportsDefaultFallback(req.model)) body["fallbacks"] = "default";
  return body;
}

std::vector<ModelInfo> ParseAnthropicModels(const json& listing) {
  std::vector<ModelInfo> out;
  if (!listing.contains("data") || !listing["data"].is_array()) return out;
  for (const auto& m : listing["data"]) {
    std::string id = m.value("id", "");
    if (id.empty()) continue;
    ModelInfo info = InferModel("anthropic", id);
    info.name = m.value("display_name", info.name);
    if (m.contains("capabilities") && m["capabilities"].is_object()) {
      const json& caps = m["capabilities"];
      info.vision = JsonGet<bool>(caps, "image_input/supported", true);
      bool adaptive = JsonGet<bool>(caps, "thinking/types/adaptive/supported", false);
      bool enabled = JsonGet<bool>(caps, "thinking/types/enabled/supported", false);
      std::vector<std::string> levels;
      for (const char* l : {"low", "medium", "high", "xhigh", "max"}) {
        if (JsonGet<bool>(caps, std::string("effort/") + l + "/supported", false)) levels.push_back(l);
      }
      if (adaptive && !levels.empty()) {
        std::string def = info.reasoning.defaultLevel;
        if (std::find(levels.begin(), levels.end(), def) == levels.end()) {
          def = std::find(levels.begin(), levels.end(), "high") != levels.end() ? "high" : levels.back();
        }
        info.reasoning = ReasoningSpec{"effort", levels, def};
      } else if (enabled) {
        info.reasoning = ReasoningSpec{"budget", {"off", "low", "medium", "high"}, "off"};
      } else if (JsonGet<bool>(caps, "thinking/supported", false) == false) {
        info.reasoning = ReasoningSpec{};
      }
    }
    out.push_back(std::move(info));
  }
  return out;
}

void AnthropicStream::OnEvent(const json& data) {
  std::string type = data.value("type", "");
  if (type == "message_start") {
    if (data.contains("message")) result_.usage = data["message"].value("usage", json::object());
  } else if (type == "content_block_start") {
    size_t index = data.value("index", blocks_.size());
    if (blocks_.size() <= index) blocks_.resize(index + 1);
    blocks_[index] = data.value("content_block", json::object());
    const json& b = blocks_[index];
    if (b.value("type", "") == "text") EmitText(b.value("text", ""));
    if (b.value("type", "") == "thinking") EmitReasoning(b.value("thinking", ""));
  } else if (type == "content_block_delta") {
    size_t index = data.value("index", size_t{0});
    if (blocks_.size() <= index) blocks_.resize(index + 1, json::object());
    json& block = blocks_[index];
    const json delta = data.value("delta", json::object());
    std::string dt = delta.value("type", "");
    if (dt == "text_delta") {
      std::string t = delta.value("text", "");
      block["text"] = block.value("text", "") + t;
      EmitText(t);
    } else if (dt == "thinking_delta") {
      std::string t = delta.value("thinking", "");
      block["thinking"] = block.value("thinking", "") + t;
      EmitReasoning(t);
    } else if (dt == "signature_delta") {
      block["signature"] = block.value("signature", "") + delta.value("signature", "");
    }
  } else if (type == "message_delta") {
    if (data.contains("delta")) {
      const json& d = data["delta"];
      if (d.contains("stop_reason") && d["stop_reason"].is_string()) result_.stopReason = d["stop_reason"];
      if (d.contains("stop_details") && d["stop_details"].is_object()) stopDetails_ = d["stop_details"];
    }
    if (data.contains("usage") && data["usage"].is_object()) {
      if (!result_.usage.is_object()) result_.usage = json::object();
      result_.usage.update(data["usage"]);
    }
  } else if (type == "error") {
    Fail(JsonGet<std::string>(data, "error/message", "The provider reported an error"));
  }
}

StreamResult AnthropicStream::Finish() {
  if (result_.stopReason == "refusal") {
    std::string category = stopDetails_.is_object() ? stopDetails_.value("category", "") : "";
    std::string msg = "The model declined to answer this request";
    if (!category.empty()) msg += " (" + category + ")";
    if (!sawText_) Fail(msg);
  }
  // Build replayable content. After a mid-output fallback, thinking blocks before the last
  // fallback marker must not be echoed back; the markers themselves are dropped.
  ptrdiff_t lastFallback = -1;
  for (size_t i = 0; i < blocks_.size(); ++i) {
    if (blocks_[i].value("type", "") == "fallback") lastFallback = static_cast<ptrdiff_t>(i);
  }
  json raw = json::array();
  for (size_t i = 0; i < blocks_.size(); ++i) {
    const json& b = blocks_[i];
    std::string t = b.value("type", "");
    if (t.empty() || t == "fallback") continue;
    bool beforeFallback = static_cast<ptrdiff_t>(i) < lastFallback;
    if (beforeFallback && t != "text") continue;
    if (t == "text" && b.value("text", "").empty()) continue;
    if (t != "text" && t != "thinking" && t != "redacted_thinking") continue;
    raw.push_back(b);
  }
  result_.raw = raw;
  result_.ok = result_.error.empty();
  return result_;
}

// ============================================================================
// OpenAI Responses
// ============================================================================

json BuildOpenAIResponsesBody(const ChatRequest& req, const ModelInfo& model) {
  json input = json::array();
  for (const auto& m : req.messages) {
    if (m.role == "assistant") {
      if (m.text.empty()) continue;
      input.push_back({{"role", "assistant"}, {"content", json::array({{{"type", "output_text"}, {"text", m.text}}})}});
      continue;
    }
    json content = json::array();
    content.push_back({{"type", "input_text"}, {"text", m.text.empty() ? std::string("(see screenshot)") : m.text}});
    if (m.image) content.push_back({{"type", "input_image"}, {"image_url", DataUrl(*m.image)}});
    input.push_back({{"role", "user"}, {"content", content}});
  }
  json body = {{"model", req.model}, {"input", input}, {"stream", true}, {"store", false}};
  if (!req.system.empty()) body["instructions"] = req.system;
  if (model.reasoning.mode == "effort") {
    std::string level = EffectiveLevel(req, model);
    json reasoning = {{"summary", "auto"}};
    if (!level.empty()) reasoning["effort"] = level;
    body["reasoning"] = reasoning;
  }
  return body;
}

void OpenAIResponsesStream::OnEvent(const json& data) {
  std::string type = data.value("type", "");
  if (type == "response.output_text.delta") {
    EmitText(data.value("delta", ""));
  } else if (type == "response.reasoning_summary_text.delta" || type == "response.reasoning_text.delta") {
    EmitReasoning(data.value("delta", ""));
    reasoningStarted_ = true;
  } else if (type == "response.reasoning_summary_part.added") {
    if (reasoningStarted_) EmitReasoning("\n\n");
  } else if (type == "response.completed") {
    result_.usage = JsonGet<json>(data, "response/usage", json::object());
    result_.stopReason = "completed";
  } else if (type == "response.incomplete") {
    result_.stopReason = JsonGet<std::string>(data, "response/incomplete_details/reason", "incomplete");
    if (!sawText_) Fail("The response was incomplete (" + result_.stopReason + ")");
  } else if (type == "response.failed") {
    Fail(JsonGet<std::string>(data, "response/error/message", "The response failed"));
  } else if (type == "error") {
    std::string msg = data.value("message", "");
    if (msg.empty()) msg = JsonGet<std::string>(data, "error/message", "The provider reported an error");
    Fail(msg);
  }
}

StreamResult OpenAIResponsesStream::Finish() {
  result_.ok = result_.error.empty();
  return result_;
}

// ============================================================================
// OpenAI-compatible Chat Completions
// ============================================================================

json BuildChatCompletionsBody(const ChatRequest& req, const ModelInfo& model, const std::string& providerId) {
  json messages = json::array();
  if (!req.system.empty()) messages.push_back({{"role", "system"}, {"content", req.system}});
  for (const auto& m : req.messages) {
    if (m.role == "assistant") {
      if (!m.text.empty()) messages.push_back({{"role", "assistant"}, {"content", m.text}});
      continue;
    }
    std::string text = m.text.empty() ? std::string("(see screenshot)") : m.text;
    if (m.image && model.vision) {
      messages.push_back({{"role", "user"},
                          {"content", json::array({{{"type", "text"}, {"text", text}},
                                                   {{"type", "image_url"}, {"image_url", {{"url", DataUrl(*m.image)}}}}})}});
    } else {
      messages.push_back({{"role", "user"}, {"content", text}});
    }
  }
  json body = {{"model", req.model}, {"messages", messages}, {"stream", true}};
  if (model.reasoning.mode == "effort") {
    std::string level = EffectiveLevel(req, model);
    if (!level.empty()) {
      if (providerId == "openrouter") body["reasoning"] = {{"effort", level}};
      else body["reasoning_effort"] = level;
    }
  }
  return body;
}

std::vector<ModelInfo> ParseOpenAIStyleModels(const json& listing, const std::string& providerId) {
  struct Entry {
    ModelInfo info;
    int64_t created;
  };
  std::vector<Entry> entries;
  const json& data = listing.contains("data") ? listing["data"] : (listing.contains("models") ? listing["models"] : listing);
  if (!data.is_array()) return {};
  for (const auto& m : data) {
    std::string id = m.is_string() ? m.get<std::string>() : m.value("id", "");
    if (id.empty()) continue;
    std::string lower = ToLower(id);
    bool skip = false;
    for (const char* bad : {"embed", "whisper", "tts", "dall-e", "davinci", "babbage", "moderation", "realtime",
                            "audio", "transcribe", "image", "sora", "guard", "rerank", "search-preview"}) {
      if (Contains(lower, bad)) skip = true;
    }
    if (providerId == "openai" && !(StartsWith(lower, "gpt-") || StartsWith(lower, "o1") || StartsWith(lower, "o3") ||
                                    StartsWith(lower, "o4") || StartsWith(lower, "chatgpt"))) {
      skip = true;
    }
    if (skip) continue;
    ModelInfo info = InferModel(providerId, id);
    if (m.is_object()) {
      if (m.contains("name") && m["name"].is_string()) info.name = m["name"];
      if (providerId == "openrouter") {
        if (auto it = m.find("supported_parameters"); it != m.end() && it->is_array()) {
          bool reasoning = std::find(it->begin(), it->end(), "reasoning") != it->end();
          if (reasoning && info.reasoning.mode == "none") {
            info.reasoning = ReasoningSpec{"effort", {"low", "medium", "high"}, "medium"};
          } else if (!reasoning) {
            info.reasoning = ReasoningSpec{};
          }
        }
        if (auto mods = JsonGet<json>(m, "architecture/input_modalities", json()); mods.is_array()) {
          info.vision = std::find(mods.begin(), mods.end(), "image") != mods.end();
        }
      }
    }
    int64_t created = m.is_object() ? m.value("created", int64_t{0}) : 0;
    entries.push_back({std::move(info), created});
  }
  std::stable_sort(entries.begin(), entries.end(), [&](const Entry& a, const Entry& b) {
    if (a.created != b.created) return a.created > b.created;
    return a.info.id < b.info.id;
  });
  std::vector<ModelInfo> out;
  for (auto& e : entries) out.push_back(std::move(e.info));
  return out;
}

void ChatCompletionsStream::OnData(const std::string& data) {
  if (data == "[DONE]") {
    if (result_.stopReason.empty()) result_.stopReason = "stop";
    return;
  }
  json j;
  try {
    j = json::parse(data);
  } catch (...) {
    return;
  }
  if (j.contains("error")) {
    const json& e = j["error"];
    Fail(e.is_object() ? e.value("message", e.dump()) : e.dump());
    return;
  }
  if (j.contains("usage") && j["usage"].is_object()) result_.usage = j["usage"];
  if (!j.contains("choices") || !j["choices"].is_array() || j["choices"].empty()) return;
  const json& choice = j["choices"][0];
  if (choice.contains("delta")) {
    const json& d = choice["delta"];
    for (const char* key : {"reasoning", "reasoning_content"}) {
      if (d.contains(key) && d[key].is_string()) EmitReasoning(d[key].get<std::string>());
    }
    if (d.contains("content") && d["content"].is_string()) RouteContent(d["content"].get<std::string>());
  }
  if (choice.contains("finish_reason") && choice["finish_reason"].is_string()) {
    result_.stopReason = choice["finish_reason"];
  }
}

void ChatCompletionsStream::RouteContent(std::string_view content) {
  pending_.append(content);
  for (;;) {
    if (think_ == ThinkState::Unknown) {
      size_t first = pending_.find_first_not_of(" \t\r\n");
      if (first == std::string::npos) return;  // only whitespace so far
      std::string_view rest(pending_.data() + first, pending_.size() - first);
      static constexpr std::string_view kOpen = "<think>";
      if (rest.size() < kOpen.size() && kOpen.substr(0, rest.size()) == rest) return;  // undecided yet
      if (StartsWith(rest, kOpen)) {
        think_ = ThinkState::Inside;
        pending_.erase(0, first + kOpen.size());
      } else {
        think_ = ThinkState::Outside;
      }
      continue;
    }
    if (think_ == ThinkState::Inside) {
      static constexpr std::string_view kClose = "</think>";
      size_t end = pending_.find(kClose);
      if (end == std::string::npos) {
        // Hold back a possible partial closing tag.
        size_t keep = std::min(pending_.size(), kClose.size() - 1);
        size_t emit = pending_.size() - keep;
        EmitReasoning(std::string_view(pending_.data(), emit));
        pending_.erase(0, emit);
        return;
      }
      EmitReasoning(std::string_view(pending_.data(), end));
      pending_.erase(0, end + kClose.size());
      size_t nonWs = pending_.find_first_not_of(" \t\r\n");
      pending_.erase(0, nonWs == std::string::npos ? pending_.size() : nonWs);
      think_ = ThinkState::Outside;
      continue;
    }
    EmitText(pending_);
    pending_.clear();
    return;
  }
}

StreamResult ChatCompletionsStream::Finish() {
  if (!pending_.empty()) {
    if (think_ == ThinkState::Inside) EmitReasoning(pending_);
    else EmitText(pending_);
    pending_.clear();
  }
  result_.ok = result_.error.empty();
  return result_;
}

// ============================================================================
// Gemini
// ============================================================================

json BuildGeminiBody(const ChatRequest& req, const ModelInfo& model) {
  json contents = json::array();
  for (const auto& m : req.messages) {
    if (m.role == "assistant") {
      if (!m.text.empty()) contents.push_back({{"role", "model"}, {"parts", json::array({{{"text", m.text}}})}});
      continue;
    }
    json parts = json::array();
    if (m.image) parts.push_back({{"inline_data", {{"mime_type", m.image->mime}, {"data", m.image->base64}}}});
    parts.push_back({{"text", m.text.empty() ? std::string("(see screenshot)") : m.text}});
    contents.push_back({{"role", "user"}, {"parts", parts}});
  }
  json body = {{"contents", contents}};
  if (!req.system.empty()) body["systemInstruction"] = {{"parts", json::array({{{"text", req.system}}})}};
  std::string level = EffectiveLevel(req, model);
  if (model.reasoning.mode == "level" && !level.empty()) {
    body["generationConfig"] = {{"thinkingConfig", {{"thinkingLevel", level}, {"includeThoughts", true}}}};
  } else if (model.reasoning.mode == "budget") {
    int budget = BudgetFor(level);
    if (level == "off") {
      body["generationConfig"] = {{"thinkingConfig", {{"thinkingBudget", 0}}}};
    } else {
      body["generationConfig"] = {
          {"thinkingConfig", {{"thinkingBudget", budget > 0 ? budget : -1}, {"includeThoughts", true}}}};
    }
  }
  return body;
}

std::vector<ModelInfo> ParseGeminiModels(const json& listing) {
  std::vector<ModelInfo> out;
  if (!listing.contains("models") || !listing["models"].is_array()) return out;
  for (const auto& m : listing["models"]) {
    std::string name = m.value("name", "");
    std::string id = StartsWith(name, "models/") ? name.substr(7) : name;
    if (id.empty()) continue;
    bool generates = false;
    if (auto it = m.find("supportedGenerationMethods"); it != m.end() && it->is_array()) {
      generates = std::find(it->begin(), it->end(), "generateContent") != it->end();
    }
    if (!generates) continue;
    std::string lower = ToLower(id);
    bool skip = false;
    for (const char* bad : {"embedding", "aqa", "imagen", "veo", "-tts", "image", "live", "robotics", "learnlm"}) {
      if (Contains(lower, bad)) skip = true;
    }
    if (skip) continue;
    ModelInfo info = InferModel("gemini", id);
    info.name = m.value("displayName", info.name);
    if (info.reasoning.mode == "none" && m.value("thinking", false)) {
      info.reasoning = ReasoningSpec{"budget", {"low", "medium", "high"}, "medium"};
    }
    out.push_back(std::move(info));
  }
  // Newest families first: sort by id descending so gemini-3 precedes gemini-2.5.
  std::stable_sort(out.begin(), out.end(), [](const ModelInfo& a, const ModelInfo& b) { return a.id > b.id; });
  return out;
}

void GeminiStream::OnEvent(const json& data) {
  if (data.contains("error")) {
    Fail(JsonGet<std::string>(data, "error/message", "The provider reported an error"));
    return;
  }
  if (data.contains("usageMetadata")) result_.usage = data["usageMetadata"];
  blockReason_ = JsonGet<std::string>(data, "promptFeedback/blockReason", blockReason_);
  if (!data.contains("candidates") || !data["candidates"].is_array() || data["candidates"].empty()) return;
  const json& c = data["candidates"][0];
  if (c.contains("finishReason") && c["finishReason"].is_string()) finishReason_ = c["finishReason"];
  const json parts = JsonGet<json>(c, "content/parts", json::array());
  for (const auto& p : parts) {
    if (!p.contains("text") || !p["text"].is_string()) continue;
    if (p.value("thought", false)) EmitReasoning(p["text"].get<std::string>());
    else EmitText(p["text"].get<std::string>());
  }
}

StreamResult GeminiStream::Finish() {
  result_.stopReason = finishReason_;
  if (!sawText_ && result_.error.empty()) {
    if (!blockReason_.empty()) Fail("The prompt was blocked (" + blockReason_ + ")");
    else if (!finishReason_.empty() && finishReason_ != "STOP") Fail("No answer (" + finishReason_ + ")");
  }
  result_.ok = result_.error.empty();
  return result_;
}

// ============================================================================
// CLI bridges
// ============================================================================

std::string FlattenConversation(const ChatRequest& req, bool includeSystem) {
  std::string out;
  if (includeSystem && !req.system.empty()) out += "<instructions>\n" + req.system + "\n</instructions>\n\n";
  if (req.messages.empty()) return out;
  if (req.messages.size() > 1) {
    out += "<conversation_so_far>\n";
    for (size_t i = 0; i + 1 < req.messages.size(); ++i) {
      const auto& m = req.messages[i];
      out += m.role == "assistant" ? "Assistant: " : "User: ";
      out += m.text;
      if (m.image) out += "\n[a screenshot was attached to this message]";
      out += "\n\n";
    }
    out += "</conversation_so_far>\n\nReply to the user's latest message:\n";
  }
  const auto& last = req.messages.back();
  out += last.text.empty() ? std::string("(see the attached screenshot)") : last.text;
  return out;
}

std::string BuildClaudeCliInput(const ChatRequest& req) {
  json content = json::array();
  if (!req.messages.empty() && req.messages.back().image) {
    const auto& img = *req.messages.back().image;
    content.push_back({{"type", "image"}, {"source", {{"type", "base64"}, {"media_type", img.mime}, {"data", img.base64}}}});
  }
  content.push_back({{"type", "text"}, {"text", FlattenConversation(req, false)}});
  json line = {{"type", "user"}, {"message", {{"role", "user"}, {"content", content}}}};
  return line.dump() + "\n";
}

void ClaudeCliStream::OnLine(std::string_view line) {
  json j;
  try {
    j = json::parse(line);
  } catch (...) {
    return;
  }
  std::string type = j.value("type", "");
  if (type == "stream_event" && j.contains("event")) {
    inner_.OnEvent(j["event"]);
  } else if (type == "rate_limit_event") {
    auto status = JsonGet<std::string>(j, "rate_limit_info/status", "");
    if (status != "allowed" && status != "allowed_warning" && status != "rejected") return;
    result_.limitWarning = status == "allowed_warning" ? "Claude reports that account usage is nearing a limit. Check /usage in Claude Code."
      : status == "rejected" ? "Claude reports an account usage limit. Check /usage in Claude Code for the reset time." : "";
    if (sink_.limit) sink_.limit(result_.limitWarning);
  } else if (type == "result" || j.contains("duration_api_ms")) {
    gotResult_ = true;
    if (j.contains("usage")) result_.usage = j["usage"];
    bool isError = j.value("is_error", false) || (j.contains("subtype") && j["subtype"] != "success");
    std::string text = j.contains("result") && j["result"].is_string() ? j["result"].get<std::string>() : "";
    if (isError) Fail(text.empty() ? "Claude Code reported an error" : text);
    else resultText_ = text;
  } else if (type == "system" && j.value("subtype", "") == "init") {
    if (sink_.status) sink_.status("Claude Code connected");
  }
}

StreamResult ClaudeCliStream::Finish() {
  StreamResult inner = inner_.Finish();
  if (!inner_.SawOutput() && !resultText_.empty()) EmitText(resultText_);
  if (!inner.error.empty()) Fail(inner.error);
  if (!gotResult_ && !inner_.SawOutput() && !sawText_) Fail("Claude Code exited without a response");
  result_.stopReason = inner.stopReason;
  result_.ok = result_.error.empty();
  return result_;
}

void CodexCliStream::Item(const json& item) {
  std::string id = item.value("id", "");
  std::string type = item.value("type", "");
  std::string text = item.value("text", "");
  if (type != "agent_message" && type != "reasoning") return;
  size_t already = emitted_.value(id, size_t{0});
  if (text.size() <= already) return;
  std::string_view fresh(text.data() + already, text.size() - already);
  emitted_[id] = text.size();
  if (type == "agent_message") {
    if (sawText_ && already == 0) EmitText("\n\n");
    EmitText(fresh);
  } else {
    if (reasoningStarted_ && already == 0) EmitReasoning("\n\n");
    EmitReasoning(fresh);
    reasoningStarted_ = true;
  }
}

void CodexCliStream::OnLine(std::string_view line) {
  json j;
  try {
    j = json::parse(line);
  } catch (...) {
    return;
  }
  std::string type = j.value("type", "");
  if ((type == "item.started" || type == "item.updated" || type == "item.completed") && j.contains("item")) {
    Item(j["item"]);
  } else if (type == "turn.completed") {
    done_ = true;
    result_.usage = j.value("usage", json::object());
    result_.stopReason = "completed";
  } else if (type == "turn.failed") {
    done_ = true;
    Fail(JsonGet<std::string>(j, "error/message", "Codex failed"));
  } else if (type == "error") {
    Fail(j.value("message", "Codex reported an error"));
  } else if (type == "thread.started") {
    if (sink_.status) sink_.status("Codex connected");
  }
}

StreamResult CodexCliStream::Finish() {
  if (!sawText_ && result_.error.empty()) Fail("Codex exited without a response");
  result_.ok = result_.error.empty();
  result_.streamed = false;  // codex exec --json emits each message only once it is complete
  return result_;
}

void GeminiCliStream::OnLine(std::string_view line) {
  json j;
  try {
    j = json::parse(line);
  } catch (...) {
    return;
  }
  std::string type = j.value("type", "");
  if (type == "message" && j.value("role", "") == "assistant") {
    if (j.contains("content") && j["content"].is_string()) EmitText(j["content"].get<std::string>());
  } else if (type == "thought" || type == "thinking") {
    std::string t = j.contains("content") && j["content"].is_string() ? j["content"].get<std::string>() : j.value("text", "");
    EmitReasoning(t);
  } else if (type == "result") {
    done_ = true;
    if (j.contains("stats")) result_.usage = j["stats"];
    if (j.value("status", "success") != "success") {
      Fail(JsonGet<std::string>(j, "error/message", "Gemini CLI reported an error"));
    }
  } else if (type == "error") {
    std::string msg = j.value("message", "");
    if (msg.empty()) msg = JsonGet<std::string>(j, "error/message", "Gemini CLI reported an error");
    Fail(msg);
  }
}

StreamResult GeminiCliStream::Finish() {
  if (!sawText_ && result_.error.empty()) Fail("Gemini CLI exited without a response");
  result_.ok = result_.error.empty();
  return result_;
}

}  // namespace aia
