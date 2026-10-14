#pragma once

#include <windows.h>
#include <objbase.h>  // WIN32_LEAN_AND_MEAN drops COM; WebView2.h needs it first
#include <WebView2.h>
#include <wrl/client.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/Util.h"
#include "ui/WebViewHost.h"

namespace aia {

// A top-level window hosting one transparent WebView2.
//  Overlay:  borderless, topmost, tool window, no redirection bitmap (per-pixel transparency).
//  Settings: normal resizable window with a Mica backdrop showing through the transparent page.
class WebWindow {
 public:
  enum class Kind { Overlay, Settings };

  struct Handlers {
    std::function<void(const json&)> message;
    std::function<void(bool active)> activated;
    std::function<void()> closed;  // Settings window destroyed
    std::function<void()> webviewLost;  // Browser process died; recreate the window
    // Serves https://capture.aiassistant/<path>. Return false for 404.
    std::function<bool(const std::wstring& path, std::wstring& mime, std::vector<uint8_t>& data)> resource;
  };

  WebWindow() = default;
  ~WebWindow();
  WebWindow(const WebWindow&) = delete;
  WebWindow& operator=(const WebWindow&) = delete;

  bool Create(WebViewHost& host, Kind kind, const std::wstring& page, Handlers handlers, bool dark);
  void Destroy();

  HWND Hwnd() const { return hwnd_; }
  bool Ready() const { return webview_ != nullptr; }
  void Post(const json& message);
  void FocusWebView();
  UINT Dpi() const;

  // Theme / backdrop styling for the native frame.
  void ApplyFrameStyle(bool dark, bool systemBackdrop);
  // Restricts the window's hit-test and visible area (CSS pixels relative to the client area).
  void SetHitRegion(double x, double y, double width, double height, double radius);
  void ClearHitRegion();

  // Overlay: shows the window at (x, y) with size (w, h) in physical pixels.
  void ShowOverlay(int x, int y, int w, int h);
  void Hide();
  bool Visible() const { return hwnd_ && IsWindowVisible(hwnd_); }
  void ShowSettings();

 private:
  static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT Handle(UINT msg, WPARAM wp, LPARAM lp);
  void CreateWebView();
  void SetupWebView(ICoreWebView2Controller* controller);
  void ResizeWebView();
  void ApplyPendingRegion();

  WebViewHost* host_ = nullptr;
  Kind kind_ = Kind::Overlay;
  std::wstring page_;
  Handlers handlers_;
  HWND hwnd_ = nullptr;
  bool dark_ = false;
  bool systemBackdrop_ = false;
  Microsoft::WRL::ComPtr<ICoreWebView2Controller> controller_;
  Microsoft::WRL::ComPtr<ICoreWebView2> webview_;
  std::vector<json> queued_;  // Messages posted before the page said "ready"
  bool pageReady_ = false;
  std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
  struct Region {
    double x = 0, y = 0, w = 0, h = 0, r = 0;
    bool set = false;
  } region_;
};

}  // namespace aia
