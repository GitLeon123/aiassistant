#include "app/App.h"

#include <shellapi.h>

#include <algorithm>
#include <condition_variable>
#include <thread>

#include "core/HotkeySpec.h"
#include "core/CapturePolicy.h"
#include "core/ClipboardFormat.h"
#include "core/UsageBudget.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "platform/Autostart.h"
#include "platform/SecretStore.h"
#include "platform/Process.h"
#include "providers/Catalog.h"
#include "providers/Setup.h"
#include "resource.h"

#ifndef AIASSISTANT_VERSION
#define AIASSISTANT_VERSION "dev"
#endif

namespace aia {
namespace {

constexpr UINT WM_APP_INVOKE = WM_APP + 1;
constexpr UINT WM_APP_TRAY = WM_APP + 2;
constexpr int kHotkeyId = 1;
constexpr UINT_PTR kFlushTimer = 1;
constexpr UINT_PTR kBackdropTimer = 2;
constexpr UINT_PTR kHideFallbackTimer = 3;

// Overlay canvas in DIPs. The glass panel lives inside it; the rest is transparent and click-through.
constexpr int kBarWidthDip = 760;
constexpr int kBarHeightDip = 680;
constexpr int kBarPanelTopDip = 36;  // Where the panel starts inside the canvas (matches the UI's CSS)
constexpr int kBarPanelLeftDip = 40;  // The panel is centred horizontally in the canvas
constexpr int kBarPanelWidthDip = 680;
constexpr int kBarEdgeDip = 32;  // Gap between the panel and the left/right edge of the work area

// Places the canvas for one of the nine "barPosition" values ("top|middle|bottom-left|center|right").
// The top row keeps the panel's top edge at 20% of the work area. The UI keeps the panel attached to
// the chosen vertical edge (or centre) while it grows.
void BarOrigin(const std::string& position, const RECT& work, double s, int w, int h, int& x, int& y) {
  if (position.find("left") != std::string::npos) {
    x = work.left + int(kBarEdgeDip * s) - int(kBarPanelLeftDip * s);
  } else if (position.find("right") != std::string::npos) {
    x = work.right - int(kBarEdgeDip * s) - int((kBarPanelLeftDip + kBarPanelWidthDip) * s);
  } else {
    x = work.left + (work.right - work.left - w) / 2;
  }
  if (position.rfind("top", 0) == 0) {
    y = work.top + int((work.bottom - work.top) * 0.2) - int(kBarPanelTopDip * s);
  } else if (position.rfind("bottom", 0) == 0) {
    y = work.bottom - h;
  } else {
    y = work.top + (work.bottom - work.top - h) / 2;
  }
  x = std::clamp<int>(x, work.left, std::max<LONG>(work.left, work.right - w));
  y = std::clamp<int>(y, work.top, std::max<LONG>(work.top, work.bottom - h));
}

constexpr auto kModelCacheTtl = std::chrono::minutes(15);

// Screenshot sent as context: longest side in pixels and JPEG quality.
constexpr int kContextMaxSide = 1568;
constexpr float kContextQuality = 0.85f;

const char* KindName(ProviderKind k) {
  switch (k) {
    case ProviderKind::Api: return "api";
    case ProviderKind::Local: return "local";
    case ProviderKind::Cli: return "cli";
  }
  return "api";
}

UINT MonitorDpi(HMONITOR monitor) {
  using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
  static auto fn = reinterpret_cast<GetDpiForMonitorFn>(
      GetProcAddress(LoadLibraryExW(L"shcore.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32), "GetDpiForMonitor"));
  UINT x = 96, y = 96;
  if (fn) fn(monitor, 0, &x, &y);
  return x;
}

std::string Today() {
  SYSTEMTIME st;
  GetLocalTime(&st);
  char buf[32];
  wsprintfA(buf, "%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
  return buf;
}

bool EnvFlag(const wchar_t* name) {
  wchar_t buf[8];
  DWORD n = GetEnvironmentVariableW(name, buf, ARRAYSIZE(buf));
  return n > 0 && n < ARRAYSIZE(buf) && buf[0] == L'1';
}

std::string Trim(std::string s) {
  auto ws = [](unsigned char c) { return std::isspace(c) != 0; };
  while (!s.empty() && ws(s.back())) s.pop_back();
  size_t i = 0;
  while (i < s.size() && ws(s[i])) ++i;
  return s.substr(i);
}

}  // namespace

// ---------------------------------------------------------------------------
// Startup / message loop

int App::Run(const LaunchOptions& options) {
  launch_ = options;
  bool firstRun = GetFileAttributesW((RoamingDataDir() + L"\\settings.json").c_str()) == INVALID_FILE_ATTRIBUTES;
  settings_.Load();
  if (firstRun) settings_.Save();
  theme_ = QueryTheme();
  screenMode_ = settings_.Data().value("screenMode", "monitor");
  allowCapture_ = EnvFlag(L"AIASSISTANT_ALLOW_CAPTURE");
  Log(std::string("Starting AIAssistant ") + AIASSISTANT_VERSION);

  HINSTANCE inst = GetModuleHandleW(nullptr);
  WNDCLASSEXW wc{sizeof(wc)};
  wc.lpfnWndProc = &App::HostProc;
  wc.hInstance = inst;
  wc.lpszClassName = kHostClass;
  RegisterClassExW(&wc);
  // A hidden top-level window (not message-only) so it receives broadcasts such as TaskbarCreated
  // and WM_SETTINGCHANGE, and so a second instance can find it.
  host_ = CreateWindowExW(WS_EX_TOOLWINDOW, kHostClass, L"AIAssistant", WS_POPUP, 0, 0, 0, 0, nullptr, nullptr,
                          inst, this);
  if (!host_) return 1;
  taskbarCreatedMsg_ = RegisterWindowMessageW(L"TaskbarCreated");

  HICON icon = static_cast<HICON>(LoadImageW(inst, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                             GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
  tray_.Create(host_, WM_APP_TRAY, icon, L"AIAssistant (" + Widen(settings_.Hotkey()) + L")");
  RegisterCurrentHotkey();

  bool configured = !settings_.Data()["defaultModel"].value("provider", "").empty();
  if (options.settings || (!options.background && (!settings_.Data().value("setupCompleted", false) || !configured))) pendingSettings_ = true;
  if (options.toggle || (!options.background && !pendingSettings_)) pendingShow_ = true;

  webHost_.Initialize([this](bool ok, const std::string& error) {
    if (!ok) {
      Log(error);
      MessageBoxW(nullptr,
                  (Widen(error) + L"\n\nInstall the Evergreen WebView2 Runtime from "
                                  L"https://developer.microsoft.com/microsoft-edge/webview2/ and try again.")
                      .c_str(),
                  L"AIAssistant", MB_ICONERROR);
      Quit();
      return;
    }
    webReady_ = true;
    CreateOverlay();
    if (pendingSettings_) {
      pendingSettings_ = false;
      OpenSettings();
    }
  });

  MSG msg;
  while (GetMessageW(&msg, nullptr, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessageW(&msg);
  }

  // Give cancelled workers a moment to unwind; anything still blocked is torn down with the process.
  for (int i = 0; i < 40 && workers_.load() > 0; ++i) Sleep(25);
  if (workers_.load() > 0) {
    Log("Exiting with workers still running");
    ExitProcess(0);
  }
  return 0;
}

void App::Quit() {
  if (quitting_) return;
  quitting_ = true;
  CancelStream();
  if (setupCancel_) setupCancel_->Cancel();
  if (hotkeyRegistered_) UnregisterHotKey(host_, kHotkeyId);
  tray_.Remove();
  settingsWindow_.reset();
  overlay_.reset();
  PostQuitMessage(0);
}

void App::PostToUi(std::function<void()> fn) {
  auto* p = new std::function<void()>(std::move(fn));
  if (!host_ || !PostMessageW(host_, WM_APP_INVOKE, 0, reinterpret_cast<LPARAM>(p))) delete p;
}

void App::RunAsync(std::function<void()> fn) {
  ++workers_;
  std::thread([this, fn = std::move(fn)] {
    try {
      fn();
    } catch (const std::exception& e) {
      Log(std::string("Worker failed: ") + e.what());
    }
    --workers_;
  }).detach();
}

LRESULT CALLBACK App::HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == WM_NCCREATE) {
    auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
  }
  auto* self = reinterpret_cast<App*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
  if (self && self->host_ == hwnd) return self->HandleHost(msg, wp, lp);
  return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT App::HandleHost(UINT msg, WPARAM wp, LPARAM lp) {
  if (msg == taskbarCreatedMsg_ && taskbarCreatedMsg_ != 0) {
    tray_.Recreate();
    return 0;
  }
  switch (msg) {
    case WM_APP_INVOKE: {
      std::unique_ptr<std::function<void()>> fn(reinterpret_cast<std::function<void()>*>(lp));
      if (!quitting_) (*fn)();
      return 0;
    }
    case WM_HOTKEY:
      if (wp == kHotkeyId) ToggleBar();
      return 0;
    case WM_APP_TRAY:
      switch (LOWORD(lp)) {
        case NIN_SELECT:
        case NIN_KEYSELECT:
          ToggleBar();
          break;
        case WM_CONTEXTMENU: {
          UINT cmd = tray_.ShowMenu(IsAutostartEnabled(), Widen(settings_.Hotkey()), theme_.dark);
          if (cmd == kTrayAsk) ShowBar();
          if (cmd == kTraySettings) OpenSettings();
          if (cmd == kTrayAutostart) {
            SetAutostartEnabled(!IsAutostartEnabled());
            Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
          }
          if (cmd == kTrayQuit) Quit();
          break;
        }
      }
      return 0;
    case WM_SETTINGCHANGE:
      if (IsThemeChangeMessage(reinterpret_cast<const wchar_t*>(lp))) {
        ThemeInfo t = QueryTheme();
        if (!(t == theme_)) {
          theme_ = t;
          ApplyAppearance();
          Broadcast({{"type", "theme"}, {"theme", ToJson(theme_)}});
        }
      }
      return 0;
    case WM_COPYDATA: {
      auto* cds = reinterpret_cast<COPYDATASTRUCT*>(lp);
      if (!cds || cds->dwData != kCopyDataMagic) return FALSE;
      std::string cmd(static_cast<const char*>(cds->lpData), cds->cbData);
      // Defer: WM_COPYDATA is sent synchronously from the other process.
      PostToUi([this, cmd] {
        if (cmd == "toggle") ToggleBar();
        else if (cmd == "show") ShowBar();
        else OpenSettings();
      });
      return TRUE;
    }
    case WM_TIMER:
      if (wp == kFlushTimer) FlushDeltas();
      if (wp == kBackdropTimer) RefreshBackdrop();
      if (wp == kHideFallbackTimer) {
        KillTimer(host_, kHideFallbackTimer);
        if (overlay_ && overlay_->Visible()) OnBarHidden(false);
      }
      return 0;
    case WM_QUERYENDSESSION:
      return TRUE;
    case WM_ENDSESSION:
      if (wp) Quit();
      return 0;
    default:
      break;
  }
  return DefWindowProcW(host_, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Windows

void App::CreateOverlay() {
  overlay_ = std::make_unique<WebWindow>();
  WebWindow* w = overlay_.get();
  WebWindow::Handlers h;
  h.message = [this, w](const json& m) { OnMessage(w, m); };
  h.activated = [this](bool active) {
    if (!active && overlay_ && overlay_->Visible() && !settings_.Data().value("barPinned", false)) DismissBar();
  };
  h.resource = [this](const std::wstring& path, std::wstring& mime, std::vector<uint8_t>& data) {
    return ResourceFor(path, mime, data);
  };
  h.webviewLost = [this] {
    PostToUi([this] {
      overlay_.reset();
      CreateOverlay();
    });
  };
  if (!overlay_->Create(webHost_, WebWindow::Kind::Overlay, L"bar", std::move(h), EffectiveDark())) {
    Log("Failed to create the overlay window");
    return;
  }
  overlay_->ApplyFrameStyle(EffectiveDark(), !settings_.Data().value("solidSurfaces", false) && settings_.Data().value("backdrop", "liquid") == "system");
}

void App::OpenSettings() {
  if (!webReady_) {
    pendingSettings_ = true;
    return;
  }
  if (overlay_ && overlay_->Visible()) DismissBar();
  if (settingsWindow_ && settingsWindow_->Hwnd()) {
    settingsWindow_->ShowSettings();
    return;
  }
  settingsWindow_ = std::make_unique<WebWindow>();
  WebWindow* w = settingsWindow_.get();
  WebWindow::Handlers h;
  h.message = [this, w](const json& m) { OnMessage(w, m); };
  h.closed = [this] {
    if (setupCancel_) setupCancel_->Cancel();
    // Destroy the C++ object after the window procedure has unwound.
    PostToUi([this] {
      if (settingsWindow_ && !settingsWindow_->Hwnd()) settingsWindow_.reset();
      if (!hotkeyRegistered_) RegisterCurrentHotkey();  // In case the recorder was left suspended
    });
  };
  h.webviewLost = [this] {
    PostToUi([this] {
      settingsWindow_.reset();
      OpenSettings();
    });
  };
  if (!settingsWindow_->Create(webHost_, WebWindow::Kind::Settings, L"settings", std::move(h), EffectiveDark())) {
    settingsWindow_.reset();
    return;
  }
  settingsWindow_->ShowSettings();
}

bool App::EffectiveDark() const {
  std::string a = settings_.Data().value("appearance", "system");
  if (a == "dark") return true;
  if (a == "light") return false;
  return theme_.dark;
}

void App::ApplyAppearance() {
  bool system = !settings_.Data().value("solidSurfaces", false) && settings_.Data().value("backdrop", "liquid") == "system";
  if (overlay_) overlay_->ApplyFrameStyle(EffectiveDark(), system);
  if (settingsWindow_) settingsWindow_->ApplyFrameStyle(EffectiveDark(), false);
}

void App::Broadcast(const json& msg) {
  if (overlay_) overlay_->Post(msg);
  if (settingsWindow_ && settingsWindow_->Hwnd()) settingsWindow_->Post(msg);
}

json App::ProvidersJson() const {
  json arr = json::array();
  for (const auto& p : AllProviders()) {
    json j = {{"id", p.id},
              {"name", p.name},
              {"kind", KindName(p.kind)},
              {"description", p.description},
              {"defaultBaseUrl", p.defaultBaseUrl},
              {"needsKey", p.needsKey},
              {"keyOptional", p.keyOptional},
              {"baseUrlEditable", p.baseUrlEditable},
              {"keyUrl", p.keyUrl},
              {"defaultModel", p.defaultModel},
              {"hasKey", (p.needsKey || p.keyOptional) && SecretStore::Has(p.id)}};
    if (auto it = cliStatus_.find(p.id); it != cliStatus_.end()) {
      j["cli"] = {{"installed", it->second.installed},
                  {"signedIn", it->second.signedIn},
                  {"path", it->second.path},
                  {"detail", it->second.detail}};
    }
    json connection = {{"state", "not-connected"}, {"detail", ""}, {"nextStep", "Add an API key."}};
    if (p.kind == ProviderKind::Cli) {
      auto it = cliStatus_.find(p.id);
      if (it == cliStatus_.end()) connection = {{"state", "checking"}, {"detail", "Looking for an existing sign-in…"}, {"nextStep", ""}};
      else connection = {{"state", it->second.signedIn ? "ready" : "not-connected"},
                         {"detail", it->second.detail},
                         {"nextStep", !it->second.installed ? "Open the setup guide to install the provider's app."
                                     : !it->second.signedIn ? "Sign in to the provider's app, then check again."
                                                           : "Send a test question to confirm it can answer."}};
    } else if (p.kind == ProviderKind::Local) {
      connection = {{"state", "unchecked"}, {"detail", ""}, {"nextStep", "Start the local server and load a model."}};
    } else if (j["hasKey"].get<bool>() || settings_.ProviderConfig(p.id).value("enabled", false)) {
      connection = {{"state", "ready"}, {"detail", j["hasKey"].get<bool>() ? "API key saved" : "Endpoint configured"},
                    {"nextStep", "Test the connection."}};
    }
    if (auto it = connections_.find(p.id); it != connections_.end()) connection = it->second;
    j["connection"] = connection;
    arr.push_back(std::move(j));
  }
  return arr;
}

json App::SettingsPayload() const {
  return {{"settings", settings_.Data()},
          {"budget", BudgetPayload()},
          {"providers", ProvidersJson()},
          {"autostart", IsAutostartEnabled()},
          {"hotkeyError", hotkeyError_},
          {"hotkeySuggestions", HotkeySuggestions()},
          {"discovery", discovery_},
          {"defaultSystemPrompt", Settings::DefaultSystemPrompt()},
          {"defaultCaptureBlocklist", DefaultCaptureBlocklist()}};
}

void App::SendInit(WebWindow* to, const std::string& page) {
  json init = {{"type", "init"},
               {"page", page},
               {"version", AIASSISTANT_VERSION},
               {"theme", ToJson(theme_)},
               {"payload", SettingsPayload()},
               {"liveBackdrop", !allowCapture_}};
  to->Post(init);
  if (page == "settings" && !discovery_.value("complete", false)) DiscoverProviders();
  // Warm the model lists for every provider the user has set up.
  for (const auto& p : AllProviders()) {
    json cfg = settings_.ProviderConfig(p.id);
    bool enabled = cfg.value("enabled", false) || ((p.needsKey || p.keyOptional) && SecretStore::Has(p.id));
    if (enabled || settings_.Data()["defaultModel"].value("provider", "") == p.id) ListModels(p.id, false);
  }
}

// ---------------------------------------------------------------------------
// Bar show / hide

void App::ToggleBar() {
  if (overlay_ && overlay_->Visible()) {
    DismissBar();
  } else {
    ShowBar();
  }
}

void App::ShowBar() {
  if (!overlay_ || !overlay_->Ready()) {
    pendingShow_ = true;
    return;
  }
  pendingShow_ = false;
  KillTimer(host_, kHideFallbackTimer);
  if (overlay_->Visible()) {
    overlay_->ShowOverlay(overlayRect_.left, overlayRect_.top, overlayRect_.right - overlayRect_.left,
                          overlayRect_.bottom - overlayRect_.top);
    return;
  }

  auto t0 = std::chrono::steady_clock::now();
  // Remember what the user was looking at before we take focus.
  HWND fg = GetForegroundWindow();
  bool ours = fg == overlay_->Hwnd() || (settingsWindow_ && fg == settingsWindow_->Hwnd());
  previousForeground_ = ours ? nullptr : fg;
  foreground_ = ours ? ForegroundInfo{} : QueryForegroundWindow();

  POINT cursor;
  GetCursorPos(&cursor);
  HMONITOR mon = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
  MONITORINFO mi{sizeof(mi)};
  GetMonitorInfoW(mon, &mi);
  double s = MonitorDpi(mon) / 96.0;
  RECT work = mi.rcWork;
  int w = std::min<int>(int(kBarWidthDip * s), work.right - work.left);
  int h = std::min<int>(int(kBarHeightDip * s), work.bottom - work.top);
  std::string position = settings_.Data().value("barPosition", "top-center");
  int x = 0, y = 0;
  BarOrigin(position, work, s, w, h, x, y);
  overlayRect_ = {x, y, x + w, y + h};

  // Check BEFORE GDI reads any pixels, including pixels used for the glass backdrop.
  ClearCapture();
  monitorNumber_ = MonitorNumber(mon);
  captureError_ = CapturePermission();
  if (captureError_.empty()) {
    auto shot = std::make_shared<ScreenImage>(CaptureMonitor(mon));
    // Reject a focus/title change during capture as well.
    captureError_ = CapturePermission();
    if (captureError_.empty()) {
      snapshot_ = shot->Empty() ? nullptr : shot;
      live_ = snapshot_;
      if (!snapshot_) captureError_ = "Screen capture unavailable. Reopen the bar to try again.";
      PrepareContextImage();
    }
  }

  // Conversations expire after a period of inactivity.
  bool reset = false;
  int timeout = settings_.ConversationTimeoutMinutes();
  if (!history_.empty() && !active_ &&
      (timeout == 0 || std::chrono::steady_clock::now() - lastActivity_ > std::chrono::minutes(timeout))) {
    ResetConversation();
    reset = true;
  }

  bool solid = settings_.Data().value("solidSurfaces", false);
  bool liquid = !solid && settings_.Data().value("backdrop", "liquid") == "liquid";
  overlay_->ApplyFrameStyle(EffectiveDark(), !solid && !liquid);
  std::string v = std::to_string(captureVersion_);
  overlay_->Post({{"type", "show"},
                  {"backdrop", liquid && snapshot_ ? "https://capture.aiassistant/backdrop.jpg?v=" + v : ""},
                  {"screen", ScreenPayload()},
                  {"screenDefault", settings_.Data().value("screenContextDefault", true)},
                  {"conversationReset", reset},
                  {"position", position},
                  {"scale", s}});
  overlay_->ShowOverlay(x, y, w, h);
  if (snapshot_ && liquid && !allowCapture_ && settings_.Data().value("liveBackdrop", true)) {
    SetTimer(host_, kBackdropTimer, 1200, nullptr);
  }
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
  Log("Bar shown in " + std::to_string(ms) + " ms");
}

void App::DismissBar() {
  if (!overlay_ || !overlay_->Visible()) return;
  overlay_->Post({{"type", "dismiss"}});
  // If the page doesn't answer (e.g. it crashed), hide anyway.
  SetTimer(host_, kHideFallbackTimer, 700, nullptr);
}

void App::OnBarHidden(bool restoreFocus) {
  KillTimer(host_, kHideFallbackTimer);
  KillTimer(host_, kBackdropTimer);
  if (!overlay_) return;
  bool wasActive = GetForegroundWindow() == overlay_->Hwnd();
  overlay_->Hide();
  if (restoreFocus && wasActive && previousForeground_ && IsWindow(previousForeground_)) {
    SetForegroundWindow(previousForeground_);
  }
  ClearCapture();
  // Don't leave a CLI idling in the background; the next show starts a fresh one.
  RunAsync([] { DiscardPreparedCli(); });
}

void App::ClearCapture() {
  ++screenVersion_;
  ++captureVersion_;
  reviewedScreenVersion_ = -1;
  snapshot_.reset();
  live_.reset();
  backdropJpeg_.clear();
  backdropJpeg_.shrink_to_fit();
  backdropFor_ = -1;
  contextJpeg_.reset();
  contextJpegSource_ = nullptr;
}

// ---------------------------------------------------------------------------
// Screen capture

std::string App::CapturePermission() const {
  const bool enabled = settings_.Data().value("screenCaptureProtection", true);
  const auto& rules = settings_.Data()["screenCaptureBlocklist"];
  auto source = QueryWindow(foreground_.hwnd);
  auto reason = CaptureBlockReason(enabled, rules, source.process, source.title);
  if (!reason.empty()) return reason;
  auto current = QueryForegroundWindow();
  if (current.hwnd != foreground_.hwnd && (!overlay_ || current.hwnd != overlay_->Hwnd()) &&
      (!settingsWindow_ || current.hwnd != settingsWindow_->Hwnd())) {
    reason = CaptureBlockReason(enabled, rules, current.process, current.title);
  }
  return reason;
}

void App::BlockCapture(const std::string& reason) {
  KillTimer(host_, kBackdropTimer);
  ClearCapture();
  captureError_ = reason;
  if (overlay_) {
    overlay_->Post({{"type", "backdrop"}, {"url", ""}});
    overlay_->Post({{"type", "screen"}, {"screen", ScreenPayload()}});
  }
}

json App::ScreenPayload() const {
  RECT region = ContextRegion();
  const bool ready = contextJpeg_ && !contextJpeg_->empty();
  return {{"available", ready}, {"title", foreground_.title}, {"app", foreground_.process},
          {"hasWindow", foreground_.hwnd != nullptr}, {"mode", screenMode_},
          {"id", screenVersion_}, {"monitor", monitorNumber_},
          {"width", region.right - region.left}, {"height", region.bottom - region.top},
          {"thumb", ready ? "https://capture.aiassistant/context-" + std::to_string(screenVersion_) + ".jpg" : ""},
          {"status", ready ? "captured" : "blocked"}, {"reason", captureError_}};
}

RECT App::ContextRegion() const {
  if (!snapshot_) return {};
  if (screenMode_ == "window" && foreground_.hwnd) {
    RECT r;
    if (IntersectRect(&r, &foreground_.rect, &snapshot_->bounds)) return r;
    return {};  // Never silently broaden a window attachment to the whole monitor.
  }
  if (screenMode_ == "window") return {};
  return snapshot_->bounds;
}

bool App::ResourceFor(const std::wstring& path, std::wstring& mime, std::vector<uint8_t>& data) {
  if (snapshot_) {
    if (auto reason = CapturePermission(); !reason.empty()) {
      BlockCapture(reason);
      return false;
    }
  }
  mime = L"image/jpeg";
  if (path == L"backdrop.jpg") {
    auto img = live_ ? live_ : snapshot_;
    if (!img) return false;
    if (backdropFor_ != captureVersion_ || backdropJpeg_.empty()) {
      // The backdrop is heavily blurred, so half resolution is plenty and much faster.
      backdropJpeg_ = EncodeRegion(*img, overlayRect_, 0, 0.5, ImageFormat::Jpeg, 0.8f);
      backdropFor_ = captureVersion_;
    }
    data = backdropJpeg_;
    return !data.empty();
  }
  if (path == L"context-" + std::to_wstring(screenVersion_) + L".jpg") {
    if (!contextJpeg_) return false;
    data = *contextJpeg_;
    return !data.empty();
  }
  return false;
}

void App::RefreshBackdrop() {
  if (!overlay_ || !overlay_->Visible() || !snapshot_ || backdropRefreshing_) return;
  if (auto reason = CapturePermission(); !reason.empty()) return BlockCapture(reason);
  backdropRefreshing_ = true;
  RECT rect = overlayRect_;
  HMONITOR mon = MonitorFromRect(&rect, MONITOR_DEFAULTTONEAREST);
  const int version = screenVersion_;
  const auto rules = settings_.Data()["screenCaptureBlocklist"];
  const bool protectedCapture = settings_.Data().value("screenCaptureProtection", true);
  const HWND source = foreground_.hwnd, bar = overlay_->Hwnd();
  const HWND settings = settingsWindow_ ? settingsWindow_->Hwnd() : nullptr;
  RunAsync([this, mon, rect, version, rules, protectedCapture, source, bar, settings] {
    auto current = QueryForegroundWindow();
    if (current.hwnd == bar || current.hwnd == settings) current = QueryWindow(source);
    auto reason = CaptureBlockReason(protectedCapture, rules, current.process, current.title);
    if (!reason.empty()) {
      PostToUi([this, version, reason] {
        backdropRefreshing_ = false;
        if (version == screenVersion_) BlockCapture(reason);
      });
      return;
    }
    auto shot = std::make_shared<ScreenImage>(CaptureMonitor(mon));
    auto jpeg = shot->Empty() ? std::vector<uint8_t>{}
                              : EncodeRegion(*shot, rect, 0, 0.5, ImageFormat::Jpeg, 0.8f);
    PostToUi([this, shot, version, jpeg = std::move(jpeg)]() mutable {
      backdropRefreshing_ = false;
      if (!overlay_ || !overlay_->Visible() || version != screenVersion_ || jpeg.empty()) return;
      if (auto reason = CapturePermission(); !reason.empty()) return BlockCapture(reason);
      live_ = shot;
      ++captureVersion_;
      backdropJpeg_ = std::move(jpeg);
      backdropFor_ = captureVersion_;
      overlay_->Post({{"type", "backdrop"},
                      {"url", "https://capture.aiassistant/backdrop.jpg?v=" + std::to_string(captureVersion_)}});
    });
  });
}

void App::PrepareContextImage() {
  if (!snapshot_) return;
  RECT region = ContextRegion();
  if (PreparedContextImage(region)) return;
  contextJpeg_ = std::make_shared<const std::vector<uint8_t>>(
      EncodeRegion(*snapshot_, region, kContextMaxSide, 1.0, ImageFormat::Jpeg, kContextQuality));
  if (contextJpeg_->empty()) {
    BlockCapture("Couldn't prepare the screenshot. Reopen the bar to try again.");
    return;
  }
  contextJpegSource_ = snapshot_.get();
  contextJpegRegion_ = region;
}

std::shared_ptr<const std::vector<uint8_t>> App::PreparedContextImage(RECT region) const {
  if (!contextJpeg_ || !snapshot_ || contextJpegSource_ != snapshot_.get() || !EqualRect(&region, &contextJpegRegion_)) {
    return nullptr;
  }
  return contextJpeg_;
}

// ---------------------------------------------------------------------------
// Messages from the web UI

void App::OnMessage(WebWindow* from, const json& msg) {
  const std::string type = msg.value("type", "");
  const bool fromOverlay = overlay_ && from == overlay_.get();

  if (type == "ready") {
    SendInit(from, fromOverlay ? "bar" : "settings");
    if (fromOverlay && pendingShow_) PostToUi([this] { ShowBar(); });
  } else if (type == "hidden") {
    if (fromOverlay) OnBarHidden(msg.value("restoreFocus", false));
  } else if (type == "dismiss") {
    if (fromOverlay) DismissBar();
  } else if (type == "setHitRegion") {
    if (fromOverlay) {
      overlay_->SetHitRegion(msg.value("x", 0.0), msg.value("y", 0.0), msg.value("width", 0.0),
                             msg.value("height", 0.0), msg.value("radius", 0.0));
    }
  } else if (type == "ask") {
    if (fromOverlay) Ask(msg);
  } else if (type == "prepare") {
    if (fromOverlay) {
      prepare_ = msg;
      Prepare();
    }
  } else if (type == "cancel") {
    if (active_ && active_->id == msg.value("id", "")) active_->cancel.Cancel();
  } else if (type == "newChat") {
    ResetConversation();
  } else if (type == "screenPreviewReady") {
    if (fromOverlay && contextJpeg_ && msg.value("screenId", -1) == screenVersion_) {
      reviewedScreenVersion_ = screenVersion_;
    }
  } else if (type == "setScreenMode") {
    if (!fromOverlay) return;
    std::string mode = msg.value("mode", "monitor");
    screenMode_ = mode == "window" ? "window" : "monitor";
    ++screenVersion_;
    reviewedScreenVersion_ = -1;
    if (auto reason = CapturePermission(); !reason.empty()) BlockCapture(reason);
    else PrepareContextImage();
    overlay_->Post({{"type", "screen"}, {"screen", ScreenPayload()}});
  } else if (type == "listModels") {
    ListModels(msg.value("provider", ""), msg.value("refresh", false));
  } else if (type == "openSettings") {
    OpenSettings();
  } else if (type == "saveSettings") {
    json patch = msg.value("patch", json::object());
    patch.erase("hotkey");  // Changed through setHotkey so it can be validated and registered
    patch.erase("setupCompleted");  // Only a successful test may complete setup.
    json before = settings_.Data();
    settings_.Apply(patch);
    settings_.Save();
    if (patch.contains("providers") && patch["providers"].is_object()) {
      for (auto& [id, _] : patch["providers"].items()) {
        InvalidateModels(id);
        connections_.erase(id);
      }
      setupValidated_ = nullptr;
      if (setupCancel_) setupCancel_->Cancel();
    }
    if (before.value("appearance", "") != settings_.Data().value("appearance", "") ||
        before.value("backdrop", "") != settings_.Data().value("backdrop", "") ||
        before.value("solidSurfaces", false) != settings_.Data().value("solidSurfaces", false)) {
      ApplyAppearance();
    }
    if (overlay_ && overlay_->Visible()) {
      if (auto reason = CapturePermission(); !reason.empty()) BlockCapture(reason);
      else if (patch.contains("screenMode")) {
        screenMode_ = settings_.Data().value("screenMode", "monitor");
        ++screenVersion_;
        reviewedScreenVersion_ = -1;
        PrepareContextImage();
        overlay_->Post({{"type", "screen"}, {"screen", ScreenPayload()}});
      }
    } else if (patch.contains("screenMode")) screenMode_ = settings_.Data().value("screenMode", "monitor");
    Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
  } else if (type == "setApiKey") {
    std::string provider = msg.value("provider", "");
    std::string key = Trim(msg.value("key", ""));
    const ProviderInfo* info = FindProvider(provider);
    if (!info || key.empty()) return;
    bool ok = SecretStore::Set(provider, key);
    Log("API key " + std::string(ok ? "saved" : "NOT saved") + " for " + provider);
    InvalidateModels(provider);
    connections_.erase(provider);
    setupValidated_ = nullptr;
    if (setupCancel_) setupCancel_->Cancel();
    Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
    if (ok) TestProvider(provider);
    else Broadcast({{"type", "testResult"}, {"provider", provider}, {"ok", false},
                    {"message", "Couldn't save the key in Windows Credential Manager. Try again."}});
  } else if (type == "removeApiKey") {
    std::string provider = msg.value("provider", "");
    SecretStore::Remove(provider);
    InvalidateModels(provider);
    connections_.erase(provider);
    setupValidated_ = nullptr;
    if (setupCancel_) setupCancel_->Cancel();
    Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
  } else if (type == "testProvider") {
    TestProvider(msg.value("provider", ""));
  } else if (type == "detectCli") {
    DetectCliAsync(msg.value("provider", ""));
  } else if (type == "discoverProviders") {
    DiscoverProviders();
  } else if (type == "testSetup") {
    if (!fromOverlay) TestSetup(msg);
  } else if (type == "cancelSetupTest") {
    if (setupCancel_) setupCancel_->Cancel();
  } else if (type == "completeSetup") {
    if (!fromOverlay) CompleteSetup(msg);
  } else if (type == "setHotkey") {
    SetHotkey(msg.value("hotkey", ""));
  } else if (type == "suspendHotkey") {
    if (hotkeyRegistered_) UnregisterHotKey(host_, kHotkeyId);
    hotkeyRegistered_ = false;
  } else if (type == "resumeHotkey") {
    if (!hotkeyRegistered_) RegisterCurrentHotkey();
  } else if (type == "setAutostart") {
    SetAutostartEnabled(msg.value("enabled", false));
    Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
  } else if (type == "copy") {
    bool ok = CopyToClipboard(msg.value("text", ""), msg.value("html", ""));
    from->Post({{"type", "copyResult"}, {"id", msg.value("id", "")}, {"ok", ok}});
  } else if (type == "openUrl") {
    std::string url = msg.value("url", "");
    if (StartsWith(url, "https://") || StartsWith(url, "http://")) {
      ShellExecuteW(nullptr, L"open", Widen(url).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }
  } else if (type == "openDataFolder") {
    ShellExecuteW(nullptr, L"open", LocalDataDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  } else if (type == "quit") {
    Quit();
  }
}

// ---------------------------------------------------------------------------
// Hotkey

json App::HotkeySuggestions() const {
  json suggestions = json::array();
  for (const char* text : {"Ctrl+Alt+Space", "Ctrl+Shift+Space", "Alt+Shift+Space", "Alt+Space"}) {
    auto spec = ParseHotkey(text);
    bool available = hotkeyRegistered_ && settings_.Hotkey() == text;
    if (!available && host_ && RegisterHotKey(host_, 2, spec->modifiers | MOD_NOREPEAT, spec->vk)) {
      available = true;
      UnregisterHotKey(host_, 2);
    }
    suggestions.push_back({{"hotkey", text}, {"available", available}});
  }
  return suggestions;
}

bool App::RegisterCurrentHotkey() {
  auto spec = ParseHotkey(settings_.Hotkey());
  if (!spec) spec = ParseHotkey("Alt+Space");
  hotkeyRegistered_ = RegisterHotKey(host_, kHotkeyId, spec->modifiers | MOD_NOREPEAT, spec->vk) != FALSE;
  hotkeyError_ = hotkeyRegistered_ ? "" : settings_.Hotkey() + " is already used by another app";
  if (!hotkeyRegistered_) Log("RegisterHotKey failed for " + settings_.Hotkey());
  return hotkeyRegistered_;
}

void App::SetHotkey(const std::string& text) {
  auto reply = [&](bool ok, const std::string& error) {
    Broadcast({{"type", "hotkeyResult"}, {"ok", ok}, {"hotkey", settings_.Hotkey()}, {"error", error}});
  };
  auto spec = ParseHotkey(text);
  if (!spec) {
    reply(false, "Use at least one modifier (Ctrl, Alt, Shift or Win) plus a key");
    return;
  }
  if (hotkeyRegistered_) UnregisterHotKey(host_, kHotkeyId);
  hotkeyRegistered_ = false;
  if (!RegisterHotKey(host_, kHotkeyId, spec->modifiers | MOD_NOREPEAT, spec->vk)) {
    RegisterCurrentHotkey();
    reply(false, FormatHotkey(*spec) + " is already used by another app");
    return;
  }
  hotkeyRegistered_ = true;
  hotkeyError_.clear();
  settings_.Apply({{"hotkey", FormatHotkey(*spec)}});
  settings_.Save();
  tray_.SetTooltip(L"AIAssistant (" + Widen(settings_.Hotkey()) + L")");
  reply(true, "");
  Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
}

// ---------------------------------------------------------------------------
// Conversation

void App::ResetConversation() {
  CancelStream();
  history_.clear();
  conversationSystem_.clear();
  historyProvider_.clear();
}

void App::CancelStream() {
  if (!active_) return;
  active_->cancel.Cancel();
  active_.reset();
  KillTimer(host_, kFlushTimer);
}

ModelInfo App::LookupModel(const std::string& provider, const std::string& model) const {
  if (auto it = modelCache_.find(provider); it != modelCache_.end()) {
    for (const auto& m : it->second.models) {
      if (m.id == model) return m;
    }
  }
  for (const auto& m : BuiltinModels(provider)) {
    if (m.id == model) return ApplyModelConfig(provider, {m}, settings_.ProviderConfig(provider)).front();
  }
  return ApplyModelConfig(provider, {InferModel(provider, model)}, settings_.ProviderConfig(provider)).front();
}

void App::Ask(const json& msg) {
  std::string id = msg.value("id", "");
  std::string text = msg.value("text", "");
  std::string providerId = msg.value("provider", "");
  std::string modelId = msg.value("model", "");
  std::string reasoning = msg.value("reasoning", "");
  bool withScreen = msg.value("screen", false);

  auto fail = [&](const std::string& error) {
    Broadcast({{"type", "done"}, {"id", id}, {"ok", false}, {"cancelled", false}, {"error", error}});
  };
  const ProviderInfo* info = FindProvider(providerId);
  if (!info) return fail("Choose a model first: open Settings to connect a provider.");
  if (!history_.empty() && historyProvider_ != providerId) {
    return fail("Start a new chat before sending this conversation to a different provider.");
  }
  ModelInfo model = LookupModel(providerId, modelId);
  if (withScreen) {
    if (auto reason = CapturePermission(); !reason.empty()) {
      BlockCapture(reason);
      return fail(reason);
    }
    if (!snapshot_ || !contextJpeg_ || contextJpeg_->empty()) {
      return fail("No screenshot is available. Turn Screen off or reopen the bar.");
    }
    if (msg.value("screenId", -1) != screenVersion_ || reviewedScreenVersion_ != screenVersion_) {
      return fail("Review the current screenshot before sending. Its preview is still loading or has changed.");
    }
    if (!model.vision) return fail("This model cannot view screenshots. Turn Screen off or choose a vision model.");
  }
  std::string key;
  if (info->needsKey || info->keyOptional) {
    auto k = SecretStore::Get(providerId);
    if (k) key = *k;
    if (key.empty() && info->needsKey && !info->keyOptional) {
      return fail("Add your " + info->name + " API key in Settings to use this model.");
    }
  }

  CancelStream();
  if (history_.empty()) {
    conversationSystem_ = FreshSystemPrompt();
  }

  ChatMessage user;
  user.role = "user";
  user.text = text;
  std::shared_ptr<const std::vector<uint8_t>> jpeg;
  if (withScreen) {
    std::string where = screenMode_ == "window" ? "the active window" : "their screen";
    std::string ctx = "<screen_context>\n";
    if (!foreground_.title.empty() || !foreground_.process.empty()) {
      ctx += "Active window: \"" + foreground_.title + "\"" +
             (foreground_.process.empty() ? "" : " (" + foreground_.process + ")") + "\n";
    }
    ctx += "A screenshot of " + where + " frozen when the bar opened is attached.\n";
    jpeg = contextJpeg_;  // Exact bytes served by the reviewed preview; no send-time recapture.
    ctx += "</screen_context>\n\n";
    user.text = ctx + text;
  }

  std::string budgetError;
  if (!ReserveApiBudget(providerId, budgetError)) return fail(budgetError);

  auto session = std::make_shared<StreamSession>();
  session->id = id;
  session->provider = providerId;
  active_ = session;
  SetTimer(host_, kFlushTimer, 16, nullptr);

  ChatRequest request;
  request.model = modelId;
  request.reasoning = reasoning;
  request.system = conversationSystem_;
  request.messages = history_;
  json config = settings_.ProviderConfig(providerId);
  bool isCli = info->kind == ProviderKind::Cli;

  RunAsync([this, session, request = std::move(request), user = std::move(user), model, jpeg,
            providerId, config, key, isCli]() mutable {
    if (jpeg) {
      if (session->cancel.IsCancelled()) return;
      if (!jpeg->empty()) user.image = ImageData{"image/jpeg", Base64Encode(*jpeg)};
      Log("Screen context attached: " + std::to_string(jpeg->size()) + " bytes" + (isCli ? " (CLI)" : ""));
    }
    request.messages.push_back(user);
    StreamSink sink;
    sink.text = [session](std::string_view s) {
      std::lock_guard lock(session->mutex);
      session->pendingText.append(s);
      session->fullText.append(s);
    };
    sink.reasoning = [session](std::string_view s) {
      std::lock_guard lock(session->mutex);
      session->pendingReasoning.append(s);
    };
    sink.status = [session](std::string_view s) {
      std::lock_guard lock(session->mutex);
      session->pendingStatus.assign(s);
    };
    sink.limit = [session](std::string_view s) {
      std::lock_guard lock(session->mutex);
      session->pendingLimit = std::string(s);
    };
    StreamResult result;
    auto provider = CreateProvider(providerId, config, key);
    if (!provider) {
      result.error = "Unknown provider";
    } else {
      result = provider->Stream(request, model, sink, session->cancel);
    }
    PostToUi([this, session, user = std::move(user), result = std::move(result)]() mutable {
      OnStreamDone(session, std::move(user), std::move(result));
    });
  });
}

std::string App::FreshSystemPrompt() const {
  return settings_.SystemPrompt() + "\n\nToday's date is " + Today() + ".";
}

// Warms up what the bar's next question will use, mirroring how Ask builds the request.
void App::Prepare() {
  if (!overlay_ || !overlay_->Visible() || active_ || !prepare_.is_object()) return;
  std::string providerId = prepare_.value("provider", "");
  const ProviderInfo* info = FindProvider(providerId);
  if (!info) return;
  ModelInfo model = LookupModel(providerId, prepare_.value("model", ""));
  // The attachment was encoded before publishing the preview, never during send/preparation.

  ChatRequest request;
  request.model = prepare_.value("model", "");
  request.reasoning = prepare_.value("reasoning", "");
  request.system = history_.empty() ? FreshSystemPrompt() : conversationSystem_;
  json config = settings_.ProviderConfig(providerId);
  bool needsKey = info->needsKey || info->keyOptional;
  RunAsync([providerId, config, needsKey, request = std::move(request), model] {
    std::string key = needsKey ? SecretStore::Get(providerId).value_or("") : "";
    if (auto provider = CreateProvider(providerId, config, key)) provider->Prepare(request, model);
  });
}

void App::FlushDeltas() {
  if (!active_) {
    KillTimer(host_, kFlushTimer);
    return;
  }
  std::string text, reasoning, status;
  std::optional<std::string> limit;
  {
    std::lock_guard lock(active_->mutex);
    text.swap(active_->pendingText);
    reasoning.swap(active_->pendingReasoning);
    status.swap(active_->pendingStatus);
    limit.swap(active_->pendingLimit);
  }
  if (text.empty() && reasoning.empty() && status.empty() && !limit) return;
  json delta = {{"type", "delta"}, {"id", active_->id}};
  if (!text.empty()) delta["text"] = text;
  if (!reasoning.empty()) delta["reasoning"] = reasoning;
  if (!status.empty()) delta["status"] = status;
  if (limit) delta["limitWarning"] = *limit;
  if (overlay_) overlay_->Post(delta);
}

void App::OnStreamDone(const std::shared_ptr<StreamSession>& session, ChatMessage userMessage,
                       StreamResult result) {
  if (session != active_) return;  // Superseded by a newer question or a new chat
  Log(session->provider + ": " +
      (result.ok ? "done" : result.cancelled ? "cancelled" : "failed: " + result.error));
  FlushDeltas();
  KillTimer(host_, kFlushTimer);
  active_.reset();

  std::string fullText;
  {
    std::lock_guard lock(session->mutex);
    fullText = session->fullText;
  }
  if (result.ok || (result.cancelled && !fullText.empty())) {
    historyProvider_ = session->provider;
    history_.push_back(std::move(userMessage));
    ChatMessage reply;
    reply.role = "assistant";
    reply.text = fullText;
    if (result.ok) {
      reply.raw = result.raw;
      reply.rawProvider = session->provider;
    }
    history_.push_back(std::move(reply));
  }
  lastActivity_ = std::chrono::steady_clock::now();
  Prepare();  // Get ready for a follow-up
  if (overlay_) {
    overlay_->Post({{"type", "done"},
                    {"id", session->id},
                    {"ok", result.ok},
                    {"cancelled", result.cancelled},
                    {"error", result.error},
                    {"stopReason", result.stopReason},
                    {"limitWarning", result.limitWarning},
                    {"usage", result.usage},
                    {"streamed", result.streamed}});
  }
}

// ---------------------------------------------------------------------------
// Providers

void App::InvalidateModels(const std::string& provider) {
  modelCache_.erase(provider);
  ++providerRevision_[provider];
}

void App::ListModels(const std::string& providerId, bool refresh) {
  const ProviderInfo* info = FindProvider(providerId);
  if (!info) return;
  if (!refresh) {
    if (auto it = modelCache_.find(providerId);
        it != modelCache_.end() && std::chrono::steady_clock::now() - it->second.fetched < kModelCacheTtl) {
      json models = json::array();
      for (const auto& m : it->second.models) models.push_back(ToJson(m));
      Broadcast({{"type", "models"}, {"provider", providerId}, {"models", models}, {"error", ""}});
      return;
    }
  }
  if (listing_[providerId]) return;
  listing_[providerId] = true;
  json config = settings_.ProviderConfig(providerId);
  auto revision = providerRevision_[providerId];
  RunAsync([this, providerId, config, revision] {
    std::string key = SecretStore::Get(providerId).value_or("");
    ModelListResult result;
    if (auto provider = CreateProvider(providerId, config, key)) {
      CancelToken cancel;
      result = provider->ListModels(cancel);
    }
    bool live = result.error.empty() && !result.models.empty();
    if (result.models.empty()) result.models = BuiltinModels(providerId);
    result.models = ApplyModelConfig(providerId, std::move(result.models), config);
    PostToUi([this, providerId, config, revision, result, live] {
      listing_[providerId] = false;
      if (providerRevision_[providerId] != revision || settings_.ProviderConfig(providerId) != config) {
        ListModels(providerId, true);
        return;
      }
      if (live) modelCache_[providerId] = {result.models, std::chrono::steady_clock::now()};
      json models = json::array();
      for (const auto& m : result.models) models.push_back(ToJson(m));
      Broadcast({{"type", "models"}, {"provider", providerId}, {"models", models}, {"error", result.error}});
    });
  });
}

void App::TestProvider(const std::string& providerId) {
  const ProviderInfo* info = FindProvider(providerId);
  if (!info) return;
  json config = settings_.ProviderConfig(providerId);
  bool isCli = info->kind == ProviderKind::Cli;
  auto revision = providerRevision_[providerId], testNumber = ++providerTests_[providerId];
  RunAsync([this, providerId, config, isCli, revision, testNumber] {
    json reply = {{"type", "testResult"}, {"provider", providerId}};
    if (isCli) {
      CliStatus st = DetectCli(providerId);
      reply["ok"] = st.installed && st.signedIn;
      reply["message"] = st.installed ? st.detail : "Not installed";
      PostToUi([this, providerId, config, st, reply, revision, testNumber] {
        if (providerTests_[providerId] != testNumber) return;
        if (providerRevision_[providerId] != revision || settings_.ProviderConfig(providerId) != config) {
          Broadcast({{"type", "testResult"}, {"provider", providerId}, {"ok", false}, {"message", "Settings changed during the check. Test again."}});
          return;
        }
        cliStatus_[providerId] = st;
        connections_[providerId] = {{"state", st.signedIn ? "ready" : "not-connected"},
          {"detail", st.detail}, {"nextStep", st.signedIn ? "Send a test question to confirm it can answer." : "Sign in, then check again."}};
        Broadcast(reply);
        Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
      });
      return;
    }
    std::string key = SecretStore::Get(providerId).value_or("");
    ModelListResult result;
    if (auto provider = CreateProvider(providerId, config, key)) {
      CancelToken cancel;
      result = provider->ListModels(cancel);
    }
    bool ok = result.error.empty() && !result.models.empty();
    reply["ok"] = ok;
    reply["message"] = ok ? "Connected · " + std::to_string(result.models.size()) + " models"
                          : result.error.empty() ? "No models available. Load a model, then test again." : result.error;
    if (ok) result.models = ApplyModelConfig(providerId, std::move(result.models), config);
    PostToUi([this, providerId, config, result, ok, reply, revision, testNumber] {
      if (providerTests_[providerId] != testNumber) return;
      if (providerRevision_[providerId] != revision || settings_.ProviderConfig(providerId) != config) {
        Broadcast({{"type", "testResult"}, {"provider", providerId}, {"ok", false}, {"message", "Settings changed during the check. Test again."}});
        return;
      }
      connections_[providerId] = {{"state", ok ? "connected" : "not-connected"},
        {"detail", reply["message"]}, {"nextStep", ok ? "Ready to ask." : "Check your key or server address, then test again."}};
      if (ok && !result.models.empty()) {
        modelCache_[providerId] = {result.models, std::chrono::steady_clock::now()};
        json models = json::array();
        for (const auto& m : result.models) models.push_back(ToJson(m));
        Broadcast({{"type", "models"}, {"provider", providerId}, {"models", models}, {"error", ""}});
      }
      Broadcast(reply);
      Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
    });
  });
}

void App::DetectCliAsync(const std::string& providerId) {
  const auto* info = FindProvider(providerId);
  if (!info || info->kind != ProviderKind::Cli || detecting_[providerId]) return;
  detecting_[providerId] = true;
  RunAsync([this, providerId] {
    CliStatus st = DetectCli(providerId);
    PostToUi([this, providerId, st] {
      cliStatus_[providerId] = st;
      detecting_[providerId] = false;
      connections_.erase(providerId);
      Broadcast({{"type", "cliStatus"},
                 {"provider", providerId},
                 {"installed", st.installed},
                 {"signedIn", st.signedIn},
                 {"path", st.path},
                 {"detail", st.detail}});
      Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
    });
  });
}

// ---------------------------------------------------------------------------

void App::DiscoverProviders() {
  if (discoveryPending_) return;
  discoveryPending_ = 6;
  discovery_["running"] = true;
  discovery_["complete"] = false;
  discovery_["suggestedProvider"] = "";
  auto finish = [this] {
    if (--discoveryPending_ == 0) {
      discovery_["running"] = false;
      discovery_["complete"] = true;
      auto ready = [this](const std::string& id) {
        auto it = connections_.find(id);
        if (it != connections_.end()) return it->second.value("state", "") == "connected";
        auto cli = cliStatus_.find(id);
        return cli != cliStatus_.end() ? cli->second.signedIn : SecretStore::Has(id);
      };
      // Keep an existing usable choice; otherwise suggest local models before cloud services.
      std::string preferred = settings_.Data()["defaultModel"].value("provider", "");
      if (!preferred.empty() && ready(preferred)) discovery_["suggestedProvider"] = preferred;
      else for (const char* id : {"ollama", "lmstudio", "claude-cli", "codex-cli", "gemini-cli", "anthropic", "openai", "gemini", "openrouter", "xai", "groq", "mistral", "deepseek"}) {
        if (ready(id)) { discovery_["suggestedProvider"] = id; break; }
      }
    }
    Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
  };
  Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
  RunAsync([this, finish] {
    bool node = !FindExecutable(L"node", {L"%ProgramFiles%\\nodejs", L"%LOCALAPPDATA%\\Programs\\nodejs"}).empty();
    bool npm = !FindExecutable(L"npm", {L"%ProgramFiles%\\nodejs", L"%APPDATA%\\npm"}).empty();
    PostToUi([this, finish, node, npm] {
      discovery_["nodeInstalled"] = node;
      discovery_["npmInstalled"] = npm;
      finish();
    });
  });
  for (const char* id : {"claude-cli", "codex-cli", "gemini-cli", "ollama", "lmstudio"}) {
    std::string providerId = id;
    json config = settings_.ProviderConfig(providerId);
    connections_.erase(providerId);
    RunAsync([this, providerId, config, finish] {
      CliStatus cli;
      ModelListResult local;
      bool isCli = FindProvider(providerId)->kind == ProviderKind::Cli;
      try {
        if (isCli) cli = DetectCli(providerId);
        else {
          auto base = config.value("baseUrl", "");
          if (base.empty()) base = FindProvider(providerId)->defaultBaseUrl;
          local = DiscoverLocalModels(providerId, base);
        }
      } catch (...) {
        cli.detail = local.error = "Couldn't check this provider. Check its setup and try again.";
      }
      PostToUi([this, providerId, config, finish, cli, local, isCli] {
        if (settings_.ProviderConfig(providerId) == config) {
          if (isCli) cliStatus_[providerId] = cli;
          else {
            bool ok = local.error.empty() && !local.models.empty();
            connections_[providerId] = {{"state", ok ? "connected" : "not-connected"},
              {"detail", ok ? std::to_string(local.models.size()) + " models available" : local.error},
              {"nextStep", ok ? "Send a test question." : local.error}};
            if (ok) {
              modelCache_[providerId] = {local.models, std::chrono::steady_clock::now()};
              json models = json::array();
              for (const auto& model : local.models) models.push_back(ToJson(model));
              Broadcast({{"type", "models"}, {"provider", providerId}, {"models", models}, {"error", ""}});
            }
          }
        }
        finish();
      });
    });
  }
}

void App::TestSetup(const json& msg) {
  std::string providerId = msg.value("provider", ""), modelId = msg.value("model", ""), id = msg.value("id", "");
  const auto* info = FindProvider(providerId);
  if (!info || modelId.empty() || id.empty()) return;
  std::string budgetError;
  if (!ReserveApiBudget(providerId, budgetError)) {
    Broadcast({{"type", "setupTestResult"}, {"id", id}, {"provider", providerId}, {"model", modelId},
               {"ok", false}, {"answer", ""}, {"error", budgetError}});
    return;
  }
  if (setupCancel_) setupCancel_->Cancel();
  setupCancel_ = std::make_shared<CancelToken>();
  auto cancel = setupCancel_;
  setupRequestId_ = id;
  setupValidated_ = nullptr;
  auto config = settings_.ProviderConfig(providerId);
  auto model = LookupModel(providerId, modelId);
  RunAsync([this, providerId, model, config, id, cancel] {
    SetupAnswer answer;
    std::mutex mutex;
    std::condition_variable cv;
    bool done = false;
    std::atomic<bool> timedOut = false;
    std::thread timeout([&] {
      std::unique_lock lock(mutex);
      if (!cv.wait_for(lock, std::chrono::seconds(45), [&] { return done; })) {
        timedOut = true;
        cancel->Cancel();
      }
    });
    try {
      auto provider = CreateProvider(providerId, config, SecretStore::Get(providerId).value_or(""));
      if (provider) answer = TestSetupAnswer(*provider, model, *cancel);
    } catch (...) {
      answer.error = "The provider couldn't complete the test. Check its setup and try again.";
    }
    { std::lock_guard lock(mutex); done = true; }
    cv.notify_one();
    timeout.join();
    if (cancel->IsCancelled()) {
      answer.ok = false;
      answer.error = timedOut ? "The test took too long. Check the provider and try again." : "Test stopped. You can try again.";
    }
    PostToUi([this, providerId, model, config, id, cancel, answer] {
      if (setupRequestId_ != id) return;
      setupCancel_.reset();
      bool ok = answer.ok && settings_.ProviderConfig(providerId) == config && !cancel->IsCancelled();
      if (ok) setupValidated_ = {{"id", id}, {"provider", providerId}, {"model", model.id}, {"config", config}};
      connections_[providerId] = {{"state", ok ? "connected" : "not-connected"},
        {"detail", ok ? "Answered the test question" : answer.error}, {"nextStep", ok ? "Ready to ask." : "Check setup and try the test again."}};
      Broadcast({{"type", "setupTestResult"}, {"id", id}, {"provider", providerId}, {"model", model.id},
        {"ok", ok}, {"answer", ok ? answer.answer : ""}, {"error", answer.error}});
      Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
    });
  });
}

void App::CompleteSetup(const json& msg) {
  std::string id = msg.value("id", "");
  if (!setupValidated_.is_object() || setupValidated_.value("id", "") != id ||
      settings_.ProviderConfig(setupValidated_.value("provider", "")) != setupValidated_["config"] || !hotkeyRegistered_) {
    Broadcast({{"type", "setupComplete"}, {"ok", false}, {"error", "Confirm a working test question and an available shortcut first."}});
    return;
  }
  std::string provider = setupValidated_.value("provider", ""), model = setupValidated_.value("model", "");
  auto before = settings_.Data();
  settings_.Apply({{"setupCompleted", true}, {"providers", {{provider, {{"enabled", true}}}}},
    {"defaultModel", {{"provider", provider}, {"model", model}, {"reasoning", ""}}}});
  if (!settings_.Save()) {
    settings_.Apply(before);
    Broadcast({{"type", "setupComplete"}, {"ok", false}, {"error", "Couldn't save setup. Check access to your settings folder and try again."}});
    return;
  }
  Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
  Broadcast({{"type", "setupComplete"}, {"ok", true}, {"error", ""}});
}

json App::BudgetPayload() const {
  std::string content;
  json ledger;
  auto path = LocalDataDir() + L"\\usage-budget.json";
  bool exists = GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES;
  if (exists) ledger = ReadFileUtf8(path, content) ? json::parse(content, nullptr, false) : json::object();
  double used = ledger.is_discarded() ? -1 : DailyAllowanceUsed(ledger, Today());
  return {{"day", Today()}, {"reservedUsd", std::max(0.0, used)}, {"error", used < 0 ? "API allowance record is unreadable." : ""}};
}

bool App::ReserveApiBudget(const std::string& providerId, std::string& error) {
  const auto* info = FindProvider(providerId);
  if (!info || info->kind == ProviderKind::Cli) return true;
  auto endpoint = settings_.ProviderConfig(providerId).value("baseUrl", "");
  if (endpoint.empty()) endpoint = info->defaultBaseUrl;
  if (info->kind == ProviderKind::Local && IsLoopbackUrl(endpoint)) return true;
  double limit = settings_.Data().value("apiDailyBudgetUsd", 0.0);
  if (limit == 0) return true;
  auto path = LocalDataDir() + L"\\usage-budget.json";
  json ledger;
  std::string content;
  if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
    ledger = ReadFileUtf8(path, content) ? json::parse(content, nullptr, false) : json::object();
    if (ledger.is_discarded()) ledger = json::object();
  }
  auto reservation = ReserveDailyAllowance(ledger, Today(), limit, settings_.Data().value("apiRequestAllowanceUsd", 0.25));
  error = reservation.error;
  if (!reservation.allowed) return false;
  if (!WriteFileAtomic(path, reservation.ledger.dump(2))) {
    error = "Couldn't save the API allowance. Check access to the data folder before sending.";
    return false;
  }
  Broadcast({{"type", "settings"}, {"payload", SettingsPayload()}});
  return true;
}

bool App::CopyToClipboard(const std::string& text, const std::string& html) {
  std::wstring w = Widen(text);
  auto allocate = [](const void* data, size_t bytes) -> HGLOBAL {
    HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!mem) return nullptr;
    void* buffer = GlobalLock(mem);
    if (!buffer) { GlobalFree(mem); return nullptr; }
    memcpy(buffer, data, bytes);
    GlobalUnlock(mem);
    return mem;
  };
  HGLOBAL plain = allocate(w.c_str(), (w.size() + 1) * sizeof(wchar_t));
  if (!plain) return false;
  std::string payload = html.empty() ? "" : HtmlClipboardPayload(html);
  UINT htmlFormat = payload.empty() ? 0 : RegisterClipboardFormatW(L"HTML Format");
  HGLOBAL rich = htmlFormat ? allocate(payload.c_str(), payload.size() + 1) : nullptr;
  if ((!html.empty() && !rich) || !OpenClipboard(host_)) {
    GlobalFree(plain); if (rich) GlobalFree(rich); return false;
  }
  bool ok = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, plain);
  if (!ok) GlobalFree(plain);
  if (rich && (!ok || !SetClipboardData(htmlFormat, rich))) { GlobalFree(rich); ok = false; }
  CloseClipboard();
  return ok;
}

}  // namespace aia
