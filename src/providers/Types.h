#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/Util.h"

namespace aia {

struct ImageData {
  std::string mime;    // "image/jpeg" or "image/png"
  std::string base64;  // No data: prefix
};

struct ChatMessage {
  std::string role;  // "user" | "assistant"
  std::string text;
  std::optional<ImageData> image;  // user messages only
  // Provider-native assistant content (e.g. Anthropic content blocks with thinking signatures),
  // replayed verbatim on follow-ups when the same provider is used.
  json raw;
  std::string rawProvider;
};

struct ChatRequest {
  std::string model;
  std::string reasoning;  // A level from ModelInfo::reasoning.levels, or empty for the default
  std::string system;
  std::vector<ChatMessage> messages;
};

// How a model exposes reasoning control.
//   none   - no reasoning
//   effort - named effort levels (Anthropic output_config.effort, OpenAI reasoning.effort, CLIs)
//   budget - thinking token budget presets (older Claude, Gemini 2.5)
//   level  - Gemini 3 thinkingLevel
//   always - reasons internally, not configurable (levels empty)
struct ReasoningSpec {
  std::string mode = "none";
  std::vector<std::string> levels;
  std::string defaultLevel;
};

struct ModelInfo {
  std::string id;
  std::string name;
  bool vision = true;
  ReasoningSpec reasoning;
};

json ToJson(const ModelInfo& m);
ModelInfo ModelFromJson(const json& j);

// Callbacks a provider invokes while streaming. Called on the worker thread.
struct StreamSink {
  std::function<void(std::string_view)> text;
  std::function<void(std::string_view)> reasoning;
  std::function<void(std::string_view)> status;  // short progress notes ("Starting Claude Code…")
  std::function<void(std::string_view)> limit;   // provider-reported subscription warning, empty = cleared
};

struct StreamResult {
  bool ok = false;
  bool cancelled = false;
  std::string error;
  std::string stopReason;
  json raw;    // Provider-native assistant content for replay
  json usage;  // Provider usage object (if any)
  std::string limitWarning;
  bool streamed = true;  // False when text arrives in whole items, so timing can't show tokens
};

struct ModelListResult {
  std::vector<ModelInfo> models;
  std::string error;
};

enum class ProviderKind { Api, Local, Cli };

struct ProviderInfo {
  std::string id;
  std::string name;
  ProviderKind kind = ProviderKind::Api;
  std::string description;
  std::string defaultBaseUrl;
  bool needsKey = true;
  bool keyOptional = false;
  bool baseUrlEditable = false;
  std::string keyUrl;      // Where to create a key
  std::string defaultModel;
};

class Provider {
 public:
  virtual ~Provider() = default;
  // Both calls block; run them on a worker thread.
  virtual ModelListResult ListModels(CancelToken& cancel) = 0;
  virtual StreamResult Stream(const ChatRequest& request, const ModelInfo& model, const StreamSink& sink,
                              CancelToken& cancel) = 0;
  // Does slow setup for a request like this one ahead of time (connecting, starting a CLI) so a
  // following Stream starts faster. Blocking but best-effort; run it on a worker thread.
  virtual void Prepare(const ChatRequest& /*request*/, const ModelInfo& /*model*/) {}
};

}  // namespace aia
