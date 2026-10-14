#include <windows.h>
#include <wincred.h>

#include "platform/SecretStore.h"

#include "core/Util.h"

namespace aia::SecretStore {
namespace {

std::wstring Target(const std::string& id) { return L"AIAssistant/" + Widen(id); }

}  // namespace

bool Set(const std::string& id, const std::string& secret) {
  std::wstring target = Target(id);
  CREDENTIALW cred{};
  cred.Type = CRED_TYPE_GENERIC;
  cred.TargetName = target.data();
  cred.CredentialBlobSize = static_cast<DWORD>(secret.size());
  cred.CredentialBlob = reinterpret_cast<LPBYTE>(const_cast<char*>(secret.data()));
  cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
  std::wstring user = L"AIAssistant";
  cred.UserName = user.data();
  return CredWriteW(&cred, 0) != FALSE;
}

std::optional<std::string> Get(const std::string& id) {
  PCREDENTIALW cred = nullptr;
  if (!CredReadW(Target(id).c_str(), CRED_TYPE_GENERIC, 0, &cred)) return std::nullopt;
  std::string secret(reinterpret_cast<const char*>(cred->CredentialBlob), cred->CredentialBlobSize);
  SecureZeroMemory(cred->CredentialBlob, cred->CredentialBlobSize);
  CredFree(cred);
  return secret;
}

bool Has(const std::string& id) {
  PCREDENTIALW cred = nullptr;
  if (!CredReadW(Target(id).c_str(), CRED_TYPE_GENERIC, 0, &cred)) return false;
  CredFree(cred);
  return true;
}

bool Remove(const std::string& id) { return CredDeleteW(Target(id).c_str(), CRED_TYPE_GENERIC, 0) != FALSE; }

}  // namespace aia::SecretStore
