#include "providers/Catalog.h"

#include <windows.h>

#include <algorithm>
#include <mutex>

#include "core/Paths.h"

namespace aia {
namespace {

const std::vector<std::string> kClaudeEffortFull = {"low", "medium", "high", "xhigh", "max"};
const std::vector<std::string> kClaudeEffort46 = {"low", "medium", "high", "max"};
const std::vector<std::string> kBudgetWithOff = {"off", "low", "medium", "high"};
const std::vector<std::string> kLowMedHigh = {"low", "medium", "high"};

ReasoningSpec Spec(std::string mode, std::vector<std::string> levels, std::string def) {
  return ReasoningSpec{std::move(mode), std::move(levels), std::move(def)};
}

ReasoningSpec None() { return ReasoningSpec{}; }
ReasoningSpec Always() { return Spec("always", {}, ""); }

std::string PrettyName(const std::string& id) {
  // "claude-opus-5-5" -> "Claude Opus 5.5"; "gpt-5.5-mini" -> "GPT-5.5 Mini"
  std::string out;
  bool upperNext = true;
  for (size_t i = 0; i < id.size(); ++i) {
    char c = id[i];
    if (c == '-' || c == '_') {
      bool digitsAround = i > 0 && i + 1 < id.size() && std::isdigit(static_cast<unsigned char>(id[i - 1])) &&
                          std::isdigit(static_cast<unsigned char>(id[i + 1]));
      out.push_back(digitsAround ? '.' : ' ');
      upperNext = true;
      continue;
    }
    out.push_back(upperNext ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : c);
    upperNext = false;
  }
  if (StartsWith(out, "Gpt")) out.replace(0, 3, "GPT");
  if (StartsWith(out, "GPT ")) out[3] = '-';
  return out;
}

ReasoningSpec AnthropicReasoning(const std::string& id) {
  if (Contains(id, "fable") || Contains(id, "mythos")) return Spec("effort", kClaudeEffortFull, "high");
  if (Contains(id, "opus-5-5") || Contains(id, "haiku-5")) return Spec("effort", kClaudeEffortFull, "medium");
  if (Contains(id, "opus-5") || Contains(id, "sonnet-5") || Contains(id, "opus-4-8") || Contains(id, "opus-4-7")) {
    return Spec("effort", kClaudeEffortFull, "high");
  }
  if (Contains(id, "opus-4-6") || Contains(id, "sonnet-4-6")) return Spec("effort", kClaudeEffort46, "high");
  if (Contains(id, "claude-3-5") || Contains(id, "claude-3-haiku") || Contains(id, "claude-3-opus")) return None();
  return Spec("budget", kBudgetWithOff, "off");
}

ReasoningSpec OpenAIReasoning(const std::string& id) {
  // Prefer Codex's cache, which carries the server's real reasoning levels.
  for (const auto& m : CodexCachedModels(true)) {
    if (m.id == id) return m.reasoning;
  }
  if (StartsWith(id, "o1") || StartsWith(id, "o3") || StartsWith(id, "o4")) return Spec("effort", kLowMedHigh, "medium");
  if (id == "gpt-5" || StartsWith(id, "gpt-5-")) {
    if (Contains(id, "chat")) return None();
    return Spec("effort", {"minimal", "low", "medium", "high"}, "medium");
  }
  if (StartsWith(id, "gpt-5.") || StartsWith(id, "gpt-6") || Contains(id, "codex")) {
    if (Contains(id, "chat")) return None();
    return Spec("effort", {"low", "medium", "high", "xhigh"}, "medium");
  }
  if (StartsWith(id, "gpt-oss")) return Spec("effort", kLowMedHigh, "medium");
  return None();
}

ReasoningSpec GeminiReasoning(const std::string& id) {
  if (StartsWith(id, "gemini-3")) {
    if (Contains(id, "flash")) return Spec("level", {"minimal", "low", "medium", "high"}, "high");
    return Spec("level", {"low", "high"}, "high");
  }
  if (StartsWith(id, "gemini-2.5-pro")) return Spec("budget", kLowMedHigh, "medium");
  if (StartsWith(id, "gemini-2.5-flash")) return Spec("budget", kBudgetWithOff, "medium");
  return None();
}

bool LocalVision(const std::string& id) {
  for (const char* hint : {"vl", "vision", "llava", "gemma3", "gemma-3", "minicpm-v", "moondream", "pixtral",
                           "mistral-small3", "granite3.2-vision", "qwen2.5vl", "qwen3-vl", "llama4"}) {
    if (Contains(id, hint)) return true;
  }
  return false;
}

}  // namespace

ModelInfo InferModel(const std::string& providerId, const std::string& rawId) {
  ModelInfo m;
  m.id = rawId;
  m.name = PrettyName(rawId);
  std::string id = ToLower(rawId);
  // OpenRouter-style "vendor/model" ids: infer from the model part with the vendor's rules.
  std::string vendor;
  if (size_t slash = id.find('/'); slash != std::string::npos) {
    vendor = id.substr(0, slash);
    id = id.substr(slash + 1);
  }
  const std::string& p = providerId;

  if (p == "anthropic" || vendor == "anthropic") {
    m.reasoning = AnthropicReasoning(id);
    m.vision = true;
  } else if (p == "openai" || vendor == "openai") {
    m.reasoning = OpenAIReasoning(id);
    m.vision = !StartsWith(id, "gpt-3.5") && !StartsWith(id, "gpt-oss") && !StartsWith(id, "o1-mini") &&
               !StartsWith(id, "o3-mini");
  } else if (p == "gemini" || vendor == "google") {
    m.reasoning = GeminiReasoning(id);
    m.vision = !Contains(id, "gemma") || Contains(id, "gemma-3");
  } else if (p == "xai" || vendor == "x-ai") {
    if (StartsWith(id, "grok-3-mini")) m.reasoning = Spec("effort", {"low", "high"}, "low");
    else if (StartsWith(id, "grok-4") || Contains(id, "reasoning")) m.reasoning = Always();
    m.vision = Contains(id, "vision") || StartsWith(id, "grok-4");
  } else if (p == "deepseek" || vendor == "deepseek") {
    if (Contains(id, "reasoner") || Contains(id, "r1")) m.reasoning = Always();
    m.vision = false;
  } else if (p == "mistral" || vendor == "mistralai") {
    if (Contains(id, "magistral")) m.reasoning = Always();
    m.vision = Contains(id, "pixtral") || Contains(id, "medium") || Contains(id, "small") || Contains(id, "large");
  } else if (p == "groq" || p == "ollama" || p == "lmstudio" || p == "custom") {
    if (Contains(id, "gpt-oss")) m.reasoning = Spec("effort", kLowMedHigh, "medium");
    else if (Contains(id, "deepseek-r1") || Contains(id, "qwq") || Contains(id, "qwen3")) m.reasoning = Always();
    m.vision = p == "custom" ? true : LocalVision(id);
  } else if (p == "openrouter") {
    m.reasoning = None();
    m.vision = true;
  }
  return m;
}

std::vector<ModelInfo> BuiltinModels(const std::string& providerId) {
  auto make = [&](std::initializer_list<const char*> ids) {
    std::vector<ModelInfo> out;
    for (const char* id : ids) out.push_back(InferModel(providerId, id));
    return out;
  };
  if (providerId == "anthropic") {
    return make({"claude-opus-5-5", "claude-fable-5-1", "claude-sonnet-5-5", "claude-haiku-5-5"});
  }
  if (providerId == "claude-cli") {
    std::vector<ModelInfo> out;
    auto add = [&](const char* id, const char* name, ReasoningSpec spec) {
      out.push_back(ModelInfo{id, name, true, std::move(spec)});
    };
    add("opus", "Opus (latest)", Spec("effort", kClaudeEffortFull, "high"));
    add("sonnet", "Sonnet (latest)", Spec("effort", kClaudeEffortFull, "high"));
    add("fable", "Fable (latest)", Spec("effort", kClaudeEffortFull, "high"));
    add("haiku", "Haiku (latest)", Spec("effort", kClaudeEffortFull, "medium"));
    return out;
  }
  if (providerId == "codex-cli") {
    auto cached = CodexCachedModels();
    if (!cached.empty()) return cached;
    std::vector<ModelInfo> out;
    out.push_back(ModelInfo{"", "Codex default", true, Spec("effort", {"low", "medium", "high", "xhigh"}, "medium")});
    return out;
  }
  if (providerId == "gemini-cli") {
    std::vector<ModelInfo> out;
    out.push_back(ModelInfo{"", "Gemini CLI default", true, None()});
    out.push_back(ModelInfo{"gemini-2.5-pro", "Gemini 2.5 Pro", true, None()});
    out.push_back(ModelInfo{"gemini-2.5-flash", "Gemini 2.5 Flash", true, None()});
    return out;
  }
  return {};
}

std::vector<ModelInfo> ParseCodexModelsCache(const json& cache, bool includeHidden) {
  std::vector<ModelInfo> out;
  const json& models = cache.contains("models") ? cache["models"] : cache;
  if (!models.is_array()) return out;
  for (const auto& m : models) {
    if (!m.is_object()) continue;
    std::string slug = m.value("slug", "");
    if (slug.empty()) continue;
    if (!includeHidden && m.value("visibility", "list") != "list") continue;
    ModelInfo info;
    info.id = slug;
    info.name = m.value("display_name", slug);
    info.vision = true;
    if (auto it = m.find("input_modalities"); it != m.end() && it->is_array()) {
      info.vision = std::find(it->begin(), it->end(), "image") != it->end();
    }
    std::vector<std::string> levels;
    if (auto it = m.find("supported_reasoning_levels"); it != m.end() && it->is_array()) {
      for (const auto& l : *it) {
        std::string effort = l.is_object() ? l.value("effort", "") : (l.is_string() ? l.get<std::string>() : "");
        // "ultra" delegates to sub-agents; not meaningful for a single-shot answer.
        if (!effort.empty() && effort != "ultra") levels.push_back(effort);
      }
    }
    if (!levels.empty()) {
      info.reasoning = Spec("effort", levels, m.value("default_reasoning_level", levels.front()));
    }
    out.push_back(std::move(info));
  }
  return out;
}

std::vector<ModelInfo> CodexCachedModels(bool includeHidden) {
  // The file is consulted for every OpenAI model during a listing, so keep it briefly in memory.
  static std::mutex mutex;
  static json cached;
  static ULONGLONG loadedAt = 0;
  json cache;
  {
    std::lock_guard lock(mutex);
    ULONGLONG now = GetTickCount64();
    if (loadedAt == 0 || now - loadedAt > 60'000) {
      loadedAt = now;
      cached = json();
      wchar_t home[MAX_PATH * 2];
      std::wstring dir;
      if (GetEnvironmentVariableW(L"CODEX_HOME", home, static_cast<DWORD>(std::size(home)))) {
        dir = home;
      } else if (GetEnvironmentVariableW(L"USERPROFILE", home, static_cast<DWORD>(std::size(home)))) {
        dir = std::wstring(home) + L"\\.codex";
      }
      std::string text;
      if (!dir.empty() && ReadFileUtf8(dir + L"\\models_cache.json", text)) {
        try {
          cached = json::parse(text);
        } catch (...) {
        }
      }
    }
    cache = cached;
  }
  if (cache.is_null()) return {};
  return ParseCodexModelsCache(cache, includeHidden);
}

}  // namespace aia
