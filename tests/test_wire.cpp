#include <doctest/doctest.h>

#include <string>

#include "providers/Catalog.h"
#include "providers/Registry.h"
#include "providers/Wire.h"

using namespace aia;

namespace {

struct Capture {
  std::string text, reasoning;
  StreamSink Sink() {
    StreamSink s;
    s.text = [this](std::string_view v) { text.append(v); };
    s.reasoning = [this](std::string_view v) { reasoning.append(v); };
    return s;
  }
};

ChatRequest Simple(const std::string& model, const std::string& reasoning = "") {
  ChatRequest r;
  r.model = model;
  r.reasoning = reasoning;
  r.system = "sys";
  ChatMessage m;
  m.role = "user";
  m.text = "hi";
  r.messages.push_back(m);
  return r;
}

ModelInfo Model(std::string mode, std::vector<std::string> levels, std::string def, bool vision = true) {
  return ModelInfo{"m", "M", vision, ReasoningSpec{std::move(mode), std::move(levels), std::move(def)}};
}

}  // namespace

// ---------------------------------------------------------------------------
// Anthropic

TEST_CASE("Anthropic body: adaptive thinking with effort") {
  auto req = Simple("claude-opus-5-5", "high");
  json b = BuildAnthropicBody(req, InferModel("anthropic", "claude-opus-5-5"), true);
  CHECK(b["thinking"]["type"] == "adaptive");
  CHECK(b["thinking"]["display"] == "summarized");
  CHECK(b["output_config"]["effort"] == "high");
  CHECK(b["max_tokens"] == 32000);
  CHECK(b["system"] == "sys");
  CHECK(b["stream"] == true);
  CHECK(b["fallbacks"] == "default");
}

TEST_CASE("Anthropic body: unknown level falls back to the model default; no fallbacks when disabled") {
  auto req = Simple("claude-opus-5-5", "bogus");
  json b = BuildAnthropicBody(req, InferModel("anthropic", "claude-opus-5-5"), false);
  CHECK(b["output_config"]["effort"] == "medium");
  CHECK_FALSE(b.contains("fallbacks"));
}

TEST_CASE("Anthropic body: budget thinking and off") {
  auto model = Model("budget", {"off", "low", "medium", "high"}, "off");
  json on = BuildAnthropicBody(Simple("claude-haiku-4-5", "medium"), model, true);
  CHECK(on["thinking"]["type"] == "enabled");
  CHECK(on["thinking"]["budget_tokens"] == 8192);
  CHECK(on["max_tokens"] == 8192 + 16000);
  CHECK_FALSE(on.contains("fallbacks"));  // haiku is not on the server-fallback list

  json off = BuildAnthropicBody(Simple("claude-haiku-4-5", "off"), model, true);
  CHECK_FALSE(off.contains("thinking"));
  CHECK(off["max_tokens"] == 8192);
}

TEST_CASE("Anthropic body: image block precedes text; raw assistant content is replayed") {
  ChatRequest req = Simple("claude-opus-5-5");
  req.messages[0].image = ImageData{"image/jpeg", "QUJD"};
  ChatMessage a;
  a.role = "assistant";
  a.text = "answer";
  a.raw = json::array({{{"type", "thinking"}, {"thinking", "t"}, {"signature", "sig"}}, {{"type", "text"}, {"text", "answer"}}});
  a.rawProvider = "anthropic";
  req.messages.push_back(a);
  ChatMessage u;
  u.role = "user";
  u.text = "more";
  req.messages.push_back(u);
  json b = BuildAnthropicBody(req, InferModel("anthropic", "claude-opus-5-5"), false);
  const json& msgs = b["messages"];
  REQUIRE(msgs.size() == 3);
  CHECK(msgs[0]["content"][0]["type"] == "image");
  CHECK(msgs[0]["content"][0]["source"]["media_type"] == "image/jpeg");
  CHECK(msgs[0]["content"][1]["text"] == "hi");
  CHECK(msgs[1]["content"][0]["signature"] == "sig");

  // Raw from a different provider is not replayed; plain text is used instead.
  req.messages[1].rawProvider = "openai";
  json b2 = BuildAnthropicBody(req, InferModel("anthropic", "claude-opus-5-5"), false);
  CHECK(b2["messages"][1]["content"][0]["type"] == "text");
  CHECK(b2["messages"][1]["content"][0]["text"] == "answer");
}

