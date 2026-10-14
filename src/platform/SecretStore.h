#pragma once

#include <optional>
#include <string>

namespace aia {

// API keys live in Windows Credential Manager (generic credentials, target "AIAssistant/<id>").
namespace SecretStore {

bool Set(const std::string& id, const std::string& secret);
std::optional<std::string> Get(const std::string& id);
bool Has(const std::string& id);
bool Remove(const std::string& id);

}  // namespace SecretStore
}  // namespace aia
