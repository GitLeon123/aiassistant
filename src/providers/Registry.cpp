#include "providers/Registry.h"

#include <algorithm>

#include "providers/Catalog.h"

namespace aia {

json ToJson(const ModelInfo& m) {
  return {{"id", m.id},
          {"name", m.name},
          {"vision", m.vision},
          {"reasoning", {{"mode", m.reasoning.mode}, {"levels", m.reasoning.levels}, {"default", m.reasoning.defaultLevel}}}};
}

ModelInfo ModelFromJson(const json& j) {
  ModelInfo m;
  m.id = JsonGet<std::string>(j, "id", "");
  m.name = JsonGet<std::string>(j, "name", m.id);
  m.vision = JsonGet<bool>(j, "vision", true);
  m.reasoning.mode = JsonGet<std::string>(j, "reasoning/mode", "none");
  m.reasoning.levels = JsonGet<std::vector<std::string>>(j, "reasoning/levels", {});
  m.reasoning.defaultLevel = JsonGet<std::string>(j, "reasoning/default", "");
  return m;
}

const std::vector<ProviderInfo>& AllProviders() {
  static const std::vector<ProviderInfo> providers = [] {
    std::vector<ProviderInfo> v;
    auto add = [&](ProviderInfo p) { v.push_back(std::move(p)); };
    // Subscriptions, bridged through the official CLIs (they hold the sign-in, not us).
    add({"claude-cli", "Claude", ProviderKind::Cli,
         "Your Claude Pro/Max subscription, via the Claude Code CLI", "", false, false, false,
         "https://claude.com/claude-code", "opus"});
    add({"codex-cli", "ChatGPT", ProviderKind::Cli, "Your ChatGPT Plus/Pro subscription, via the Codex CLI", "",
         false, false, false, "https://developers.openai.com/codex/cli", ""});
    add({"gemini-cli", "Gemini", ProviderKind::Cli, "Your Google account, via the Gemini CLI", "", false, false,
         false, "https://github.com/google-gemini/gemini-cli", ""});
    // API keys.
    add({"anthropic", "Anthropic", ProviderKind::Api, "Claude models with an Anthropic API key",
         "https://api.anthropic.com", true, false, false, "https://console.anthropic.com/settings/keys",
         "claude-opus-5-5"});
    add({"openai", "OpenAI", ProviderKind::Api, "GPT models with an OpenAI API key", "https://api.openai.com/v1",
         true, false, false, "https://platform.openai.com/api-keys", ""});
    add({"gemini", "Google Gemini", ProviderKind::Api, "Gemini models with a Google AI Studio key",
         "https://generativelanguage.googleapis.com/v1beta", true, false, false, "https://aistudio.google.com/apikey",
         ""});
    add({"openrouter", "OpenRouter", ProviderKind::Api, "Hundreds of models with one key",
         "https://openrouter.ai/api/v1", true, false, false, "https://openrouter.ai/keys", ""});
    add({"xai", "xAI", ProviderKind::Api, "Grok models", "https://api.x.ai/v1", true, false, false,
         "https://console.x.ai", ""});
    add({"groq", "Groq", ProviderKind::Api, "Very fast open models", "https://api.groq.com/openai/v1", true, false,
         false, "https://console.groq.com/keys", ""});
    add({"mistral", "Mistral", ProviderKind::Api, "Mistral and Magistral models", "https://api.mistral.ai/v1", true,
         false, false, "https://console.mistral.ai/api-keys", ""});
    add({"deepseek", "DeepSeek", ProviderKind::Api, "DeepSeek chat and reasoner", "https://api.deepseek.com/v1",
         true, false, false, "https://platform.deepseek.com/api_keys", ""});
    // Local and custom endpoints (OpenAI-compatible).
    add({"ollama", "Ollama", ProviderKind::Local, "Local models served by Ollama", "http://localhost:11434/v1",
         false, true, true, "https://ollama.com/download", ""});
    add({"lmstudio", "LM Studio", ProviderKind::Local, "Local models served by LM Studio",
         "http://localhost:1234/v1", false, true, true, "https://lmstudio.ai", ""});
    add({"custom", "Custom endpoint", ProviderKind::Api, "Any OpenAI-compatible /chat/completions endpoint", "",
         true, true, true, "", ""});
    return v;
  }();
  return providers;
}

const ProviderInfo* FindProvider(const std::string& id) {
  for (const auto& p : AllProviders()) {
    if (p.id == id) return &p;
  }
  return nullptr;
}

std::unique_ptr<Provider> CreateProvider(const std::string& id, const json& config, const std::string& apiKey) {
  const ProviderInfo* info = FindProvider(id);
  if (!info) return nullptr;
  if (info->kind == ProviderKind::Cli) return MakeCliProvider(id);
  std::string base = config.is_object() ? config.value("baseUrl", "") : "";
  if (base.empty() || !info->baseUrlEditable) base = info->defaultBaseUrl;
  base = TrimRight(base, "/ ");
  if (id == "anthropic") return MakeAnthropicProvider(base, apiKey);
  if (id == "openai") return MakeOpenAIProvider(base, apiKey);
  if (id == "gemini") return MakeGeminiProvider(base, apiKey);
  return MakeCompatProvider(id, base, apiKey);
}

std::vector<ModelInfo> ApplyModelConfig(const std::string& providerId, std::vector<ModelInfo> models,
                                        const json& config) {
  if (!config.is_object()) return models;
  if (auto it = config.find("customModels"); it != config.end() && it->is_array()) {
    for (const auto& id : *it) {
      if (!id.is_string() || id.get<std::string>().empty()) continue;
      std::string s = id.get<std::string>();
      bool exists = std::any_of(models.begin(), models.end(), [&](const ModelInfo& m) { return m.id == s; });
      if (!exists) models.push_back(InferModel(providerId, s));
    }
  }
  if (auto it = config.find("reasoningOverrides"); it != config.end() && it->is_object()) {
    for (auto& m : models) {
      auto o = it->find(m.id);
      if (o == it->end() || !o->is_boolean()) continue;
      if (!o->get<bool>()) {
        m.reasoning = ReasoningSpec{};
      } else if (m.reasoning.mode == "none") {
        m.reasoning = ReasoningSpec{"effort", {"low", "medium", "high"}, "medium"};
      }
    }
  }
  return models;
}

}  // namespace aia