TEST_CASE("Anthropic stream: thinking, text, signature, usage") {
  Capture c;
  StreamSink sink = c.Sink();
  AnthropicStream s(sink);
  s.OnEvent(json::parse(R"({"type":"message_start","message":{"usage":{"input_tokens":10}}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":""}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_delta","index":0,"delta":{"type":"thinking_delta","thinking":"hmm"}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_delta","index":0,"delta":{"type":"signature_delta","signature":"abc"}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_start","index":1,"content_block":{"type":"text","text":""}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_delta","index":1,"delta":{"type":"text_delta","text":"Hel"}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_delta","index":1,"delta":{"type":"text_delta","text":"lo"}})"));
  s.OnEvent(json::parse(R"({"type":"message_delta","delta":{"stop_reason":"end_turn"},"usage":{"output_tokens":5}})"));
  StreamResult r = s.Finish();
  CHECK(r.ok);
  CHECK(c.text == "Hello");
  CHECK(c.reasoning == "hmm");
  CHECK(r.stopReason == "end_turn");
  CHECK(r.usage["input_tokens"] == 10);
  CHECK(r.usage["output_tokens"] == 5);
  REQUIRE(r.raw.size() == 2);
  CHECK(r.raw[0]["signature"] == "abc");
  CHECK(r.raw[1]["text"] == "Hello");
}

TEST_CASE("Anthropic stream: refusal without output is an error") {
  Capture c;
  StreamSink sink = c.Sink();
  AnthropicStream s(sink);
  s.OnEvent(json::parse(R"({"type":"message_delta","delta":{"stop_reason":"refusal","stop_details":{"category":"cyber"}}})"));
  StreamResult r = s.Finish();
  CHECK_FALSE(r.ok);
  CHECK(r.error.find("declined") != std::string::npos);
  CHECK(r.error.find("cyber") != std::string::npos);
}

TEST_CASE("Anthropic stream: thinking before a fallback marker is not replayed") {
  Capture c;
  StreamSink sink = c.Sink();
  AnthropicStream s(sink);
  s.OnEvent(json::parse(R"({"type":"content_block_start","index":0,"content_block":{"type":"thinking","thinking":"a","signature":"s1"}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_start","index":1,"content_block":{"type":"text","text":"partial "}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_start","index":2,"content_block":{"type":"fallback"}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_start","index":3,"content_block":{"type":"thinking","thinking":"b","signature":"s2"}})"));
  s.OnEvent(json::parse(R"({"type":"content_block_start","index":4,"content_block":{"type":"text","text":"rest"}})"));
  StreamResult r = s.Finish();
  REQUIRE(r.raw.size() == 3);
  CHECK(r.raw[0]["text"] == "partial ");
  CHECK(r.raw[1]["signature"] == "s2");
  CHECK(r.raw[2]["text"] == "rest");
}

TEST_CASE("Anthropic models: capabilities drive reasoning levels") {
  json listing = json::parse(R"({"data":[
    {"id":"claude-opus-5-5","display_name":"Claude Opus 5.5","capabilities":{
      "image_input":{"supported":true},
      "thinking":{"supported":true,"types":{"adaptive":{"supported":true},"enabled":{"supported":false}}},
      "effort":{"low":{"supported":true},"medium":{"supported":true},"high":{"supported":true},"xhigh":{"supported":true},"max":{"supported":true}}}},
    {"id":"claude-haiku-5-5","display_name":"Claude Haiku 5.5","capabilities":{
      "thinking":{"supported":true,"types":{"adaptive":{"supported":false},"enabled":{"supported":true}}}}}
  ]})");
  auto models = ParseAnthropicModels(listing);
  REQUIRE(models.size() == 2);
  CHECK(models[0].name == "Claude Opus 5.5");
  CHECK(models[0].reasoning.mode == "effort");
  CHECK(models[0].reasoning.levels.size() == 5);
  CHECK(models[0].reasoning.defaultLevel == "medium");
  CHECK(models[1].reasoning.mode == "budget");
}

