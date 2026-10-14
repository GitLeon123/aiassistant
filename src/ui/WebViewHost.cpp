#include "ui/WebViewHost.h"

#include <wrl.h>

#include "core/Log.h"
#include "core/Paths.h"
#include "core/Util.h"

using namespace Microsoft::WRL;

namespace aia {

void WebViewHost::Initialize(std::function<void(bool, const std::string&)> done) {
  wchar_t dev[1024];
  DWORD n = GetEnvironmentVariableW(L"AIASSISTANT_DEV_URL", dev, ARRAYSIZE(dev));
  if (n > 0 && n < ARRAYSIZE(dev)) {
    devUrl_ = dev;
    while (!devUrl_.empty() && devUrl_.back() == L'/') devUrl_.pop_back();
    Log("Using dev UI at " + Narrow(devUrl_));
  }
  // Transparent from the very first frame (avoids a white flash before put_DefaultBackgroundColor applies).
  SetEnvironmentVariableW(L"WEBVIEW2_DEFAULT_BACKGROUND_COLOR", L"00000000");

  std::wstring userData = LocalDataDir() + L"\\WebView2";
  HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
      nullptr, userData.c_str(), nullptr,
      Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
          [this, done](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result) || !env) {
              char buf[32];
              wsprintfA(buf, "0x%08lX", static_cast<unsigned long>(result));
              done(false, std::string("Couldn't start WebView2 (") + buf + ")");
              return S_OK;
            }
            env_ = env;
            done(true, {});
            return S_OK;
          })
          .Get());
  if (FAILED(hr)) {
    done(false, hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)
                    ? "The Microsoft Edge WebView2 Runtime is not installed."
                    : "Couldn't start WebView2.");
  }
}

std::wstring WebViewHost::PageUrl(const std::wstring& page) const {
  if (!devUrl_.empty()) return devUrl_ + L"/" + page + L".html";
  return std::wstring(L"https://") + kAppHost + L"/" + page + L".html";
}

bool WebViewHost::IsAppUri(const std::wstring& uri) const {
  std::wstring app = std::wstring(L"https://") + kAppHost + L"/";
  if (uri.rfind(app, 0) == 0) return true;
  if (!devUrl_.empty() && uri.rfind(devUrl_ + L"/", 0) == 0) return true;
  return uri == L"about:blank";
}

}  // namespace aia
