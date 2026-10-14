#pragma once

#include <string>
#include <vector>

#include "providers/Types.h"

namespace aia {

// Built-in capability knowledge, used when a provider's model listing does not report
// vision/reasoning support (or for CLI bridges that cannot list models).
ModelInfo InferModel(const std::string& providerId, const std::string& modelId);

// Models shown before (or instead of) a live listing.
std::vector<ModelInfo> BuiltinModels(const std::string& providerId);

// Models from Codex's local cache (%USERPROFILE%\.codex\models_cache.json), if present.
std::vector<ModelInfo> CodexCachedModels(bool includeHidden = false);

// Parses Codex's models_cache.json contents.
std::vector<ModelInfo> ParseCodexModelsCache(const json& cache, bool includeHidden);

}  // namespace aia