// ---------------------------------------------------------------------------
// OpenAI

TEST_CASE("OpenAI Responses body") {
  ChatRequest req = Simple("gpt-5.5", "high");
  req.messages[0].image = ImageData{"image/png", "QUJD"};
  json b = BuildOpenAIResponsesBody(req, Model("effort", {"low", "medium", "high"}, "medium"));
  CHECK(b["instructions"] == "sys");
  CHECK(b["store"] == false);
  CHECK(b["reasoning"]["effort"] == "high");
  CHECK(b["reasoning"]["summary"] == "auto");
  CHECK(b["input"][0]["content"][1]["image_url"] == "data:image/png;base64,QUJD");

  json none = BuildOpenAIResponsesBody(Simple("gpt-4.1"), Model("none", {}, ""));
  CHECK_FALSE(none.contains("reasoning"));
}

TEST_CASE("OpenAI Responses stream") {
  Capture c;
  StreamSink sink = c.Sink();
  OpenAIResponsesStream s(sink);
  s.OnEvent(json::parse(R"({"type":"response.reasoning_summary_text.delta","delta":"think"})"));
  s.OnEvent(json::parse(R"({"type":"response.output_text.delta","delta":"Hi"})"));
  s.OnEvent(json::parse(R"({"type":"response.completed","response":{"usage":{"total_tokens":3}}})"));
  auto r = s.Finish();
  CHECK(r.ok);
  CHECK(c.text == "Hi");
  CHECK(c.reasoning == "think");
  CHECK(r.usage["total_tokens"] == 3);
}

TEST_CASE("Chat Completions: reasoning parameter per provider") {
  auto model = Model("effort", {"low", "medium", "high"}, "medium");
  json routed = BuildChatCompletionsBody(Simple("x/y", "low"), model, "openrouter");
  CHECK(routed["reasoning"]["effort"] == "low");
  json other = BuildChatCompletionsBody(Simple("grok", "high"), model, "xai");
  CHECK(other["reasoning_effort"] == "high");
  CHECK(other["messages"][0]["role"] == "system");
}

TEST_CASE("Chat Completions: images dropped for non-vision models") {
  ChatRequest req = Simple("m");
  req.messages[0].image = ImageData{"image/jpeg", "QUJD"};
  json vision = BuildChatCompletionsBody(req, Model("none", {}, "", true), "custom");
  CHECK(vision["messages"][1]["content"].is_array());
  json blind = BuildChatCompletionsBody(req, Model("none", {}, "", false), "custom");
  CHECK(blind["messages"][1]["content"] == "hi");
}

TEST_CASE("Chat Completions stream: <think> tags split into reasoning across chunks") {
  Capture c;
  StreamSink sink = c.Sink();
  ChatCompletionsStream s(sink);
  auto chunk = [&](const std::string& content) {
    s.OnData(json({{"choices", json::array({{{"delta", {{"content", content}}}}})}}).dump());
  };
  chunk("\n<thi");
  chunk("nk>plan");
  chunk(" more</th");
  chunk("ink>\n\nAnswer");
  chunk(" here");
  s.OnData("[DONE]");
  auto r = s.Finish();
  CHECK(r.ok);
  CHECK(c.reasoning == "plan more");
  CHECK(c.text == "Answer here");
}

