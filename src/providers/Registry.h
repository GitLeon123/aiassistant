#pragma once

#include <memory>
#include <string>
#include <vector>

#include "providers/Types.h"

namespace aia {

const std::vector<ProviderInfo>& AllProviders();
const ProviderInfo* FindProvider(const std::string& id);

// config is the provider's entry from settings ("providers"/<id>); apiKey may be empty.
std::unique_ptr<Provider> CreateProvider(const std::string& id, const json& config, const std::string& apiKey);

// Applies user settings (custom model ids, reasoning on/off overrides) to a model listing.
std::vector<ModelInfo> ApplyModelConfig(const std::string& providerId, std::vector<ModelInfo> models,
                                        const json& config);

struct CliStatus {
  bool installed = false;
  bool signedIn = false;
  std::string path;
  std::string detail;  // e.g. "Signed in as you@example.com (claude.ai)"
};

// Stops any CLI process that Provider::Prepare started ahead of time.
void DiscardPreparedCli();

// Blocking: locates the CLI and asks it for its sign-in state.
CliStatus DetectCli(const std::string& providerId);

// API-provider implementations (ApiProviders.cpp / CliProviders.cpp).
std::unique_ptr<Provider> MakeAnthropicProvider(std::string baseUrl, std::string key);
std::unique_ptr<Provider> MakeOpenAIProvider(std::string baseUrl, std::string key);
std::unique_ptr<Provider> MakeCompatProvider(std::string providerId, std::string baseUrl, std::string key);
std::unique_ptr<Provider> MakeGeminiProvider(std::string baseUrl, std::string key);
std::unique_ptr<Provider> MakeCliProvider(const std::string& providerId);

}  // namespace aia
