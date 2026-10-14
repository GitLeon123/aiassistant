# Phase 1 privacy verification

Record date: 2026-10-08. Implementation checks and release audits are tracked separately. A source review, mock browser, or a successful Windows API call cannot establish that every capture tool honors exclusion or that a CLI emits no telemetry.

## Screenshot behavior

- The active process and window title are checked **before** the initial GDI capture, before live backdrop captures, while serving captured images, and before attaching an image to a question. Both the original source window and any newly active external window are checked. Unknown processes are blocked while protection is enabled.
- Defaults cover common password managers and remote desktop clients, banking title fragments, and English/German private-browser markers. Settings → Privacy supports adding/removing rules, restoring defaults, and disabling protection. Process patterns support `*` and `?`; title fragments are literal; comparisons ignore case. When a rule has both fields, both must match.
- Rules apply to the active/source window. Other sensitive windows visible in a whole-monitor screenshot are not automatically detected. Browser titles are not a reliable private-mode signal in every browser/locale. The preview remains necessary.
- The image is frozen at bar opening. Switching capture area crops that same image; it does not recapture. A window outside the captured monitor fails rather than falling back to the whole monitor. Images are JPEG, scaled to a maximum side of 1568 px at quality 0.85 **before** previewing. The displayed monitor dimensions describe the original capture area.
- The thumbnail and expanded preview load the same encoded bytes used for upload. The host rejects missing/stale previews, failed captures, and image requests to a model without vision. Send does not take a fresh screenshot. Live glass refreshes do not change the attachment ID or bytes. Closing the bar clears preview/capture buffers and invalidates pending refreshes.
- Removing the preview deselects Screen for the next question. It does not erase screenshots already sent. API follow-ups replay earlier images; the UI shows a reminder to start a new chat to clear them. CLI bridges currently send only the latest attachment, with text history flattened. The model picker starts a new conversation on a provider change; the host rejects a provider change with uncleared history.
- Local means the actual configured HTTP endpoint is loopback (`localhost`, IPv4 127/8, or IPv6 `::1`). A remote Ollama/LM Studio server is labeled Cloud. A custom loopback endpoint is labeled Local. This describes the question's destination, not the absence of all other network traffic.
- Native capture buffers and API history are in memory. Codex/Gemini image input uses a scratch file under `%TEMP%\AIAssistant\run-*`, removed after the run. Crashes/forced termination can leave it behind. CLI/provider persistence is independent of the app's conversation timeout.

## Capture exclusion: Phase 1.5

The overlay requests `WDA_EXCLUDEFROMCAPTURE` unless the developer-only `AIASSISTANT_ALLOW_CAPTURE=1` override is set. Failure is logged. Resource responses use `Cache-Control: no-store`.

