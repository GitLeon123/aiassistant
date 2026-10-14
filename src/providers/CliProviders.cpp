// Subscription bridges: drive the official CLIs the user has installed and signed in to.
// The app never sees or stores subscription tokens; the CLIs own authentication.

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <map>
#include <mutex>

#include "core/Log.h"
#include "core/Paths.h"
#include "net/Sse.h"
#include "platform/Process.h"
#include "providers/Catalog.h"
#include "providers/Registry.h"
#include "providers/Wire.h"

namespace aia {
namespace {

std::wstring FindCli(const std::string& providerId) {
  if (providerId == "claude-cli") {
    return FindExecutable(L"claude", {L"%USERPROFILE%\\.local\\bin", L"%APPDATA%\\npm",
                                      L"%LOCALAPPDATA%\\Programs\\claude", L"%USERPROFILE%\\.claude\\local"});
  }
  if (providerId == "codex-cli") {
    return FindExecutable(L"codex", {L"%LOCALAPPDATA%\\Programs\\OpenAI\\Codex\\bin", L"%APPDATA%\\npm"});
  }
  if (providerId == "gemini-cli") return FindExecutable(L"gemini", {L"%APPDATA%\\npm"});
  return {};
}

// Searching PATH costs dozens of file probes, so remember the result while the file still exists.
std::wstring LocateCli(const std::string& providerId) {
  static std::mutex mutex;
  static std::map<std::string, std::wstring> found;
  {
    std::lock_guard lock(mutex);
    if (auto it = found.find(providerId); it != found.end()) {
      if (GetFileAttributesW(it->second.c_str()) != INVALID_FILE_ATTRIBUTES) return it->second;
      found.erase(it);
    }
  }
  std::wstring exe = FindCli(providerId);
  if (!exe.empty()) {
    std::lock_guard lock(mutex);
    found[providerId] = exe;
  }
  return exe;
}

// Model ids and levels end up on a command line; keep them to a conservative character set.
bool SafeToken(const std::string& s) {
  for (char c : s) {
    if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == ':' || c == '/' ||
          c == '@' || c == '[' || c == ']')) {
      return false;
    }
  }
  return true;
}