TEST_CASE("Chat Completions stream: reasoning_content field and errors") {
  Capture c;
  StreamSink sink = c.Sink();
  ChatCompletionsStream s(sink);
  s.OnData(R"({"choices":[{"delta":{"reasoning_content":"r"}}]})");
  s.OnData(R"({"choices":[{"delta":{"content":"t"},"finish_reason":"stop"}]})");
  auto r = s.Finish();
  CHECK(c.reasoning == "r");
  CHECK(c.text == "t");
  CHECK(r.stopReason == "stop");

  Capture c2;
  StreamSink sink2 = c2.Sink();
  ChatCompletionsStream e(sink2);
  e.OnData(R"({"error":{"message":"rate limited"}})");
  auto r2 = e.Finish();
  CHECK_FALSE(r2.ok);
  CHECK(r2.error == "rate limited");
}

TEST_CASE("OpenAI-style model listing filters non-chat models and sorts newest first") {
  json listing = json::parse(R"({"data":[
    {"id":"text-embedding-3-large","created":5},
    {"id":"gpt-4.1","created":1},
    {"id":"gpt-5.5","created":3},
    {"id":"whisper-1","created":9},
    {"id":"o3","created":2}
  ]})");
  auto models = ParseOpenAIStyleModels(listing, "openai");
  REQUIRE(models.size() == 3);
  CHECK(models[0].id == "gpt-5.5");
  CHECK(models[1].id == "o3");
  CHECK(models[2].id == "gpt-4.1");
}

TEST_CASE("OpenRouter listing uses supported_parameters and modalities") {
  json listing = json::parse(R"({"data":[
    {"id":"vendor/thinker","name":"Thinker","supported_parameters":["reasoning","tools"],
     "architecture":{"input_modalities":["text"]}},
    {"id":"vendor/seer","supported_parameters":["tools"],"architecture":{"input_modalities":["text","image"]}}
  ]})");
  auto models = ParseOpenAIStyleModels(listing, "openrouter");
  REQUIRE(models.size() == 2);
  const ModelInfo& thinker = models[0].id == "vendor/thinker" ? models[0] : models[1];
  const ModelInfo& seer = models[0].id == "vendor/seer" ? models[0] : models[1];
  CHECK(thinker.reasoning.mode != "none");
  CHECK_FALSE(thinker.vision);
  CHECK(seer.reasoning.mode == "none");
  CHECK(seer.vision);
}

// ---------------------------------------------------------------------------
// Gemini

TEST_CASE("Gemini body: thinkingLevel and thinkingBudget") {
  json lvl = BuildGeminiBody(Simple("gemini-3-pro", "high"), Model("level", {"low", "high"}, "high"));
  CHECK(lvl["generationConfig"]["thinkingConfig"]["thinkingLevel"] == "high");
  CHECK(lvl["generationConfig"]["thinkingConfig"]["includeThoughts"] == true);
  CHECK(lvl["systemInstruction"]["parts"][0]["text"] == "sys");

  json off = BuildGeminiBody(Simple("gemini-2.5-flash", "off"), Model("budget", {"off", "low"}, "off"));
  CHECK(off["generationConfig"]["thinkingConfig"]["thinkingBudget"] == 0);
}

TEST_CASE("Gemini stream: thoughts vs text, blocked prompts") {
  Capture c;
  StreamSink sink = c.Sink();
  GeminiStream s(sink);
  s.OnEvent(json::parse(R"({"candidates":[{"content":{"parts":[{"text":"why","thought":true},{"text":"Yes"}]}}]})"));
  s.OnEvent(json::parse(R"({"candidates":[{"content":{"parts":[{"text":"!"}]},"finishReason":"STOP"}],"usageMetadata":{"totalTokenCount":4}})"));
  auto r = s.Finish();
  CHECK(r.ok);
  CHECK(c.text == "Yes!");
  CHECK(c.reasoning == "why");

  Capture c2;
  StreamSink sink2 = c2.Sink();
  GeminiStream b(sink2);
  b.OnEvent(json::parse(R"({"promptFeedback":{"blockReason":"SAFETY"}})"));
  auto r2 = b.Finish();
  CHECK_FALSE(r2.ok);
  CHECK(r2.error.find("SAFETY") != std::string::npos);
}

