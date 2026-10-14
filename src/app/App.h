#pragma once

#include <windows.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/Settings.h"
#include "platform/ScreenCapture.h"
#include "platform/Theme.h"
#include "platform/TrayIcon.h"
#include "providers/Registry.h"
#include "ui/WebViewHost.h"
#include "ui/WebWindow.h"

namespace aia {

struct LaunchOptions {
  bool background = false;  // Started by Windows (autostart): stay quiet
  bool toggle = false;      // Open the bar right away
  bool settings = false;    // Open Settings right away
};

class App {
 public:
  static constexpr const wchar_t* kHostClass = L"AIAssistant.Host";
  static constexpr ULONG_PTR kCopyDataMagic = 0xA1A55157;

  int Run(const LaunchOptions& options);

 private:
  struct StreamSession {
    std::string id;
    std::string provider;
    CancelToken cancel;
    std::mutex mutex;
    std::string pendingText, pendingReasoning, pendingStatus;
    std::optional<std::string> pendingLimit;
    std::string fullText;
  };
  struct ModelCacheEntry {
    std::vector<ModelInfo> models;
    std::chrono::steady_clock::time_point fetched;
  };

  static LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
  LRESULT HandleHost(UINT msg, WPARAM wp, LPARAM lp);

  // Threading: workers hand results back to the UI thread through PostToUi.
  void PostToUi(std::function<void()> fn);
  void RunAsync(std::function<void()> fn);

  // Windows
  void CreateOverlay();
  void OpenSettings();
  void ShowBar();
  void DismissBar();
  void ToggleBar();
  void OnBarHidden(bool restoreFocus);
  void OnMessage(WebWindow* from, const json& msg);
  void Broadcast(const json& msg);
  void SendInit(WebWindow* to, const std::string& page);
  json SettingsPayload() const;
  json ProvidersJson() const;
  bool EffectiveDark() const;
  void ApplyAppearance();

  // Hotkey
  bool RegisterCurrentHotkey();
  void SetHotkey(const std::string& text);

  // Screen capture
  bool ResourceFor(const std::wstring& path, std::wstring& mime, std::vector<uint8_t>& data);
  RECT ContextRegion() const;
  void RefreshBackdrop();
  void PrepareContextImage();
  std::shared_ptr<const std::vector<uint8_t>> PreparedContextImage(RECT region) const;
  std::string CapturePermission() const;
  void ClearCapture();
  void BlockCapture(const std::string& reason);
  json ScreenPayload() const;

  // Conversation / streaming
  void Ask(const json& msg);
  void Prepare();
  std::string FreshSystemPrompt() const;
  void CancelStream();
  void ResetConversation();
  void FlushDeltas();
  void OnStreamDone(const std::shared_ptr<StreamSession>& session, ChatMessage userMessage, StreamResult result);
  ModelInfo LookupModel(const std::string& provider, const std::string& model) const;

  // Providers
  void ListModels(const std::string& provider, bool refresh);
  void TestProvider(const std::string& provider);
  void DetectCliAsync(const std::string& provider);
  void DiscoverProviders();
  void TestSetup(const json& msg);
  void CompleteSetup(const json& msg);
  json HotkeySuggestions() const;
  void InvalidateModels(const std::string& provider);

  bool CopyToClipboard(const std::string& text, const std::string& html);
  bool ReserveApiBudget(const std::string& provider, std::string& error);
  json BudgetPayload() const;
  void Quit();

  LaunchOptions launch_;
  HWND host_ = nullptr;
  UINT taskbarCreatedMsg_ = 0;
  TrayIcon tray_;
  Settings settings_;
  ThemeInfo theme_;
  WebViewHost webHost_;
  bool webReady_ = false;
  std::unique_ptr<WebWindow> overlay_;
  std::unique_ptr<WebWindow> settingsWindow_;
  bool pendingShow_ = false;
  bool pendingSettings_ = false;
  bool hotkeyRegistered_ = false;
  std::string hotkeyError_;
  bool quitting_ = false;
  std::atomic<int> workers_{0};

  // Bar state
  HWND previousForeground_ = nullptr;
  ForegroundInfo foreground_;
  RECT overlayRect_{};
  std::shared_ptr<const ScreenImage> snapshot_;  // Frozen at hotkey time: the screen context
  std::shared_ptr<const ScreenImage> live_;      // Latest backdrop refresh
  std::string screenMode_ = "monitor";
  int captureVersion_ = 0;
  int screenVersion_ = 0;  // Only changes when the attachment changes, never with the live glass
  int reviewedScreenVersion_ = -1;
  int monitorNumber_ = 0;
  std::string captureError_;
  std::vector<uint8_t> backdropJpeg_;
  int backdropFor_ = -1;
  bool backdropRefreshing_ = false;
  // The screenshot for the model, encoded ahead of the question (see PrepareContextImage).
  std::shared_ptr<const std::vector<uint8_t>> contextJpeg_;
  const ScreenImage* contextJpegSource_ = nullptr;
  RECT contextJpegRegion_{};
  bool allowCapture_ = false;

  // Conversation
  std::vector<ChatMessage> history_;
  std::string historyProvider_;
  std::string conversationSystem_;
  std::chrono::steady_clock::time_point lastActivity_{};
  std::shared_ptr<StreamSession> active_;
  json prepare_;  // Last "prepare" from the bar: the provider/model/level the next question will use

  // Providers
  std::map<std::string, ModelCacheEntry> modelCache_;
  std::map<std::string, CliStatus> cliStatus_;
  std::map<std::string, bool> listing_;
  std::map<std::string, json> connections_;
  std::map<std::string, bool> detecting_;
  std::map<std::string, unsigned> providerRevision_, providerTests_;
  json discovery_ = {{"running", false}, {"complete", false}, {"nodeInstalled", false},
                     {"npmInstalled", false}, {"suggestedProvider", ""}};
  int discoveryPending_ = 0;
  std::shared_ptr<CancelToken> setupCancel_;
  std::string setupRequestId_;
  json setupValidated_;
};

}  // namespace aia