// Each run gets its own empty scratch directory so CLIs see no project context.
struct ScratchDir {
  std::wstring path;
  ScratchDir() {
    static std::atomic<unsigned> counter{0};
    path = TempDir() + L"\\run-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(++counter);
    CreateDirectoryW(path.c_str(), nullptr);
  }
  ~ScratchDir() {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW((path + L"\\*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
      do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) DeleteFileW((path + L"\\" + fd.cFileName).c_str());
      } while (FindNextFileW(h, &fd));
      FindClose(h);
    }
    RemoveDirectoryW(path.c_str());
  }
  std::wstring WriteImage(const ImageData& img, const wchar_t* name) const {
    std::wstring file = path + L"\\" + name + (img.mime == "image/png" ? L".png" : L".jpg");
    auto bytes = Base64Decode(img.base64);
    HANDLE h = CreateFileW(file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return {};
    DWORD written = 0;
    WriteFile(h, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
    CloseHandle(h);
    return file;
  }
};

std::string CliFailure(const ProcessResult& pr, const char* name) {
  if (!pr.error.empty()) return pr.error;
  std::string err = pr.stderrText;
  // Keep the last meaningful line(s) of stderr.
  while (!err.empty() && (err.back() == '\n' || err.back() == '\r' || err.back() == ' ')) err.pop_back();
  if (err.size() > 400) err = "…" + err.substr(err.size() - 400);
  return std::string(name) + " exited with code " + std::to_string(pr.exitCode) + (err.empty() ? "" : ": " + err);
}

// The requested reasoning level if the model offers it, else the model's default.
std::string EffectiveLevel(const ChatRequest& request, const ModelInfo& model) {
  const auto& levels = model.reasoning.levels;
  if (std::find(levels.begin(), levels.end(), request.reasoning) != levels.end()) return request.reasoning;
  return model.reasoning.defaultLevel;
}

std::vector<std::wstring> ClaudeArgs(const ChatRequest& request, const ModelInfo& model, const std::string& level) {
  std::vector<std::wstring> args = {L"-p", L"--output-format", L"stream-json", L"--verbose",
                                    L"--include-partial-messages", L"--input-format", L"stream-json",
                                    L"--no-session-persistence", L"--tools", L"", L"--strict-mcp-config",
                                    L"--setting-sources", L"", L"--disable-slash-commands",
                                    L"--system-prompt", Widen(request.system)};
  if (!request.model.empty()) args.insert(args.end(), {L"--model", Widen(request.model)});
  if (model.reasoning.mode == "effort" && !level.empty()) args.insert(args.end(), {L"--effort", Widen(level)});
  return args;
}

// Skips startup work a tool-less one-shot answer doesn't need (update checks, telemetry); measured
// to cut Claude Code's time to its first event from ~1.5 s to ~0.6 s.
const std::vector<std::pair<std::wstring, std::wstring>>& ClaudeEnv() {
  static const std::vector<std::pair<std::wstring, std::wstring>> env = {
      {L"CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC", L"1"},
      {L"DISABLE_AUTOUPDATER", L"1"},
      {L"DISABLE_TELEMETRY", L"1"},
      {L"DISABLE_ERROR_REPORTING", L"1"},
  };
  return env;
}

// A CLI process started ahead of the question with stdin still open. Claude Code reads the
// conversation as stream-json on stdin, so its startup can overlap with the user typing.
struct PreparedRun {
  std::wstring key;  // exe + args: a question can only use a process started with identical arguments
  std::chrono::steady_clock::time_point started;
  std::unique_ptr<ScratchDir> scratch;   // Declared before process so the process dies first
  std::unique_ptr<ChildProcess> process;
};

constexpr auto kPreparedMaxAge = std::chrono::minutes(10);
std::mutex g_preparedMutex;
std::unique_ptr<PreparedRun> g_prepared;

std::wstring RunKey(const std::wstring& exe, const std::vector<std::wstring>& args) {
  std::wstring key = exe;
  for (const auto& a : args) key += L"\x1f" + a;
  return key;
}

std::unique_ptr<PreparedRun> StartRun(const std::wstring& exe, std::vector<std::wstring> args,
                                      const std::vector<std::pair<std::wstring, std::wstring>>& env) {
  auto run = std::make_unique<PreparedRun>();
  run->key = RunKey(exe, args);
  run->started = std::chrono::steady_clock::now();
  run->scratch = std::make_unique<ScratchDir>();
  run->process = std::make_unique<ChildProcess>();
  ProcessOptions opts;
  opts.exe = exe;
  opts.args = std::move(args);
  opts.cwd = run->scratch->path;
  opts.env = env;
  run->process->Start(opts);
  return run;
}

// Takes the prepared process if it matches and is still usable. Anything else in the slot is discarded.
std::unique_ptr<PreparedRun> TakePrepared(const std::wstring& key) {
  std::unique_ptr<PreparedRun> run;
  {
    std::lock_guard lock(g_preparedMutex);
    run = std::move(g_prepared);
  }
  if (run && run->key == key && run->process->Running() &&
      std::chrono::steady_clock::now() - run->started < kPreparedMaxAge) {
    return run;
  }
  return nullptr;  // A stale or mismatched process is terminated here, outside the lock
}

class CliProvider : public Provider {
 public:
  explicit CliProvider(std::string id) : id_(std::move(id)) {}

  ModelListResult ListModels(CancelToken&) override {
    ModelListResult out;
    out.models = BuiltinModels(id_);
    if (LocateCli(id_).empty()) out.error = NotInstalledMessage();
    return out;
  }

  void Prepare(const ChatRequest& request, const ModelInfo& model) override {
    if (id_ != "claude-cli") return;  // Codex and Gemini take the prompt or image on the command line
    std::wstring exe = LocateCli(id_);
    if (exe.empty() || !SafeToken(request.model) || !SafeToken(request.reasoning)) return;
    auto args = ClaudeArgs(request, model, EffectiveLevel(request, model));
    std::wstring key = RunKey(exe, args);
    {
      std::lock_guard lock(g_preparedMutex);
      if (g_prepared && g_prepared->key == key && g_prepared->process->Running() &&
          std::chrono::steady_clock::now() - g_prepared->started < kPreparedMaxAge) {
        return;
      }
    }
    auto run = StartRun(exe, std::move(args), ClaudeEnv());
    if (!run->process->StartError().empty()) {
      Log("claude-cli: prepare failed: " + run->process->StartError());
      return;
    }
    Log("claude-cli: prepared model=" + request.model);
    std::lock_guard lock(g_preparedMutex);
    std::swap(g_prepared, run);  // The previous process (if any) is terminated when run goes out of scope
  }

  StreamResult Stream(const ChatRequest& request, const ModelInfo& model, const StreamSink& output,
                      CancelToken& cancel) override {
    std::wstring exe = LocateCli(id_);
    if (exe.empty()) return StreamResult{false, false, NotInstalledMessage()};
    if (!SafeToken(request.model) || !SafeToken(request.reasoning)) {
      return StreamResult{false, false, "Invalid model name"};
    }
    std::string level = EffectiveLevel(request, model);
    const auto started = std::chrono::steady_clock::now();
    bool sawText = false;
    StreamSink sink = output;
    sink.text = [&](std::string_view text) {
      if (!text.empty() && !sawText) {
        sawText = true;
        const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count();
        Log(id_ + ": first text after " + std::to_string(ms) + " ms");
      }
      if (output.text) output.text(text);
    };
    const ImageData* image = (!request.messages.empty() && request.messages.back().image)
                                 ? &*request.messages.back().image
                                 : nullptr;

    StreamResult result;
    ProcessResult pr;
    if (id_ == "claude-cli") {
      auto args = ClaudeArgs(request, model, level);
      auto run = TakePrepared(RunKey(exe, args));
      bool prepared = run != nullptr;
      if (!prepared) {
        if (sink.status) sink.status("Starting " + Name() + "…");
        run = StartRun(exe, std::move(args), ClaudeEnv());
      }
      std::string input = BuildClaudeCliInput(request);
      ClaudeCliStream parser(sink);
      LineSplitter lines([&](std::string_view l) { parser.OnLine(l); });
      Log("claude-cli: run model=" + request.model + " prepared=" + (prepared ? "yes" : "no") +
          " stdin bytes=" + std::to_string(input.size()));
      pr = run->process->Run(input, &cancel, [&](std::string_view chunk) {
        lines.Feed(chunk);
        return !parser.Done();
      });
      lines.Finish();
      result = parser.Finish();
    } else {
      if (sink.status) sink.status("Starting " + Name() + "…");
      ScratchDir scratch;
      ProcessOptions opts;
      opts.exe = exe;
      opts.cwd = scratch.path;
      if (id_ == "codex-cli") {
        // A desktop question must not start the user's coding plugins/MCP servers or inherit
        // project instructions. Keep authentication in the normal CODEX_HOME; only this run's
        // configuration is isolated. Otherwise unrelated startup work delays even "hello".
        opts.args = {L"exec", L"--json", L"--ephemeral", L"--skip-git-repo-check",
                     L"--ignore-user-config", L"--ignore-rules", L"-s", L"read-only",
                     L"-c", L"features.shell_tool=false", L"-c", L"features.unified_exec=false",
                     L"-c", L"features.plugins=false", L"-c", L"features.apps=false",
                     L"-c", L"features.multi_agent=false", L"-c", L"features.hooks=false",
                     L"-c", L"features.skip_host_skill_discovery=true",
                     L"-c", L"features.browser_use=false", L"-c", L"features.computer_use=false",
                     L"-c", L"features.image_generation=false", L"-c", L"features.tool_suggest=false",
                     L"-c", L"features.sleep_tool=false", L"-c", L"features.goals=false",
                     L"-c", L"features.skill_search=false", L"-c", L"features.view_image=false",
                     L"-c", L"tools.view_image=false", L"-c", L"web_search=\"disabled\"",
                     L"-c", L"project_doc_max_bytes=0"};
        // Use the desktop assistant's instructions as the model's instructions, rather than
        // appending them to Codex's much larger coding-agent prompt.
        const std::wstring instructions = scratch.path + L"\\instructions.md";
        if (!WriteFileAtomic(instructions, request.system)) {
          return StreamResult{false, false, "Couldn't prepare Codex instructions"};
        }
        opts.args.insert(opts.args.end(), {L"-c", L"model_instructions_file=" +
            Widen(json(Narrow(instructions)).dump())});
        if (!request.model.empty()) opts.args.insert(opts.args.end(), {L"-m", Widen(request.model)});
        if (!level.empty()) opts.args.insert(opts.args.end(), {L"-c", L"model_reasoning_effort=" + Widen(level)});
        opts.args.push_back(L"-");  // prompt from stdin
        if (image) {
          std::wstring file = scratch.WriteImage(*image, L"screenshot");
          if (!file.empty()) opts.args.insert(opts.args.end(), {L"-i", file});  // -i takes the rest; keep it last
        }
        opts.stdinData = FlattenConversation(request, false);
        CodexCliStream parser(sink);
        LineSplitter lines([&](std::string_view l) { parser.OnLine(l); });
        Log("codex-cli: run model=" + request.model + " image=" + (image ? "yes" : "no"));
        // Codex lingers for a couple of seconds after turn.completed; stop reading there.
        pr = RunProcess(opts, &cancel, [&](std::string_view chunk) {
          lines.Feed(chunk);
          return !parser.Done();
        });
        lines.Finish();
        result = parser.Finish();
      } else {  // gemini-cli
        std::wstring prompt = L"Answer the user's request provided on standard input.";
        if (image && !scratch.WriteImage(*image, L"screenshot").empty()) {
          prompt = std::wstring(L"@screenshot") + (image->mime == "image/png" ? L".png " : L".jpg ") + prompt;
        }
        opts.args = {L"-p", prompt, L"--output-format", L"stream-json"};
        if (!request.model.empty()) opts.args.insert(opts.args.end(), {L"-m", Widen(request.model)});
        opts.stdinData = FlattenConversation(request, true);
        GeminiCliStream parser(sink);
        LineSplitter lines([&](std::string_view l) { parser.OnLine(l); });
        Log("gemini-cli: run model=" + request.model);
        pr = RunProcess(opts, &cancel, [&](std::string_view chunk) {
          lines.Feed(chunk);
          return !parser.Done();
        });
        lines.Finish();
        result = parser.Finish();
      }
    }

    if (pr.cancelled) {
      result.ok = false;
      result.cancelled = true;
      result.error = "Cancelled";
    } else if (!pr.error.empty() || (pr.exitCode != 0 && !result.ok)) {
      // Prefer the CLI's own structured error; fall back to exit code + stderr.
      if (result.error.empty() || Contains(result.error, "exited without")) result.error = CliFailure(pr, Name().c_str());
      result.ok = false;
    }
    if (!result.ok && !result.cancelled) Log(id_ + " failed: " + result.error);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - started).count();
    Log(id_ + ": finished after " + std::to_string(ms) + " ms reasoning=" + level);
    return result;
  }

 private:
  std::string Name() const {
    if (id_ == "claude-cli") return "Claude Code";
    if (id_ == "codex-cli") return "Codex";
    return "Gemini CLI";
  }
  std::string NotInstalledMessage() const {
    if (id_ == "claude-cli") return "Claude Code is not installed. Install it from claude.com/claude-code and run `claude` once to sign in.";
    if (id_ == "codex-cli") return "Codex CLI is not installed. Install it (npm i -g @openai/codex) and run `codex login`.";
    return "Gemini CLI is not installed. Install it (npm i -g @google/gemini-cli) and run `gemini` once to sign in.";
  }

  std::string id_;
};

}  // namespace