// ---------------------------------------------------------------------------
// CLI bridges

TEST_CASE("FlattenConversation includes history and the latest message") {
  ChatRequest req = Simple("m");
  ChatMessage a;
  a.role = "assistant";
  a.text = "hello";
  req.messages.push_back(a);
  ChatMessage u;
  u.role = "user";
  u.text = "and now?";
  req.messages.push_back(u);
  std::string flat = FlattenConversation(req, true);
  CHECK(flat.find("<instructions>\nsys\n</instructions>") == 0);
  CHECK(flat.find("User: hi") != std::string::npos);
  CHECK(flat.find("Assistant: hello") != std::string::npos);
  CHECK(flat.substr(flat.size() - 8) == "and now?");
  CHECK(FlattenConversation(Simple("m"), false) == "hi");
}

TEST_CASE("Claude CLI input is one stream-json user line with the image first") {
  ChatRequest req = Simple("opus");
  req.messages[0].image = ImageData{"image/jpeg", "QUJD"};
  std::string line = BuildClaudeCliInput(req);
  REQUIRE(line.back() == '\n');
  json j = json::parse(line);
  CHECK(j["type"] == "user");
  CHECK(j["message"]["content"][0]["type"] == "image");
  CHECK(j["message"]["content"][1]["text"] == "hi");
}

TEST_CASE("Claude CLI stream: partial messages and result") {
  Capture c;
  StreamSink sink = c.Sink();
  ClaudeCliStream s(sink);
  s.OnLine(R"({"type":"system","subtype":"init"})");
  s.OnLine(R"({"type":"stream_event","event":{"type":"content_block_start","index":0,"content_block":{"type":"text","text":""}}})");
  s.OnLine(R"({"type":"stream_event","event":{"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"Hey"}}})");
  s.OnLine(R"({"type":"assistant","message":{"content":[{"type":"text","text":"Hey"}]}})");
  CHECK_FALSE(s.Done());
  s.OnLine(R"({"type":"result","subtype":"success","is_error":false,"result":"Hey","usage":{"output_tokens":1}})");
  CHECK(s.Done());
  auto r = s.Finish();
  CHECK(r.ok);
  CHECK(c.text == "Hey");  // not duplicated by the final result
}

TEST_CASE("Claude CLI stream: error result") {
  Capture c;
  StreamSink sink = c.Sink();
  ClaudeCliStream s(sink);
  s.OnLine(R"({"type":"result","subtype":"error_during_execution","is_error":true,"result":"Not logged in"})");
  auto r = s.Finish();
  CHECK_FALSE(r.ok);
  CHECK(r.error == "Not logged in");
}

TEST_CASE("Codex CLI stream: incremental items, reasoning, usage") {
  Capture c;
  StreamSink sink = c.Sink();
  CodexCliStream s(sink);
  s.OnLine(R"({"type":"thread.started","thread_id":"t"})");
  s.OnLine(R"({"type":"item.completed","item":{"id":"item_0","type":"reasoning","text":"**Plan**"}})");
  s.OnLine(R"({"type":"item.updated","item":{"id":"item_1","type":"agent_message","text":"Hel"}})");
  s.OnLine(R"({"type":"item.completed","item":{"id":"item_1","type":"agent_message","text":"Hello"}})");
  CHECK_FALSE(s.Done());
  s.OnLine(R"({"type":"turn.completed","usage":{"input_tokens":3,"output_tokens":2}})");
  CHECK(s.Done());
  auto r = s.Finish();
  CHECK(r.ok);
  CHECK(c.text == "Hello");
  CHECK(c.reasoning == "**Plan**");
  CHECK(r.usage["output_tokens"] == 2);
}

