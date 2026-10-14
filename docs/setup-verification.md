# Phase 2: setup and installation

Implemented on 2026-10-08. The one-minute goal still needs measurement with first-time users; automated checks cannot establish it.

## First start

The Setup assistant opens until a real test answer and a working shortcut have been confirmed. Existing settings with a configured default provider migrate as already set up. Closing setup leaves it available on the next normal launch; background launches remain quiet. Setup can be opened again from Settings at any time.

Discovery checks Claude Code and Codex with their sign-in status commands. For Gemini CLI, which has no equivalent status command in this integration, it checks only for the presence of the cached Google OAuth file and never reads its tokens. This is shown as **Ready to test**, not verified authentication. An installed Gemini executable alone is no longer treated as a sign-in. A saved API key is also only ready to test.

Ollama and LM Studio discovery queries the configured loopback `/v1/models` endpoint (or the default ports 11434 and 1234). Requests have three-second timeouts, bypass the system proxy and refuse redirects. Remote configured endpoints are checked only through the explicit connection/model workflow. Empty or invalid model listings produce a next step rather than a usable suggestion. Existing usable defaults are preferred; otherwise a local model is suggested before a saved CLI sign-in or API key. Discovery does not enable providers or send a test question automatically.

The user chooses a provider/model and explicitly sends “Say hello in one short sentence.” The test uses the real provider streaming adapter, one synthetic user message and a short setup system prompt. It includes no screenshot, conversation history or user system prompt. It can use subscription allowance/API credit. Only a successful nonempty answer permits completion. Tests can be stopped and have a 45-second deadline. Changed credentials/settings invalidate a previous test. The answer is displayed in memory; settings save only the selected provider/model and completion flag.

Setup explains missing Node.js/npm with a download button and provider setup guide. Node.js is needed for the npm installation route, not for the AIAssistant installer or API/local use. Installing or signing in to a new provider remains a provider-managed step.

Windows probes suggested shortcuts without replacing the current one. Alt+Space carries a warning about the system menu and launcher apps even when registration succeeds. Alternatives include Ctrl+Alt+Space, Ctrl+Shift+Space and Alt+Shift+Space. Choosing one performs the actual registration check; completion requires a registered shortcut.

Settings → Providers has a per-provider overview with connection state, enabled state and the next step. A live model listing verifies connectivity, while the setup test additionally verifies inference. Status is a snapshot, not continuous health monitoring.

## Windows installer

The `aiassistant_installer` CMake target and `scripts/build-installer.ps1` produce `AIAssistant-0.1.0-Setup.exe` plus a SHA256 file. Inno Setup 6.3+ is a build prerequisite; see its [download and licensing information](https://jrsoftware.org/isdl.php). End users do not need Node, CMake, Visual Studio or a terminal to install AIAssistant.

The installer supports x64-compatible Windows 10 1903+ and Windows 11. It installs per user under `%LOCALAPPDATA%\Programs\AIAssistant`, includes the built UI, creates Start menu shortcuts and registers an uninstaller. Windows autostart and a desktop shortcut are optional and unchecked by default. Autostart uses the same HKCU Run value and `--background` command as Settings. Reinstalling with autostart unchecked removes its matching Run entry; uninstall removes only a matching entry for that installation.

The signed Microsoft WebView2 bootstrapper is bundled, verified at package build time, and run only if the runtime is missing. That step needs internet access. Detection and invocation follow [Microsoft’s distribution guidance](https://learn.microsoft.com/en-us/microsoft-edge/webview2/concepts/distribution). Uninstall retains user settings, Credential Manager keys and the shared runtime. Remove keys in Settings if desired. The AIAssistant setup executable is **unsigned**; signing belongs to Phase 5.

Build:

```powershell
cmake --build --preset msvc-x64-release --target aiassistant_installer
# Or package another completed Release build:
powershell -File scripts/build-installer.ps1 -BuildDir build/phase2/Release
```

## Verification record

Checks completed on 2026-10-08:

- Release native/UI build, Svelte/TypeScript diagnostics and native unit tests.
- Synthetic HTTP integration through the real Windows HTTP/provider code: model discovery, malformed/empty listings, refusing redirects, and an actual streamed text-only answer. Run `node scripts/test-setup-http.mjs build/phase2/Release/aiassistant_tests.exe`.
- Eight Edge browser workflows: local suggestion, successful test/default save, failure, cancellation, missing runtime/providers, API-key/Cloud route, all-provider status overview, and existing-install behavior. Run `npm run test:browser` in `ui/` with Vite running; the existing screenshot privacy tests are also included.
- Visual review in light/dark themes and the native settings minimum size of 720×520. Synthetic screenshots are under `build/ui-setup-tests`.
- Installer compilation and install/reinstall/uninstall smoke test under `build/installer-smoke`, using a separate test product ID. Verified bundled UI, uninstall registration, autostart on, autostart unchecked, and autostart cleanup. The smoke script restores any prior user autostart value. Run `powershell -File scripts/test-installer.ps1` (defaults to the `build/phase2` build).

No subscription usage was consumed for these checks. Live provider authentication/inference and installation on a clean Windows machine without WebView2 still need manual validation. The existing Phase 1 capture/Recall and packet-capture release gates remain open.

The roadmap’s next milestone is a timed test with **5–8 real users from at least three audience groups**. Record setup completion, time to first answer, the route chosen and points of confusion before reprioritizing later phases.
