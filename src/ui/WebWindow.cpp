#include "ui/WebWindow.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <shlwapi.h>
#include <wrl.h>

#include <algorithm>
#include <cmath>

#include "core/Log.h"
#include "core/Paths.h"
#include "resource.h"

using namespace Microsoft::WRL;

namespace aia {
namespace {

constexpr wchar_t kOverlayClass[] = L"AIAssistant.Overlay";
constexpr wchar_t kSettingsClass[] = L"AIAssistant.Settings";
constexpr UINT WM_APP_WEBVIEW_LOST = WM_APP + 40;

bool EnvFlag(const wchar_t* name) {
  wchar_t buf[8];
  DWORD n = GetEnvironmentVariableW(name, buf, ARRAYSIZE(buf));
  return n > 0 && n < ARRAYSIZE(buf) && buf[0] == L'1';
}

bool DevToolsEnabled() {
#ifndef NDEBUG
  return true;
#else
  return EnvFlag(L"AIASSISTANT_DEVTOOLS");
#endif
}

void OpenExternal(const std::wstring& uri) {
  if (uri.rfind(L"https://", 0) == 0 || uri.rfind(L"http://", 0) == 0 || uri.rfind(L"mailto:", 0) == 0) {
    ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  }
}

std::wstring TakeString(LPWSTR s) {
  std::wstring out = s ? s : L"";
  if (s) CoTaskMemFree(s);
  return out;
}

UINT MonitorDpi(HMONITOR monitor) {
  using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
  static auto fn = reinterpret_cast<GetDpiForMonitorFn>(
      GetProcAddress(LoadLibraryExW(L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32), "GetDpiForMonitor"));
  UINT x = 96, y = 96;
  if (fn) fn(monitor, 0 /* MDT_EFFECTIVE_DPI */, &x, &y);
  return x;
}

}  // namespace

WebWindow::~WebWindow() { Destroy(); }

bool WebWindow::Create(WebViewHost& host, Kind kind, const std::wstring& page, Handlers handlers, bool dark) {
  host_ = &host;
  kind_ = kind;
  page_ = page;
  handlers_ = std::move(handlers);
  dark_ = dark;
  HINSTANCE inst = GetModuleHandleW(nullptr);

  const wchar_t* cls = kind == Kind::Overlay ? kOverlayClass : kSettingsClass;
  WNDCLASSEXW wc{sizeof(wc)};
  if (!GetClassInfoExW(inst, cls, &wc)) {
    wc = {sizeof(wc)};
    wc.lpfnWndProc = &WebWindow::WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = cls;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = LoadIconW(inst, MAKEINTRESOURCEW(IDI_APP));
    wc.hIconSm = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    wc.hbrBackground = kind == Kind::Settings ? static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)) : nullptr;
    if (!RegisterClassExW(&wc)) return false;
  }

  if (kind == Kind::Overlay) {
    // Start at the bar's canvas size (760x680 DIP, as in App::ShowBar) so the page lays out
    // at its real size before the first show.
    double s = GetDpiForSystem() / 96.0;
    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST | WS_EX_NOREDIRECTIONBITMAP, cls, L"AIAssistant",
                            WS_POPUP, -32000, -32000, int(760 * s), int(680 * s), nullptr, nullptr, inst, this);
    if (!hwnd_) return false;
    if (!EnvFlag(L"AIASSISTANT_ALLOW_CAPTURE")) {
      // Best-effort exclusion; Microsoft explicitly does not guarantee content protection.
      if (!SetWindowDisplayAffinity(hwnd_, WDA_EXCLUDEFROMCAPTURE)) {
        Log("Capture exclusion could not be enabled (Win32 " + std::to_string(GetLastError()) + ")");
      }
    }
    DWM_WINDOW_CORNER_PREFERENCE corner = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd_, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
  } else {
    POINT pt;
    GetCursorPos(&pt);
    HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    double s = MonitorDpi(mon) / 96.0;
    RECT work = mi.rcWork;
    int w = std::min<int>(int(980 * s), work.right - work.left - 40);
    int h = std::min<int>(int(700 * s), work.bottom - work.top - 40);
    int x = work.left + (work.right - work.left - w) / 2;
    int y = work.top + (work.bottom - work.top - h) / 2;
    hwnd_ = CreateWindowExW(0, cls, L"AIAssistant Settings", WS_OVERLAPPEDWINDOW, x, y, w, h, nullptr, nullptr,
                            inst, this);
    if (!hwnd_) return false;
  }
  ApplyFrameStyle(dark, false);
  CreateWebView();
  return true;
}

void WebWindow::Destroy() {
  if (hwnd_) DestroyWindow(hwnd_);
  hwnd_ = nullptr;
}

