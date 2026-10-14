<img src="assets/icon.svg" width="64" height="64" alt="">

# AIAssistant

A quick, private AI assistant for Windows. Press **Alt+Space** anywhere and a glass message bar drops in. Ask a question with your screen as context, or turn that off and just ask.

- **Review screen context before sending.** The Screen chip shows an expandable, removable preview with the monitor and capture size. The preview shows the exact image attached to the next question, alongside its provider, model, and Local/Cloud destination. Sensitive-window blocking is on by default and editable in Settings → Privacy.
- **Use the subscriptions you already have.** Claude (via Claude Code), ChatGPT (via the Codex CLI), and Google (via the Gemini CLI). AIAssistant runs the official CLI you are signed in to, with tools disabled, in an empty temporary folder. It never sees your subscription credentials.
- **Or bring an API key.** Anthropic, OpenAI, Google Gemini, OpenRouter, xAI, Groq, Mistral, DeepSeek, or any OpenAI-compatible endpoint. Keys are stored in Windows Credential Manager (`AIAssistant/<provider>`). They are never written to disk and never sent to the UI.
- **Local models.** Ollama and LM Studio.
- **Default model and reasoning level**, changeable on the fly from the bar (Ctrl+M).
- **Liquid-glass look** with spring animations. Follows the Windows light/dark setting and accent colour, and respects "Animation effects" being off.
- **Accessibility controls.** Settings → General offers opaque solid surfaces, 100–200% text size, and a persistent pin to keep the bar open on focus loss. Controls support Windows Contrast themes and keyboard focus. Answer creation, completion, cancellation and errors are announced without reading each streamed fragment.
- **Readable, reusable answers.** Inline/display LaTeX renders locally, with a LaTeX copy button. Each answer keeps its provider/model label. Copy preserves Markdown; Formatted copies HTML and plain text for Word/Outlook; Copy table for Excel uses tab-separated cells.
- **Usage safeguards.** CLI quota is shown as unknown unless a supported warning is reported. Settings → Usage offers an optional daily API allowance guard that reserves a user-chosen amount per request. It does not measure or cap provider billing.