Microsoft documents this as protection through a specific set of public capture APIs, **not a security guarantee**. `WDA_EXCLUDEFROMCAPTURE` requires Windows 10 version 2004 or later; on older versions it behaves as `WDA_MONITOR` (blank window content). Recall documents display affinity as an exclusion mechanism, but a Recall test on an eligible machine is still required. Sources: [SetWindowDisplayAffinity](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity), [Recall integration guidance](https://learn.microsoft.com/en-us/windows/apps/develop/windows-integration/recall/).

Use only synthetic content such as `AIAssistant capture check 2026-10-08`. Unset `AIASSISTANT_ALLOW_CAPTURE`. Open the bar with the marker in its input and a mock/synthetic answer. Check the input, answer, and expanded screenshot preview in every tool below; include a background window to distinguish exclusion from a black/failed capture. Record OS/tool versions and pass/fail evidence. Repeat with the override enabled as a positive control: the tool must actually see the marker then. Never publish real desktop screenshots as evidence.

| Tool / path | Required test | Result in this session |
|---|---|---|
| `GetWindowDisplayAffinity` diagnostic | Confirm the overlay requests `0x11`; this alone is not a capture test | After the requested restart on 2026-10-08, the rebuilt executable in normal mode returned `0x11` (exclusion requested). The earlier running executable returned `0x0`. Actual capture-tool tests remain pending. |
| App's GDI context and live backdrop | Reopen on synthetic desktop; expanded preview and glass must not contain the bar | Pending native runtime test of the rebuilt executable |
| Snipping Tool / Win+Shift+S | Full monitor and rectangular screenshot with bar visible | Pending manual test |
| Print Screen | Full desktop screenshot | Pending manual test |
| OBS Display Capture | Recording with bar open/streaming and preview expanded; record capture method | Pending manual test |
| Teams / Zoom screen sharing | Entire display and app-window sharing; record tool versions | Pending manual test |
| Windows Recall | Eligible Copilot+ PC, snapshots enabled, synthetic marker present for a snapshot interval; inspect timeline and search | Pending eligible-machine manual test |

Run `powershell -File scripts/check-capture-affinity.ps1` with the app running to read the requested overlay affinity without saving any screen content. No results above are marked passed merely because Microsoft documents the API.

## Network/telemetry audit: Phase 1.6

**Packet-capture result: not measured.** On 2026-10-08, `pktmon status` returned `Failed to communicate with the PktMon driver: Access is denied.` This session could not collect a packet trace. No “no telemetry” claim is approved. The table below is an inventory inferred from source, not a list of observed packets. Unknown/CLI/WebView2 traffic must be measured before release.

| Mode | Expected request destination from app source | Other traffic to investigate |
|---|---|---|
| Anthropic API key | `api.anthropic.com` | DNS/proxy/TLS infrastructure; WebView2 runtime |
| OpenAI API key | `api.openai.com` | Same |
| Gemini API key | `generativelanguage.googleapis.com` | Same |
| OpenRouter API key | `openrouter.ai` (routes to selected model's provider) | Downstream provider routing is outside this trace |
| xAI API key | `api.x.ai` | DNS/proxy/TLS infrastructure; WebView2 runtime |
| Groq API key | `api.groq.com` | Same |
| Mistral API key | `api.mistral.ai` | Same |
| DeepSeek API key | `api.deepseek.com` | Same |
| Custom endpoint | Configured HTTP(S) host | Unknown until configured and measured |
| Ollama | Default `localhost:11434`, or configured host | Model download/server-originated traffic; remote endpoints |
| LM Studio | Default `localhost:1234`, or configured host | Model download/server-originated traffic; remote endpoints |
| Claude Code subscription | CLI-managed; provider inference/auth hosts must be measured | App sets `CLAUDE_CODE_DISABLE_NONESSENTIAL_TRAFFIC`, `DISABLE_AUTOUPDATER`, `DISABLE_TELEMETRY`, `DISABLE_ERROR_REPORTING`; flags are not packet evidence |
| Codex subscription | CLI-managed; provider inference/auth hosts must be measured | User configuration/tools isolated for the run; CLI/runtime behavior still unverified |
| Gemini CLI subscription | CLI-managed; provider inference/auth hosts must be measured | CLI/runtime behavior still unverified |
| App idle/settings/bar opening | Model listings for enabled providers; selected API origin may receive a `HEAD` preconnect while typing; Claude process can be prepared | CLI detection, DNS/proxy, embedded WebView2/runtime traffic |
| First-run/settings discovery (Phase 2) | CLI sign-in status commands; loopback Ollama/LM Studio `/models` queries (no proxy or redirects) | CLI authentication/status behavior still requires packet measurement; remote configured endpoints require an explicit connection/model workflow |
| Setup test question (Phase 2) | The explicitly selected provider/model, through the normal API/CLI adapter | Synthetic text only; no screenshot/history. Uses plan allowance/API credit and inherits the provider's network behavior |
| Installer when WebView2 is missing (Phase 2) | Bundled Microsoft WebView2 bootstrapper downloads the runtime from Microsoft | Installer/runtime download/update traffic; actual peers still need measurement |

`https://app.aiassistant` is a WebView2 local-folder mapping. `https://capture.aiassistant` is intercepted and answered from memory, including 404s; these are not intentionally remote endpoints. Opening external help/key links uses the user's browser. The dev UI additionally contacts the configured dev server and Vite WebSocket; exclude developer mode when testing a release binary.

### Reproducible packet procedure (elevated PowerShell)

1. Record the binary hash, Windows/WebView2 versions, provider/CLI versions, configured endpoint, proxy configuration, and selected model. Use an isolated test account/desktop and only synthetic prompts/images. Packet traces can contain unrelated machine traffic; keep raw evidence private.
2. Disable unrelated apps and enable just one provider at a time in a test settings profile. Pre-download local models. Run a packaged UI without `AIASSISTANT_DEV_URL`; record any capture override. Record existing PktMon filters with `pktmon filter list` and ensure they do not exclude DNS, loopback, or provider traffic. Do not disturb an existing capture session.
3. In an Administrator shell, run these commands with a separate directory for each provider/mode (including idle baseline):

   ```powershell
   New-Item -ItemType Directory -Path .\build\privacy-audit\local-ollama -Force
   pktmon start --capture --pkt-size 0 --file-name .\build\privacy-audit\local-ollama\packets.etl
   # Cold start; 60 seconds idle; open settings/bar; type; synthetic text ask;
   # synthetic screenshot ask; follow-up; cancel; close; wait 60 seconds.
   # Record AIAssistant, msedgewebview2, node and CLI child PIDs and their TCP peers:
   Get-NetTCPConnection | Select-Object OwningProcess,LocalAddress,LocalPort,RemoteAddress,RemotePort,State |
     Export-Csv .\build\privacy-audit\local-ollama\connections.csv -NoTypeInformation
   pktmon stop
   pktmon etl2pcap .\build\privacy-audit\local-ollama\packets.etl --out .\build\privacy-audit\local-ollama\packets.pcapng
   ```

4. Repeat for each enabled API provider, each subscription CLI, both local servers, and any custom endpoint. Include loopback traffic and inspect the server's process traffic separately. Repeat without internet for the local-only profile. Include CLI cold/warm startup, authentication refresh, and failures, not just successful inference.
5. Inspect DNS names, TCP/UDP destinations, TLS SNI where available, proxy traffic, and process ownership. OS certificate/proxy traffic, provider traffic, runtime traffic, and telemetry must be attributed separately. An encrypted or unknown connection is **unclassified**, not proof of no telemetry.
6. Add an **observed** egress table here with mode, process/version, action, domain/IP/port, purpose/evidence, trace filename/hash, and any uncertainty. Only then evaluate privacy claims for exactly those tested versions/configurations. Repeat after CLI/WebView2/provider updates.

No raw packet captures or personal settings belong in the repository.

## Automated and UI checks

`ctest --test-dir build/phase1 -C Release --output-on-failure` covers default categories, Unicode/case matching, process glob/title intersection, invalid-settings fail-safe behavior, and opt-out/empty lists. `npm run test` covers loopback vs remote destinations, fixed endpoints, invalid URLs, and secret-free host labels. `npm run check` and the production build check the host/UI protocol types. With the Vite dev server running, `npm run test:browser` runs 12 synthetic mock-host workflows in headless Edge, including preview removal/expansion, screenshot send gating, window crops, failed previews, all four blocklist categories, local/remote endpoint labels, text-only models, and rule editing. Screenshots and results go to `build/ui-browser-tests` (override with `AIASSISTANT_UI_TEST_ARTIFACTS`; `AIASSISTANT_UI_TEST_URL` overrides the dev URL). These checks passed on 2026-10-08 with no uncaught browser errors; they do not certify native capture tools or telemetry.