void WebWindow::CreateWebView() {
  std::weak_ptr<bool> alive = alive_;
  HRESULT hr = host_->Environment()->CreateCoreWebView2Controller(
      hwnd_, Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                 [this, alive](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                   if (alive.expired() || !hwnd_) {
                     if (controller) controller->Close();
                     return S_OK;
                   }
                   if (FAILED(result) || !controller) {
                     Log("CreateCoreWebView2Controller failed: " + std::to_string(result));
                     return S_OK;
                   }
                   SetupWebView(controller);
                   return S_OK;
                 })
                 .Get());
  if (FAILED(hr)) Log("CreateCoreWebView2Controller returned " + std::to_string(hr));
}

void WebWindow::SetupWebView(ICoreWebView2Controller* controller) {
  controller_ = controller;
  controller_->get_CoreWebView2(&webview_);

  ComPtr<ICoreWebView2Controller2> c2;
  if (SUCCEEDED(controller_.As(&c2))) c2->put_DefaultBackgroundColor({0, 0, 0, 0});

  ComPtr<ICoreWebView2Settings> settings;
  webview_->get_Settings(&settings);
  BOOL dev = DevToolsEnabled() ? TRUE : FALSE;
  settings->put_AreDevToolsEnabled(dev);
  settings->put_AreDefaultContextMenusEnabled(dev);
  settings->put_IsStatusBarEnabled(FALSE);
  settings->put_IsZoomControlEnabled(FALSE);
  ComPtr<ICoreWebView2Settings3> s3;
  if (SUCCEEDED(settings.As(&s3))) s3->put_AreBrowserAcceleratorKeysEnabled(dev);
  ComPtr<ICoreWebView2Settings5> s5;
  if (SUCCEEDED(settings.As(&s5))) s5->put_IsPinchZoomEnabled(FALSE);
  ComPtr<ICoreWebView2Settings6> s6;
  if (SUCCEEDED(settings.As(&s6))) s6->put_IsSwipeNavigationEnabled(FALSE);

  ComPtr<ICoreWebView2_3> w3;
  if (SUCCEEDED(webview_.As(&w3))) {
    std::wstring folder = ExecutableDir() + L"\\ui";
    w3->SetVirtualHostNameToFolderMapping(WebViewHost::kAppHost, folder.c_str(),
                                          COREWEBVIEW2_HOST_RESOURCE_ACCESS_KIND_DENY_CORS);
  }

  // In-memory images (backdrop, screen thumbnail), served without touching disk.
  std::wstring filter = std::wstring(L"https://") + WebViewHost::kCaptureHost + L"/*";
  webview_->AddWebResourceRequestedFilter(filter.c_str(), COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
  webview_->add_WebResourceRequested(
      Callback<ICoreWebView2WebResourceRequestedEventHandler>(
          [this](ICoreWebView2*, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
            ComPtr<ICoreWebView2WebResourceRequest> request;
            args->get_Request(&request);
            LPWSTR rawUri = nullptr;
            request->get_Uri(&rawUri);
            std::wstring uri = TakeString(rawUri);
            std::wstring prefix = std::wstring(L"https://") + WebViewHost::kCaptureHost + L"/";
            std::wstring path = uri.size() > prefix.size() ? uri.substr(prefix.size()) : L"";
            if (auto q = path.find_first_of(L"?#"); q != std::wstring::npos) path.resize(q);
            std::wstring mime;
            std::vector<uint8_t> data;
            ComPtr<ICoreWebView2WebResourceResponse> response;
            if (handlers_.resource && handlers_.resource(path, mime, data)) {
              ComPtr<IStream> stream;
              stream.Attach(SHCreateMemStream(data.data(), static_cast<UINT>(data.size())));
              std::wstring headers =
                  L"Content-Type: " + mime + L"\r\nCache-Control: no-store\r\nAccess-Control-Allow-Origin: *";
              host_->Environment()->CreateWebResourceResponse(stream.Get(), 200, L"OK", headers.c_str(), &response);
            } else {
              host_->Environment()->CreateWebResourceResponse(nullptr, 404, L"Not Found", L"", &response);
            }
            args->put_Response(response.Get());
            return S_OK;
          })
          .Get(),
      nullptr);

  webview_->add_WebMessageReceived(
      Callback<ICoreWebView2WebMessageReceivedEventHandler>(
          [this](ICoreWebView2*, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
            LPWSTR rawSource = nullptr;
            args->get_Source(&rawSource);
            if (!host_->IsAppUri(TakeString(rawSource))) return S_OK;
            LPWSTR rawJson = nullptr;
            if (FAILED(args->get_WebMessageAsJson(&rawJson))) return S_OK;
            json msg = json::parse(Narrow(TakeString(rawJson)), nullptr, false);
            if (!msg.is_object()) return S_OK;
            if (msg.value("type", "") == "ready") {
              pageReady_ = true;
              if (handlers_.message) handlers_.message(msg);
              auto pending = std::move(queued_);
              queued_.clear();
              for (auto& m : pending) Post(m);
              return S_OK;
            }
            if (handlers_.message) handlers_.message(msg);
            return S_OK;
          })
          .Get(),
      nullptr);

  webview_->add_NavigationStarting(
      Callback<ICoreWebView2NavigationStartingEventHandler>(
          [this](ICoreWebView2*, ICoreWebView2NavigationStartingEventArgs* args) -> HRESULT {
            LPWSTR rawUri = nullptr;
            args->get_Uri(&rawUri);
            std::wstring uri = TakeString(rawUri);
            if (!host_->IsAppUri(uri)) {
              args->put_Cancel(TRUE);
              OpenExternal(uri);
            } else {
              pageReady_ = false;
            }
            return S_OK;
          })
          .Get(),
      nullptr);

  webview_->add_NewWindowRequested(
      Callback<ICoreWebView2NewWindowRequestedEventHandler>(
          [](ICoreWebView2*, ICoreWebView2NewWindowRequestedEventArgs* args) -> HRESULT {
            LPWSTR rawUri = nullptr;
            args->get_Uri(&rawUri);
            args->put_Handled(TRUE);
            OpenExternal(TakeString(rawUri));
            return S_OK;
          })
          .Get(),
      nullptr);

  webview_->add_NavigationCompleted(
      Callback<ICoreWebView2NavigationCompletedEventHandler>(
          [](ICoreWebView2*, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
            BOOL ok = TRUE;
            args->get_IsSuccess(&ok);
            if (!ok) {
              COREWEBVIEW2_WEB_ERROR_STATUS status{};
              args->get_WebErrorStatus(&status);
              Log("UI navigation failed, status " + std::to_string(int(status)));
            }
            return S_OK;
          })
          .Get(),
      nullptr);

  webview_->add_ProcessFailed(
      Callback<ICoreWebView2ProcessFailedEventHandler>(
          [this](ICoreWebView2*, ICoreWebView2ProcessFailedEventArgs* args) -> HRESULT {
            COREWEBVIEW2_PROCESS_FAILED_KIND kind{};
            args->get_ProcessFailedKind(&kind);
            Log("WebView2 process failed, kind " + std::to_string(int(kind)));
            if (kind == COREWEBVIEW2_PROCESS_FAILED_KIND_BROWSER_PROCESS_EXITED) {
              PostMessageW(hwnd_, WM_APP_WEBVIEW_LOST, 0, 0);
            } else if (kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_EXITED ||
                       kind == COREWEBVIEW2_PROCESS_FAILED_KIND_RENDER_PROCESS_UNRESPONSIVE) {
              webview_->Reload();
            }
            return S_OK;
          })
          .Get(),
      nullptr);

  ResizeWebView();
  controller_->put_IsVisible(TRUE);
  webview_->Navigate(host_->PageUrl(page_).c_str());
}

void WebWindow::Post(const json& message) {
  if (!webview_ || !pageReady_) {
    if (queued_.size() < 64) queued_.push_back(message);
    return;
  }
  std::string text = message.dump(-1, ' ', false, json::error_handler_t::replace);
  webview_->PostWebMessageAsJson(Widen(text).c_str());
}

void WebWindow::FocusWebView() {
  if (controller_) controller_->MoveFocus(COREWEBVIEW2_MOVE_FOCUS_REASON_PROGRAMMATIC);
}

UINT WebWindow::Dpi() const { return hwnd_ ? GetDpiForWindow(hwnd_) : 96; }

void WebWindow::ApplyFrameStyle(bool dark, bool systemBackdrop) {
  dark_ = dark;
  systemBackdrop_ = systemBackdrop;
  if (!hwnd_) return;
  BOOL useDark = dark ? TRUE : FALSE;
  DwmSetWindowAttribute(hwnd_, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
  DWM_SYSTEMBACKDROP_TYPE type;
  MARGINS margins{-1, -1, -1, -1};
  if (kind_ == Kind::Settings) {
    type = DWMSBT_MAINWINDOW;  // Mica
  } else if (systemBackdrop) {
    type = DWMSBT_TRANSIENTWINDOW;  // Acrylic
  } else {
    type = DWMSBT_NONE;
    margins = {0, 0, 0, 0};
  }
  DwmExtendFrameIntoClientArea(hwnd_, &margins);
  DwmSetWindowAttribute(hwnd_, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
  ApplyPendingRegion();
}

void WebWindow::SetHitRegion(double x, double y, double width, double height, double radius) {
  region_ = {x, y, width, height, radius, true};
  ApplyPendingRegion();
}

void WebWindow::ClearHitRegion() {
  region_.set = false;
  if (hwnd_) SetWindowRgn(hwnd_, nullptr, TRUE);
}

void WebWindow::ApplyPendingRegion() {
  if (!hwnd_ || !region_.set || kind_ != Kind::Overlay) return;
  double s = Dpi() / 96.0;
  int l = int(std::floor(region_.x * s)), t = int(std::floor(region_.y * s));
  int r = int(std::ceil((region_.x + region_.w) * s)), b = int(std::ceil((region_.y + region_.h) * s));
  int d = std::max(0, int(std::round(region_.r * 2 * s)));
  HRGN rgn = d > 0 ? CreateRoundRectRgn(l, t, r + 1, b + 1, d, d) : CreateRectRgn(l, t, r, b);
  if (!SetWindowRgn(hwnd_, rgn, TRUE)) DeleteObject(rgn);
}

void WebWindow::ShowOverlay(int x, int y, int w, int h) {
  if (!hwnd_) return;
  SetWindowPos(hwnd_, HWND_TOPMOST, x, y, w, h, SWP_NOACTIVATE);
  ApplyPendingRegion();
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
  FocusWebView();
}

void WebWindow::Hide() {
  if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
}

void WebWindow::ShowSettings() {
  if (!hwnd_) return;
  if (IsIconic(hwnd_)) ShowWindow(hwnd_, SW_RESTORE);
  ShowWindow(hwnd_, SW_SHOW);
  SetForegroundWindow(hwnd_);
  FocusWebView();
}

void WebWindow::ResizeWebView() {
  if (!controller_ || !hwnd_) return;
  RECT rc;
  GetClientRect(hwnd_, &rc);
  controller_->put_Bounds(rc);
}

LRESULT CALLBACK WebWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
    auto* self = static_cast<WebWindow*>(cs->lpCreateParams);
    self->hwnd_ = hwnd;
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  auto* self = reinterpret_cast<WebWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
  if (msg == WM_NCDESTROY) {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    self->hwnd_ = nullptr;
    return DefWindowProcW(hwnd, msg, wp, lp);
  }
  return self->Handle(msg, wp, lp);
}

LRESULT WebWindow::Handle(UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
    case WM_SIZE:
      ResizeWebView();
      if (controller_) controller_->put_IsVisible(wp != SIZE_MINIMIZED);
      return 0;
    case WM_MOVE:
      if (controller_) controller_->NotifyParentWindowPositionChanged();
      return 0;
    case WM_ACTIVATE: {
      bool active = LOWORD(wp) != WA_INACTIVE;
      if (active && kind_ == Kind::Settings) FocusWebView();
      if (handlers_.activated) handlers_.activated(active);
      return 0;
    }
    case WM_DPICHANGED:
      if (kind_ == Kind::Settings) {
        auto* r = reinterpret_cast<RECT*>(lp);
        SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
      } else {
        ApplyPendingRegion();
      }
      return 0;
    case WM_GETMINMAXINFO:
      if (kind_ == Kind::Settings) {
        double s = Dpi() / 96.0;
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lp);
        MONITORINFO mi{sizeof(mi)};
        GetMonitorInfoW(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTONEAREST), &mi);
        mmi->ptMinTrackSize = {std::min(LONG(720 * s), mi.rcWork.right - mi.rcWork.left - 40),
                               std::min(LONG(520 * s), mi.rcWork.bottom - mi.rcWork.top - 40)};
        return 0;
      }
      break;
    case WM_ERASEBKGND:
      if (kind_ == Kind::Settings) {
        // Black is fully transparent inside the extended frame, so Mica shows through.
        RECT rc;
        GetClientRect(hwnd_, &rc);
        FillRect(reinterpret_cast<HDC>(wp), &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
      }
      return 1;
    case WM_CLOSE:
      if (kind_ == Kind::Overlay) {
        Post({{"type", "dismiss"}});
        return 0;
      }
      DestroyWindow(hwnd_);
      return 0;
    case WM_DESTROY:
      if (controller_) controller_->Close();
      controller_.Reset();
      webview_.Reset();
      pageReady_ = false;
      if (handlers_.closed) handlers_.closed();
      return 0;
    case WM_APP_WEBVIEW_LOST:
      if (handlers_.webviewLost) handlers_.webviewLost();
      return 0;
    default:
      break;
  }
  return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace aia
