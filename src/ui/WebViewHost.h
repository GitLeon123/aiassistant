#pragma once

#include <windows.h>
#include <objbase.h>  // WIN32_LEAN_AND_MEAN drops COM; WebView2.h needs it first
#include <WebView2.h>
#include <wrl/client.h>

#include <functional>
#include <string>
#include <vector>

namespace aia {

// Owns the shared WebView2 environment (one browser process group for all windows).
class WebViewHost {
 public:
  // Asynchronously creates the environment; done(ok, message) runs on the UI thread.
  void Initialize(std::function<void(bool, const std::string&)> done);
  ICoreWebView2Environment* Environment() const { return env_.Get(); }

  // Page URL for "bar" / "settings": the bundled UI, or AIASSISTANT_DEV_URL (Vite dev server).
  std::wstring PageUrl(const std::wstring& page) const;
  // True if uri belongs to our UI (navigations elsewhere open in the default browser).
  bool IsAppUri(const std::wstring& uri) const;

  static constexpr const wchar_t* kAppHost = L"app.aiassistant";
  static constexpr const wchar_t* kCaptureHost = L"capture.aiassistant";

 private:
  Microsoft::WRL::ComPtr<ICoreWebView2Environment> env_;
  std::wstring devUrl_;
};

}  // namespace aia