TEST_CASE("Codex CLI stream: failures") {
  Capture c;
  StreamSink sink = c.Sink();
  CodexCliStream s(sink);
  s.OnLine(R"({"type":"turn.failed","error":{"message":"usage limit reached"}})");
  CHECK(s.Done());
  auto r = s.Finish();
  CHECK_FALSE(r.ok);
  CHECK(r.error == "usage limit reached");

  Capture c2;
  StreamSink sink2 = c2.Sink();
  CodexCliStream empty(sink2);
  CHECK(empty.Finish().error.find("without a response") != std::string::npos);
}

TEST_CASE("Gemini CLI stream") {
  Capture c;
  StreamSink sink = c.Sink();
  GeminiCliStream s(sink);
  s.OnLine(R"({"type":"init","session_id":"x"})");
  s.OnLine(R"({"type":"message","role":"user","content":"hi"})");
  s.OnLine(R"({"type":"message","role":"assistant","content":"Hi ","delta":true})");
  s.OnLine(R"({"type":"message","role":"assistant","content":"there","delta":true})");
  CHECK_FALSE(s.Done());
  s.OnLine(R"({"type":"result","status":"success","stats":{"total_tokens":5}})");
  CHECK(s.Done());
  auto r = s.Finish();
  CHECK(r.ok);
  CHECK(c.text == "Hi there");
}

// ---------------------------------------------------------------------------
// Catalog

TEST_CASE("Catalog: reasoning inference") {
  CHECK(InferModel("anthropic", "claude-fable-5-1").reasoning.defaultLevel == "high");
  CHECK(InferModel("anthropic", "claude-opus-5-5").reasoning.mode == "effort");
  CHECK(InferModel("anthropic", "claude-haiku-5-5").reasoning.mode == "effort");
  CHECK(InferModel("anthropic", "claude-haiku-5-5").reasoning.defaultLevel == "medium");
  CHECK(InferModel("anthropic", "claude-haiku-4-5").reasoning.mode == "budget");
  CHECK(InferModel("openrouter", "anthropic/claude-opus-5-5").reasoning.mode == "effort");
  CHECK(InferModel("gemini", "gemini-3-pro").reasoning.mode == "level");
  CHECK(InferModel("gemini", "gemini-2.5-pro").reasoning.mode == "budget");
}

TEST_CASE("Codex models cache parsing") {
  json cache = json::parse(R"({"models":[
    {"slug":"gpt-5.5","display_name":"GPT-5.5","visibility":"list","priority":1,
     "default_reasoning_level":"medium",
     "supported_reasoning_levels":[{"effort":"low"},{"effort":"medium"},{"effort":"high"},{"effort":"xhigh"},{"effort":"ultra"}],
     "input_modalities":["text","image"]},
    {"slug":"internal","display_name":"Hidden","visibility":"hide","priority":2}
  ]})");
  auto visible = ParseCodexModelsCache(cache, false);
  REQUIRE(visible.size() == 1);
  CHECK(visible[0].id == "gpt-5.5");
  CHECK(visible[0].reasoning.mode == "effort");
  CHECK(visible[0].reasoning.levels.size() == 4);  // "ultra" is skipped
  CHECK(visible[0].reasoning.defaultLevel == "medium");
  CHECK(ParseCodexModelsCache(cache, true).size() == 2);
}

TEST_CASE("ModelInfo JSON round trip and user overrides") {
  ModelInfo m = InferModel("anthropic", "claude-opus-5-5");
  ModelInfo back = ModelFromJson(ToJson(m));
  CHECK(back.id == m.id);
  CHECK(back.reasoning.levels == m.reasoning.levels);
  CHECK(back.reasoning.defaultLevel == m.reasoning.defaultLevel);

  json cfg = {{"customModels", {"my-model"}}, {"reasoningOverrides", {{"claude-opus-5-5", false}}}};
  auto models = ApplyModelConfig("anthropic", {m}, cfg);
  REQUIRE(models.size() == 2);
  CHECK(models[0].reasoning.mode == "none");
  CHECK(models[1].id == "my-model");
}
