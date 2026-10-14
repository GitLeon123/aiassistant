#pragma once

#include <string>

#include "core/Util.h"

namespace aia {

// User settings persisted as JSON in %APPDATA%\AIAssistant\settings.json.
// Secrets (API keys) are never stored here; see platform/SecretStore.
class Settings {
 public:
  static json Defaults();
  static std::string DefaultSystemPrompt();

  void Load();
  bool Save() const;

  const json& Data() const { return data_; }
  // RFC 7386 merge patch, then re-normalized against defaults. Unknown top-level keys are dropped.
  void Apply(const json& patch);

  std::string Hotkey() const { return data_.value("hotkey", "Alt+Space"); }
  std::string SystemPrompt() const;
  json ProviderConfig(const std::string& id) const;
  int ConversationTimeoutMinutes() const { return data_.value("conversationTimeoutMinutes", 10); }

 private:
  void Normalize();
  json data_ = Defaults();
};

}  // namespace aia
