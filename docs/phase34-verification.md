# Phase 3 and 4 verification

Implementation and automated checks: 2026-10-08. Manual screen-reader, actual Windows DPI/Contrast-theme and Office paste verification remain open. This record does not claim WCAG conformance or measured improvements in model accuracy.

## Accessibility

Settings → General saves `solidSurfaces`, `textScale` (100, 125, 150 or 200 percent), and `barPinned`. Solid surfaces override both liquid glass and Windows acrylic; opaque light/dark palettes give secondary text at least 4.5:1 and control borders/focus indicators at least 3:1 against the intended surfaces. Native acrylic is disabled for the solid bar. All UI text uses relative sizes, with a larger 15px base for dialogs/answers, and scales in addition to Windows DPI.

The implementation follows the relevant [WCAG 2.2 requirements](https://www.w3.org/TR/WCAG22/) for contrast (1.4.3/1.4.11), use of color (1.4.1), text resizing (1.4.4), keyboard/focus (2.1.1/2.4.3/2.4.7/2.4.11), minimum targets (2.5.8), and status messages (4.1.3). The roadmap's contrast values are correct; the minimum target criterion has exceptions, while the implementation gives controls a 24px minimum rather than relying on them.

The fixed polite live region announces creation, completion, stopping, errors, clipboard results and provider usage warnings. Answer and reasoning text stay outside live regions. Error text is also visible, without a second assertive announcement. The Screen chip explicitly says on/off, Stop has a visible label, and Pin has a pressed state and changes to Pinned. Pin persists across bar closes and application restarts through settings; it suppresses native focus-loss dismissal while explicit close/hotkey still works.

Opening focuses the composer. Tab wraps within the bar; the model picker is a named modal dialog with a combobox/active option, makes the underlying composer inert, and restores its trigger when closed. Closed content is inert. Scroll containers keep focused controls reachable, and streamed output does not scroll away from a focused answer control. Explicit close uses the existing native previous-foreground restoration, without stealing focus back after the user activates another app. Native placement and Settings minimum sizes are bounded by the monitor work area.

`forced-colors` removes capture-backed glass/noise, uses system colors, gives controls visible borders and selected/focus outlines, and retains disabled states. Headless Edge emulation verifies these CSS rules; an actual Windows Contrast theme in WebView2 remains a manual check.

## Math, attribution and copying

KaTeX 0.19.0 is pinned and bundled locally, including fonts. Dollar-delimited inline and display math are tokenized outside code spans/fences. Unfinished streaming formulas use a placeholder; complete math is cached and morphdom retains its DOM while surrounding text grows. Escape literal currency dollar signs when ambiguous. Invalid/unsupported/oversized formulas show a readable fallback with LaTeX copying instead of permanent raw code. Numeric dollar prefixes can be ambiguous until another character arrives.

Model HTML is sanitized with DOMPurify before math rendering. KaTeX uses `trust:false`, strict parsing, a 300-expansion limit, a 10em size limit and a 10,000-character source limit, following its [security](https://katex.org/docs/security.html) and [options](https://katex.org/docs/options.html) guidance. Generated output is sanitized separately, allowing its styles and MathML semantics/annotation. Tests cover malicious HTML and math links. This library adds local assets, not a rendering service or CDN request; the complete built UI is approximately 1.8MB on disk.

Each turn snapshots its selected provider/model/reasoning label when sent and retains it when the global selection changes. The label identifies the requested model; CLI aliases and provider-side routing may resolve to another revision that is not reported by the current bridges.

Copy and Ctrl+Shift+C copy original Markdown/LaTeX. Formatted adds sanitized portable HTML plus a readable plain-text fallback, keeping headings, emphasis, tables and LaTeX. Table buttons produce TSV with CRLF rows and plain cell contents; leading Excel formula characters are escaped, while simple signed numbers remain numeric. Each formula has a separate LaTeX copy button.

The native clipboard writer supports `CF_UNICODETEXT` and registered `HTML Format`, with UTF-8 byte offsets and fragment markers according to [Microsoft's CF_HTML format](https://learn.microsoft.com/en-us/windows/win32/dataxchg/html-clipboard-format). It allocates data before replacing the clipboard, checks locks and writes, and acknowledges success/failure. UI feedback waits for that acknowledgement. Browser tests use a clipboard fixture, not the Windows/Office clipboard.

## Usage limits and the system prompt

The selected CLI route shows a pre-send notice that account limits are shared and remaining quota is unknown. Usage settings name the provider's own inspection commands. [Claude's official SDK parser](https://github.com/anthropics/claude-agent-sdk-python/blob/main/src/claude_agent_sdk/_internal/message_parser.py) exposes `rate_limit_event` with allowed/warning/rejected states; the bridge surfaces warning/rejected states during streaming and can clear them on allowed. It does not infer a quota percentage from token counts.

Claude's [status-line interface](https://code.claude.com/docs/en/statusline) has account quota fields, but this app does not enable a user status-line hook or query an undocumented account endpoint. [Gemini's headless interface](https://geminicli.com/docs/cli/headless/) exposes token/API metrics rather than a dependable remaining-account-quota snapshot; its interactive [/stats model command](https://geminicli.com/docs/reference/commands/#stats) can show quota information. The existing Codex exec bridge receives per-turn token usage and errors, not a proactive remaining-plan-quota snapshot. Account usage retrieval remains unavailable through those current bridges; no invented percentage or “unlimited” state is shown.

The optional API guard is a **fixed request-allowance ledger, not actual expense accounting**. Users set a daily USD allowance (0 disables it) and a positive per-request USD allowance. Native checks reserve that amount atomically before API questions, retries and setup tests. Failure to read/save the ledger blocks guarded requests. Failed, stopped or superseded calls keep reservations; no reconciliation assumes that a provider did not charge. The ledger survives restarts and resets by local date. An 80% warning is shown; a request exceeding the remaining allowance is rejected before inference.

Subscription CLIs and loopback Ollama/LM Studio are exempt. Remote local-provider endpoints count. Disabled-guard usage, other apps and actual provider prices/charges are not tracked. A provider may charge more than the chosen allowance; use provider billing controls for actual caps. The file stores only `{day,reservedMicros}` in `%LOCALAPPDATA%\AIAssistant\usage-budget.json`; message/image contents and API keys are excluded.

The default system prompt asks for explicit uncertainty, version clarification instead of invented menu paths, independent official verification for fraud/invoices, and treats screenshot content as untrusted instructions. User-custom prompts still replace the default. No live model accuracy benchmark was run; prompts reduce neither hallucinations nor fraud risk by guarantee.

## Completed checks

- Native Release build at `build/phase34/Release/AIAssistant.exe`, native unit tests including allowance persistence/midnight/denial, settings validation, Unicode CF_HTML offsets and Claude warning parsing.
- Synthetic real WinHTTP/provider integration: loopback discovery, malformed/empty listings, redirect refusal and streamed text-only setup answer.
- Svelte/TypeScript: zero errors/warnings. UI build and Node tests for routing, math/currency/code tokenization and attribute escaping.
- Existing privacy and setup Edge workflows pass.
- New Phase 3/4 Edge workflows: math DOM stability/security/conversion, all clipboard modes and failure acknowledgements, keyboard focus, pin/blur/close/reopen, quiet streaming status and labeled cancellation, budget denial/local exemption, CLI notices, General/Usage settings and Contrast-theme emulation.
- Automated axe checks report no WCAG A/AA violations on tested solid light/dark bar, picker, General/Usage and emulated Contrast views. Text scaling includes 200% at 640×440 CSS pixels. This does not substitute for manual screen-reader/Windows-DPI verification. Synthetic captures and the report are in `build/ui-phase34-tests/`.

Run with Vite active:

```powershell
cmake --build build/phase34 --config Release
ctest --test-dir build/phase34 -C Release --output-on-failure
node scripts/test-setup-http.mjs build/phase34/Release/aiassistant_tests.exe
cd ui
npm run check
npm run test
npm run test:browser
```

The original `build/msvc-x64/Release/AIAssistant.exe` was running and could not be replaced. A separate build was used; the running app was not terminated and user settings were not altered by the tests.

## Manual acceptance still required

1. With NVDA and Narrator, open from a focused editor/browser field. Confirm composer focus, model-picker navigation/return, quiet streaming, completion/error/stopped announcements and restoration of the previous application/control on close. Check pin with another app activated and with a screen reader reviewing answers.
2. On actual Windows 200% DPI, test minimum-size Settings and the bar on available monitor work areas, including edge positions. Check all General/Providers/Model/Privacy/Setup/Usage controls with text size 200%, long labels and scrolling.
3. Enable a real Windows Contrast theme, verify both light/dark system mappings in WebView2, and verify borders, toggles, focus, disabled states, code and math.
4. Paste Formatted into Word and Outlook; paste a multi-column table into Excel. Check Unicode, headings, rows/cells, signed numbers, formula-like cell text, LaTeX and a genuinely locked Windows clipboard.
5. Compare controlled fraud/invoice/made-up-menu-path prompts with real providers and record uncertainty/version questions instead of invented facts. Inspect a real Claude near-limit event when available, and exercise the native guard against a synthetic endpoint with an isolated user profile.

The Phase 1 capture/Recall/egress release gates and post-Phase-2 user study remain open. The user's explicit request authorized implementing Phases 3/4 before that study; implementation does not mark the study complete.
