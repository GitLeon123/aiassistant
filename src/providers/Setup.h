#pragma once

#include "providers/Types.h"

namespace aia {

// Discovery only contacts loopback servers, never arbitrary configured endpoints.
bool IsLoopbackUrl(const std::string& url);
ModelListResult DiscoverLocalModels(const std::string& providerId, const std::string& baseUrl);

struct SetupAnswer {
  bool ok = false;
  std::string answer;
  std::string error;
};

// An isolated, text-only request. Does not use the desktop, chat history or user prompt.
SetupAnswer TestSetupAnswer(Provider& provider, const ModelInfo& model, CancelToken& cancel);

}  // namespace aia