Created in [T3 Code](https://t3.codes).

## Using it

Run **AIAssistant-0.1.0-Setup.exe** to install without a terminal. Windows autostart is optional, and the app can be removed through Installed apps. The installer includes the UI and installs Microsoft WebView2 if it is missing (internet required for that step). This build is unsigned.

On first start, the **Setup assistant** finds existing CLI sign-ins and local Ollama/LM Studio servers, suggests a provider, and sends a text-only test question when you choose to test it. It then helps choose an available shortcut. Alt+Space also serves the Windows system menu and launcher apps; Ctrl+Alt+Space is an alternative. Settings → Providers shows what is connected and the next step for each provider. You can reopen Setup assistant from Settings.

| Keys | In the bar |
|---|---|
| Alt+Space | Open / close (change in Settings → General) |
| Enter / Shift+Enter | Send / new line |
| Esc | Close the model picker, stop a running answer, or close the bar |
| Ctrl+S | Toggle screen context |
| Ctrl+M | Choose model and reasoning (Alt+←/→ changes the level) |
| Ctrl+N | New chat |
| Ctrl+Shift+C | Copy the last answer |
| Ctrl+, | Settings |

Follow-up questions continue the conversation. A conversation resets after 10 idle minutes (configurable in Settings → Privacy).

Pin keeps the bar open when another app gets focus; the shortcut, Close button and Escape still close it. The native host restores the previous application on explicit close when the bar still owns focus. The built-in prompt asks for uncertainty to be stated, avoids invented menu/payment details and treats screenshot content as untrusted. A custom system prompt replaces that default.

The API allowance guard includes retries and setup test questions, keeps reservations for stopped or failed calls, and resets at local midnight. Local loopback Ollama/LM Studio and subscription CLIs are exempt; remote local-provider endpoints count. Set a conservative per-request allowance and use the provider’s billing controls for actual spending limits. Reservations are stored in `%LOCALAPPDATA%\AIAssistant\usage-budget.json`, with only the date and total. See [Phase 3/4 verification](docs/phase34-verification.md) for implementation boundaries and outstanding manual checks.

The bar takes a frozen screenshot when it opens, including for its liquid-glass backdrop, even when Screen is off. A blocked active window prevents both context capture and backdrop refresh. Defaults cover common password managers and remote desktop clients, plus recognizable banking and private-browser window titles. Title matching is incomplete; inspect the preview when other sensitive windows are visible on the same monitor. Removing the attachment turns Screen off for the next question. Earlier screenshots remain in API conversation history until New chat; switching providers from the model picker starts a new chat.

Captures and API conversation history are held in memory. Codex and Gemini receive a temporary image file that is removed after the CLI run; an interrupted process can leave files behind. CLIs and providers may retain their own data. Windows capture exclusion is best-effort and is not a security guarantee. Capture-tool/Recall validation and packet-capture audits are release gates; see [the Phase 1 privacy verification record](docs/privacy-verification.md). Network behavior has not yet been audited in every mode, so we make no “no telemetry” claim.

### Connecting a subscription

1. Install and sign in to the CLI:
   - Claude: `npm i -g @anthropic-ai/claude-code`, then `claude` and `/login`
   - ChatGPT: `npm i -g @openai/codex`, then `codex login`
   - Gemini: `npm i -g @google/gemini-cli`, then `gemini`
2. Open Settings → Providers, press **Check again** if needed, and switch the provider on.

Use a recent Codex CLI (tested with 0.161.0). Desktop answers run with
`--ignore-user-config` and `--ignore-rules`, with coding plugins, MCP servers, hooks,
shell tools, and web search disabled. Codex keeps its own sign-in credentials and
uses AIAssistant's system prompt and selected reasoning level for each question.

### Command line

```
AIAssistant.exe               start (opens Setup assistant until setup is complete)
AIAssistant.exe --toggle      open/close the bar, e.g. from another launcher
AIAssistant.exe --settings    open Settings
AIAssistant.exe --background  start quietly in the tray (used by "Start with Windows")
```

A second launch hands off to the running instance.

### Files

- Settings: `%APPDATA%\AIAssistant\settings.json` (no secrets)
- Log: `%LOCALAPPDATA%\AIAssistant\aiassistant.log` (no message contents, no keys)
- WebView2 profile: `%LOCALAPPDATA%\AIAssistant\WebView2`

## Building

Requirements: Windows 10 1903+ or Windows 11, Visual Studio 2026 with the C++ workload and Windows SDK 10.0.22621+, CMake 3.25+, Node.js 20+, and the WebView2 Runtime (preinstalled on Windows 11).

```powershell
cmake --preset msvc-x64
cmake --build --preset msvc-x64-release
ctest --preset msvc-x64-release
.\build\msvc-x64\Release\AIAssistant.exe
```

The native build runs `npm install && npm run build` in `ui/` and copies the result next to the exe. Pass `-DAIASSISTANT_BUILD_UI=OFF` to skip that step, for example when you're only iterating on C++. Dependencies (WebView2 SDK, nlohmann/json, doctest) are fetched by CMake.

To build the Windows installer, install [Inno Setup 6.3+](https://jrsoftware.org/isdl.php), then run `cmake --build --preset msvc-x64-release --target aiassistant_installer`. The setup executable and SHA256 file are written to `build/msvc-x64/installer/`. See [setup verification](docs/setup-verification.md) for packaging details, tests and outstanding manual checks.

### Working on the UI

The UI is Svelte 5 + TypeScript in `ui/`. In a browser it runs against a mock host, with fake providers, streaming, and a fake desktop behind the glass:

```powershell
cd ui
npm install
npm run dev        # http://localhost:5747/bar.html and /settings.html
npm run check      # svelte-check + TypeScript
npm run test       # destination and math-token regression tests (Node.js 22.6+)
npm run test:browser # privacy, setup and Phase 3/4 workflows; needs dev server and Edge
npm run test:phase34 # Phase 3/4 workflows, clipboard fixtures and automated accessibility checks
```

Mock URL parameters: `?theme=light|dark`, `?accent=%23ff6a00`, `?reduced` (reduced motion), and `?hidden` (start closed; Alt+Space or a click opens it). `?blocked=password|banking|private|remote` exercises the capture block notice; `?preview-error` exercises a failed preview. `?provider=ollama&endpoint=http://localhost:11434/v1` exercises local routing (try a remote endpoint to verify the Cloud label). Asking a question that contains "error" shows the error state.

Settings mock fixtures: `?first-run` opens setup, `&no-node&no-providers` shows missing dependencies, `&setup-error` fails the test, `&slow-test` exercises stopping, and `&hotkey-conflict` starts with an unavailable shortcut.

Accessibility/answer fixtures: `?solid&text-scale=200`, `&pinned`, `&math&fast`, `&budget=0.25`, and `&limit-warning`. Browser tests use only synthetic prompts, providers and clipboard data; no subscription/API usage is consumed.

Subscription smoke tests are separate from the normal unit tests. They use only
**GPT-6 Luna** and **Haiku**, with synthetic prompts and images, and report time to
first text and completion. They check system instructions, follow-up history,
image input, and cancellation. They consume usage from the signed-in subscriptions:

```powershell
cmake --preset msvc-x64 -DAIASSISTANT_BUILD_LIVE_TESTS=ON
cmake --build --preset msvc-x64-release --target aiassistant_live_tests
.\build\msvc-x64\Release\aiassistant_live_tests.exe
# Optional: test only codex-cli or claude-cli by passing that provider ID.
```

To hot-reload the real app against the dev server:

```powershell
$env:AIASSISTANT_DEV_URL = "http://localhost:5747"
.\build\msvc-x64\Release\AIAssistant.exe
```

Other developer switches (environment variables set to `1`):

- `AIASSISTANT_DEVTOOLS`: enable WebView2 DevTools.
- `AIASSISTANT_ALLOW_CAPTURE`: don't exclude the bar from screen capture, so you can screenshot it. This also stops the live glass refresh, which would otherwise capture the bar itself.

### Layout

```
src/app         App: message loop, bar/settings lifecycle, UI protocol
src/platform    hotkey, tray, theme, autostart, screen capture, Credential Manager
src/ui          WebView2 host and windows (transparent overlay, Mica settings)
src/providers   Anthropic, OpenAI Responses, OpenAI-compatible, Gemini, CLI bridges
src/net         WinHTTP streaming + SSE parser
src/core        settings, paths, hotkey parsing, logging
ui/src/bar      the bar: glass panel, streaming markdown, model picker
ui/src/settings the settings window
tests/          doctest unit tests (SSE, hotkeys, request builders, CLI event parsing)
```