std::unique_ptr<Provider> MakeCliProvider(const std::string& providerId) {
  return std::make_unique<CliProvider>(providerId);
}

void DiscardPreparedCli() {
  std::unique_ptr<PreparedRun> run;
  std::lock_guard lock(g_preparedMutex);
  std::swap(run, g_prepared);
}

CliStatus DetectCli(const std::string& providerId) {
  CliStatus status;
  std::wstring exe = LocateCli(providerId);
  if (exe.empty()) return status;
  status.installed = true;
  status.path = Narrow(exe);

  ProcessOptions opts;
  opts.exe = exe;
  opts.cwd = TempDir();
  opts.timeoutMs = 20000;
  std::string out;
  if (providerId == "claude-cli") {
    opts.args = {L"auth", L"status", L"--json"};
    ProcessResult pr = RunAndCapture(opts, out);
    try {
      json j = json::parse(out);
      status.signedIn = j.value("loggedIn", false);
      if (status.signedIn) {
        std::string email = j.value("email", "");
        std::string method = j.value("authMethod", "");
        status.detail = "Signed in" + (email.empty() ? "" : " as " + email) + (method.empty() ? "" : " (" + method + ")");
      } else {
        status.detail = "Not signed in. Run `claude` in a terminal to sign in.";
      }
    } catch (...) {
      status.signedIn = false;
      status.detail = "Couldn't confirm sign-in. Open Claude Code, sign in, then check again.";
    }
  } else if (providerId == "codex-cli") {
    opts.args = {L"login", L"status"};
    ProcessResult pr = RunAndCapture(opts, out);
    std::string text = out + pr.stderrText;
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
    status.signedIn = pr.exitCode == 0 && Contains(ToLower(text), "logged in");
    status.detail = status.signedIn ? text : "Not signed in. Run `codex login` in a terminal.";
  } else {
    // Gemini has no non-interactive status command. Check for a cached OAuth file,
    // without reading any token. Only the setup test can confirm that it still works.
    wchar_t path[32768];
    DWORD length = ExpandEnvironmentStringsW(L"%USERPROFILE%\\.gemini\\oauth_creds.json", path, ARRAYSIZE(path));
    DWORD attrs = length && length <= ARRAYSIZE(path) ? GetFileAttributesW(path) : INVALID_FILE_ATTRIBUTES;
    status.signedIn = attrs != INVALID_FILE_ATTRIBUTES && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
    status.detail = status.signedIn ? "Saved Google sign-in found. Send a test question to confirm it works."
                                    : "Open Gemini CLI and sign in with Google, then check again.";
  }
  return status;
}

}  // namespace aia
