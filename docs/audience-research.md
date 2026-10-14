# AIAssistant: Target Audience Research

Date: 2026-10-08

## How this was produced

Seven Haiku subagents each researched the app from a different role. They used web search and edited no files. This document compiles their reports. Section 1 (cross-cutting themes) is a synthesis of those reports and has no research of its own. Sections 2 to 8 follow each report closely, with light condensing and some wording changes.

| # | Role | Effort |
|---|---|---|
| 1 | Software developer / power user | xhigh |
| 2 | Student / academic researcher | high |
| 3 | Non-technical office / knowledge worker | high |
| 4 | Accessibility-focused user (low vision, screen reader, keyboard/voice, ESL) | high |
| 5 | Privacy-conscious user and enterprise IT/security admin | xhigh |
| 6 | Creative / gamer / hobbyist | high |
| 7 | Tech-support "family IT person" and everyday non-technical user | high |

### Evidence quality caveats (read first)

- Most agents read search-result snippets, not full pages. Only parts of the developer and privacy reports were verified from fetched primary pages.
- Reddit and Hacker News evidence was thin or absent in nearly every report. Community sentiment is largely inferred from blogs, vendor forums, GitHub issues, and secondary coverage.
- Many survey figures come from vendor-commissioned or secondary sources. Check primaries before quoting any number.
- Each report marks which of its claims are inferred. Those marks are kept below. Rankings and feature ideas are agent judgment, not measured demand.

---

## 1. Cross-cutting themes

These ideas recur across roles. The count is how many of the seven reports raised it.

| Theme | Raised by | What it means for the app |
|---|---|---|
| **Alt+Space is a conflicted default.** It opens the Windows window system menu and is the default for PowerToys Run and Flow Launcher. Windowed games also use Alt combos. | Developer, office, creative, (accessibility indirectly) | Detect the conflict at first run, offer alternatives, and warn visibly when registration fails. |
| **Onboarding via npm/CLI/API key is the biggest adoption risk for non-developers.** | Office, support, student, privacy (consumer accounts at work) | Auto-detect existing sign-ins and local models, add a guided setup with a test question, and offer a normal installer. Evidence is mostly inference and vendor blogs, with no rigorous abandonment data. |
| **Show exactly what a screenshot contains and where it goes.** | All seven | Preview or thumbnail before send, a clear "capturing" state, and a "Local" vs "Cloud" indicator on the destination. |
| **Redaction and a capture blocklist for sensitive content** (cards, account numbers, password managers, banking). | Support, privacy, office, student, creative | Pre-send pattern masking with a preview, plus a process/window-title blocklist (similar to Recall's deny list). |
| **Trust and answer correctness.** Wrong answers are a deal-breaker for scam, bill, and menu-path questions. | Developer, support, creative, student | Show the model that answered, and prefer "I'm not sure" over guessing in high-stakes presets. |
| **Output must paste cleanly.** | Developer, office, student | Copy buttons for code and answers, rich-text copy for Outlook/Word, tab-separated tables for Excel, and LaTeX copy with correct math rendering. |
| **Accessibility of a glass overlay.** Contrast, forced colors, live-region behavior while streaming, focus management. | Accessibility, support | Solid-surface fallback, a forced-colors stylesheet, a polite status live region (not token-by-token), and focus restore on dismiss. |
| **Do not look like cheating tooling or an anti-cheat risk.** | Student, creative | Avoid "invisible"/"undetectable" marketing. Detect fullscreen games and disable capture there. |
| **Cost visibility.** Subscription limits and API spend are opaque. | Developer, student | Show the active model, estimated usage, and an optional spend cap. |
| **Enterprise control and trusted distribution.** | Privacy, office | Signed installer, silent install, an ADMX/policy file, and an audit log without content. |

---

## 2. Software developer / power user

### Persona
A Windows developer or power user who already pays for one to three AI subscriptions, sometimes runs Ollama, and uses a keyboard-first launcher (PowerToys Run, Flow Launcher, Raycast on Mac). They are cost- and limit-aware and wary of screenshots and cloud capture. Typical moment: an error, log line, regex, PowerShell flag, or CLI syntax question. They want an answer in seconds without losing window focus.

### Top needs (agent's ranking)
1. Answer without context switching (hotkey, short answer, Esc to dismiss).
2. Give the model the right context cheaply (screen, active window, clipboard, selected text), with control over what is sent.
3. Trust the answer. The Stack Overflow 2025 survey found 46% distrust AI accuracy and 66% cite "almost right" answers.
4. Predictable cost and usage.
5. Privacy control (local models, clarity on what is stored or sent).
6. Clean output for pasting.

### Pain points with existing tools
- **Windows Copilot:** forced installs. Microsoft admitted the Copilot key "broke certain workflows" and a remap is coming. Recall drew a privacy backlash and became opt-in.
- **Claude usage limits:** reports of the desktop app showing "limit reached" while web/CLI still worked. Users asked for a usage dashboard.
- **Claude Quick Entry** is macOS-only (verified from Anthropic's help page), so Windows users have no equivalent.
- **Alt+Space collisions** with PowerToys Run, Flow Launcher, and the Windows system menu.
- **Raycast AI:** a user reported thinner answers than ChatGPT, plus CPU spikes and a steep learning curve in reviews.
- **Local frontends:** Ollama is CLI-first, and "is LM Studio really offline?" recurs. Msty's speed depends on hardware.
- **TypingMind:** keys are not encrypted unless you enable it.
- **ChatGPT Windows app:** reported Markdown/LaTeX rendering bugs.

### Must-haves
- Conflict-aware hotkey that detects Alt+Space collisions at first run and offers alternatives (for example Ctrl+Alt+Space).
- Enter sends, Shift+Enter newline, Esc cancels generation and then dismisses, Ctrl+M switches model (already built).
- Copy buttons on every code block, plus a global "copy last answer" hotkey.
- Thumbnail preview of the Screen capture before send, with a "text sent" indicator.
- Clipboard image paste and region capture (Win+Shift+S style).
- Visible model and usage: show which provider/model answered, and for CLI subscriptions, remaining usage or a warning before limits.
- Stop button on streaming, and a clear marker after the idle reset.

### Nice-to-haves
- "Ask about selection" hotkey for selected text.
- Slash presets: `/explain`, `/regex`, `/ps` (PowerShell one-liner, copy-only, never auto-run).
- Side-by-side comparison of two models.
- Explicit Markdown export of a conversation.

### Deal-breakers
- Background CPU/RAM use, or hotkey launch lag.
- A hotkey that silently fails when another app owns it.
- Screenshots or chats written to disk, logs containing images, or unexplained network calls.
- Plaintext keys, or CLI-subscription auth that breaks silently.
- Confident wrong answers with no visible model or way to switch.
- Multi-monitor or high-DPI capture bugs.

### Feature suggestions beyond what's built
- Subscription meter estimating CLI usage per day, with warnings before a limit.
- Per-app presets (coding model when VS Code is focused, a cheap model elsewhere).
- "Send to terminal": put the answer's command on the clipboard with a warning.
- Local-only badge plus a "network calls made" panel.
- Hotkey-to-CLI: send the question to Claude Code/Codex with the current folder as context.

### Confidence
Verified from fetched primary pages: the Stack Overflow 2025 press release and Anthropic's Quick Entry help page. Everything else is from search snippets. The context-switching percentages (88%, 44%) come from a secondary DevOps.com summary. The "19% slower" AI study was cited without a name, so it could not be checked. The Raycast "AI behind Pro" claim comes from one unverified blog. The persona, ranking, and feature suggestions are inferred.

### Sources
- Stack Overflow 2025 survey: https://stackoverflow.co/company/press/archive/stack-overflow-2025-developer-survey/
- JetBrains on AI tool switching: https://blog.jetbrains.com/ai/2026/02/ai-tool-switching-is-stealth-friction-beat-it-at-the-access-layer/
- Neowin, Copilot key: https://www.neowin.net/news/microsoft-admits-windows-11-copilot-key-was-a-bad-idea-right-ctrl-remap-option-coming-soon/
- Tom's Guide, Recall: https://www.tomsguide.com/computing/windows-operating-systems/windows-recall-how-it-works-how-to-turn-it-off-and-why-you-should
- Claude desktop limit bug: https://claudeissues.com/issue/54941-bug-desktop-app-blocks-usage-at-91-while-web-continues-to-work-same-account-same
- The Register, Claude limits: https://www.theregister.com/2026/01/05/claude_devs_usage_limits/
- Claude Quick Entry (macOS): https://support.claude.com/en/articles/12626668-use-quick-entry-with-claude-desktop-on-mac
- Alt+Space conflicts: https://www.thewindowsclub.com/powertoys-not-working-in-windows ; https://learn.microsoft.com/en-us/windows/powertoys/run
- Raycast reviews (G2): https://www.g2.com/products/raycast/reviews_and_filters
- LM Studio (AlternativeTo): https://www.alternativeto.net/software/lm-studio/
- Msty review: https://www.promptquorum.com/power-local-llm/msty-review
- TypingMind feedback: https://feedback.typingmind.com/p/possibility-to-have-seat-users-add-their-own-api-key
- ChatGPT app formatting: https://community.openai.com/t/chatgpt-app-formatting-problem/1397318
- Cotypist: https://daringfireball.net/linked/2026/06/18/cotypist ; https://www.macsparky.com/blog/2025/03/cotypist-ai-autocompletion-everywhere-on-your-mac/
- Overlay competitors: https://forum.cursor.com/t/zupflash-an-ai-overlay-agent-for-windows-hotkey-alt-space/153491 ; https://forum.cursor.com/t/quickprompt-a-floating-desktop-ai-assistant-for-windows/149122
- Flow Launcher plugins: https://www.flowlauncher.com/plugins

---

## 3. Student / academic researcher

### Persona
Undergrad or grad student, often STEM or paper-heavy, on a Windows laptop between classes. Tight budget: most would pay $0 or under about $10/month. Tasks: concept explanations, problem sets with steps, reading PDFs and lecture slides, finding sources. Worries: being accused of cheating, fake citations, and privacy on school-managed machines.

### Top needs (agent's ranking)
1. Explain a concept fast while reading slides or a paper.
2. Work through a homework problem with visible steps and properly rendered math.
3. Make sense of a screenshot (slide, PDF page, equation, chart, error).
4. Find and check sources. Students want a starting point they can verify, not a bibliography they can't trust.
5. Writing help that fits course integrity rules.
6. Access at no or low cost.

### Pain points with existing tools
- **Cost and churn:** ChatGPT Plus is $20/month and the free student promo has ended. Google's free-year Gemini student offer runs only through December 31, 2026. Perplexity's student offer is reported inconsistently.
- **Math rendering:** forum reports of raw LaTeX in ChatGPT and intermittent rendering failures in the Mac app.
- **Citations:** Perplexity's numbered citations help, but reviews say a cited source does not always support the claim next to it.
- **Windows AI bloat:** complaints about performance and bugs. Click to Do is limited to Copilot+ PCs, so most students can't use it, which leaves room for a screen-aware tool on ordinary hardware.
- **Friction:** switching tabs, re-uploading screenshots, and losing context.

### Must-haves
- Correct, flicker-free rendering of inline `$...$` and display `$$...$$` math while streaming, plus a copy-as-LaTeX button.
- Screenshot capture that stays readable for small slide text, with region select in addition to monitor/window capture.
- A usable free path with no setup beyond install (a cheap default model, or clear onboarding for Ollama or Gemini).
- Visible cost or model indication.
- Fast time to first token.

### Nice-to-haves
- "Study mode" that asks guiding questions before giving the full solution.
- Export to Markdown, LaTeX, or clipboard HTML for Word.
- Flag when an answer contains a citation, DOI, or number the model cannot verify.
- Optional, opt-in chat history.

### Deal-breakers
- Anything that looks like exam-cheating tooling. Marketing "invisible to screen share" or "undetectable" would hurt badly. In the HEPI/Kortext survey, 53% of students feared being accused of cheating, and proctoring vendors actively block overlay tools such as Cluely.
- Screenshots that leave the device or persist without clear consent.
- Raw LaTeX in answers.
- A paywall or key-setup wall before the first answer.
- Stealing focus, hotkey conflicts, or breaking on a managed school laptop.

### Feature suggestions beyond what's built
- Study mode with a "hint, don't solve" default and an easy switch to full answers.
- Region capture with a highlight box.
- Clipboard-first flow: copy text, press Alt+Space, ask.
- Per-day spend cap for students on pay-as-you-go keys.
- Citation check against Crossref or a similar index.
- One-click "make flashcards" from an answer or captured slide.

### Confidence
Statistics are from search snippets, not fetched pages. Almost no direct Reddit evidence was found, so student sentiment is largely inferred. The Perplexity student-offer details conflict across sources. Study mode, region highlighting, spend cap, and citation check are the agent's inferences and should be tested with real students.

### Sources
- HEPI/Kortext 2025 survey: https://www.hepi.ac.uk/2025/02/26/hepi-kortext-ai-survey-shows-explosive-increase-in-the-use-of-generative-ai-tools-by-students/
- Google student Gemini offer: https://blog.google/innovation-and-ai/products/gemini-app/student-offer-google-ai/ ; https://www.tomsguide.com/ai/google-gemini/google-is-giving-students-a-free-year-of-gemini-pro-plus-new-ai-study-features
- Click to Do: https://pureinfotech.com/use-click-to-do-windows-11/ ; https://www.pcworld.com/article/2910874/what-is-click-to-do-meet-microsofts-next-ai-headliner-for-windows-pcs.html
- Perplexity citation critique: https://redescuela.org/ai-tools/perplexity-ai-review/
- LaTeX bug reports: https://community.openai.com/t/not-rendering-latex-properly/1288417.md ; https://community.openai.com/t/rendering-math-symbols-shows-underlying-markup-instead-of-expected-symbol/1117613
- Cluely and proctoring: https://proctor360.com/blog/block-cluely-ai-cheating-proctoring
- Weak/conflicting: https://slashtest.huit.harvard.edu/free-perplexity-pro-for-students ; https://dupple.com/learn/chatgpt-for-students

---

## 4. Non-technical office / knowledge worker

### Persona
Analyst, project manager, marketer, finance, HR, or sales person who lives in Outlook, Teams, Excel, PowerPoint, and browser tabs. Already uses AI, often on a personal account. Wants fast help without learning a tool, and works under an IT department that is wary of data leaks.

### Top needs (agent's ranking)
1. Get unstuck inside the app they're in (explain or fix an Excel formula, read a dense slide, make sense of a long email thread).
2. Draft and reply faster, in a specific tone.
3. Summarize documents, notes, threads, or web pages.
4. Get a sanctioned tool that works without breaking policy.
5. Quick one-off Q&A with no setup or account juggling.

### Pain points with existing tools
- **Cost and value:** Copilot is priced per seat and reviewers call it "useful but often not worth its price." Accuracy complaints are common, especially in Excel calculations.
- **Unwanted AI in the OS:** users remap or disable the Copilot key and unpin or uninstall Copilot.
- **Shadow AI:** 59% of US employees reported using unapproved AI tools in one survey (51% in another). Only 33% said approved tools fully meet their needs.
- **Blocking backfires:** one admin reported engineers moving to phones within two weeks of a firewall block.
- **Pasting data bypasses DLP** because text goes into a chat box rather than a file upload.

### Must-haves
- Zero-config first run: auto-detect an existing sign-in (Claude Code, Codex, Gemini CLI) or local Ollama/LM Studio, and say so on screen.
- Visible privacy state on every ask, for example "Screen → Claude (cloud)".
- One-click copy that preserves formatting (rich text for Outlook/Word, tab-separated tables for Excel).
- Quick actions and presets: "Explain this formula," "Summarize this window," "Reply in a warmer tone," "Make shorter."
- Rebindable hotkey on first run (Alt+Space conflict is inferred, not verified).

### Nice-to-haves
- Selected-text input (Excel range, email body) since screenshots are blurry on high-DPI monitors.
- Pre-send redaction of emails, account numbers, and names.
- Admin policy file to lock providers and disable screenshots.
- Tone presets per recipient.
- Snippet/prompt library with team sharing.

### Deal-breakers
- Install requiring a terminal, Node/npm, or a pasted API key before the first answer. This is the biggest risk, though evidence is only vendor blogs.
- Screenshots that appear to leave the machine without clear notice.
- Slow first token or a stalled stream with no progress indication.
- Output that can't be pasted cleanly.
- Hotkey conflicts.
- Always-on or auto-start behavior the user didn't ask for.
- Admin-blocked installs with no IT/security page explaining what is sent, where, and retention.

### Feature suggestions beyond what's built
- "Ask about this selection" via hotkey on selected cells or email text.
- Redaction preview before any send.
- Per-app context profiles (Excel formula-aware, Outlook reply-to-thread).
- Org-deployable config (silent install plus a policy JSON).
- Local-only mode indicator and one-click "clear this conversation now."
- Setup wizard with a test question that verifies the connection in under a minute.

### Confidence
High that shadow AI, value skepticism, and Copilot-key backlash are real. Medium on shadow-AI percentages (vendor-commissioned, differing definitions). Low on specific abandonment rates from setup friction. No Reddit evidence on screenshot-based Excel help was found. The Alt+Space conflict, zero-config recommendation, org policy idea, and API-key abandonment claim are inferred.

### Sources
- Microsoft/LinkedIn 2024 Work Trend Index: https://news.microsoft.com/source/2024/05/08/microsoft-and-linkedin-release-the-2024-work-trend-index-on-the-state-of-ai-at-work/
- 2025 Work Trend Index: https://www.microsoft.com/en-us/worklab/work-trend-index/2025-the-year-the-frontier-firm-is-born
- Copilot value/accuracy: https://www.techradar.com/pro/microsoft-365-copilot-is-generally-available-now-but-theres-an-expensive-hitch ; https://msdynamicsworld.com/node/34818 ; https://tech.yahoo.com/ai/articles/microsofts-pricey-ai-assistant-copilot-213938908.html
- Shadow AI: https://www.wicpa.org/news/articles/2858:survey-59-of-us-employees-use-unapproved-ai-tools-for-work ; https://www.journalofaccountancy.com/news/2025/nov/lurking-in-the-shadows-the-costs-of-unapproved-ai-tools/ ; https://techstrong.ai/features/shadow-ai-is-the-silent-data-risk-taking-over-workplaces-survey/
- Copilot key remapping: https://www.windowscentral.com/artificial-intelligence/microsoft-copilot/the-copilot-pc-brand-is-dead-heres-how-to-reclaim-and-remap-the-copilot-key
- Blocking: https://community.netskope.com/next-gen-swg-2/how-to-block-microsoft-copilot-by-going-through-bimng-com-6012 ; https://comparethecloud.net/articles/shadow-ai-uk-it-managers/
- Onboarding friction: https://usetandem.ai/blog/solo-user-onboarding-activate-founders-without-it-support ; https://www.digitalocean.com/blog/hidden-cost-of-complex-ai-platforms-developer-experience
- Grammarly tone rewrites: https://voicebot.ai/2022/10/25/grammarlys-ai-writing-assistant-now-suggests-tone-rewrites-for-more-positive-and-personable-communication/

---

## 5. Accessibility-focused user

### Persona
A composite user with low vision (high zoom or large text), a screen reader (NVDA or Narrator), keyboard-first or voice-assisted input, and English as a second language. They summon the bar dozens of times a day from inside other apps and need it to behave predictably. They can't visually check an AI answer, so they depend on accurate text and on knowing exactly what was sent.

### Top needs (agent's ranking)
1. Summon, type, and dismiss without losing focus, and hear where they are.
2. Hear the answer in a usable form, without a flood of fragments.
3. Read text on a translucent surface in light, dark, and high-contrast modes.
4. Know exactly what context is sent, especially a screenshot, and turn it off with confidence.
5. Dictate the prompt without shortcuts colliding with the speech.
6. Ask and read in their own language, and get simpler explanations.
7. Control motion, timing, and text size.

### Pain points with existing tools
- **ChatGPT audits (2023):** unlabeled controls under NVDA and VoiceOver, and focus that drifts away from the input after a response. A 2025 expert study found usability problems across ChatGPT, Copilot, Perplexity, and Gemini.
- **Streaming output:** token-by-token updates either re-read the whole answer or get skipped.
- **Be My AI:** blind users can't verify descriptions, and face descriptions are often blurred or omitted.
- **Seeing AI:** a reviewer reported wrong colour descriptions.
- **Win+H:** reportedly stops on pauses in some apps and works only in editable fields.
- **Glass UIs:** text on translucent panels fails contrast as the background changes.

### Must-haves (WCAG mappings from the agent)
- **Status announcements (WCAG 4.1.3):** a polite live region for "Generating," "Response complete," and errors, kept separate from the streamed text.
- **Stop control (2.2.2):** a labeled Stop button for streaming. Whether a stream counts as "essential" is a judgment call.
- **Focus on summon (2.4.3, 2.4.11):** focus goes to the input, is restored on dismiss, and the bar never covers the focused element.
- **Contrast (1.4.11 at 3:1 and 1.4.3 at 4.5:1):** borders, focus rings, chips, send button, and text, with a solid fallback behind text.
- **State not by colour alone (1.4.1):** the Screen chip needs text or an icon, not just a tint.
- **Target size (2.5.8):** at least 24×24 CSS px.
- **Forced colours:** `@media (forced-colors: active)` with system colours and explicit borders.
- **Reduced motion:** honour Windows "Animation effects" via `prefers-reduced-motion` (already partly built).
- **Text scale (1.4.4, 1.4.10):** in-app zoom, test at Windows 200% and a 320-CSS-px reflow.
- **Character shortcuts (2.1.4):** current Ctrl/Alt shortcuts pass. Any single-key shortcut needs a turn-off or remap.

### Nice-to-haves
- Read-aloud for the last answer.
- Prompt-language and answer-language choice, with `lang` attributes (3.1.2).
- A "Simpler" button for plain-language rewrites (early evidence).
- Verify the prompt field is a real editable control so Win+H and Voice Access both work.

### Deal-breakers
- Focus lost or jumping on summon with no announcement.
- Bar auto-dismissing on blur while the user is reading.
- Token-by-token speech spam, or no signal that a response finished.
- Text below contrast on glass, or unusable under forced colours.
- Screenshot sent without a clear, announced indicator.
- Animations that can't be turned off.
- Dictation that deletes text on a pause.

### Feature suggestions beyond what's built
- A "Describe what I'm sending" line before send (for example "Sending: Monitor 2, 1920×1080").
- A "Read last answer" key that announces through UI Automation.
- Answers that start with a one-sentence summary and use heading structure.
- A pin toggle that stops auto-dismiss.
- A "solid surfaces" option separate from Windows Transparency effects.
- Shift+Enter for newline so voice commands like "new line" don't send by accident (inferred).

### Confidence
Medium-high on WCAG criteria and streaming/live-region guidance. Medium on Windows setting labels. Low on user-demand evidence: it is anecdotal and mostly 2019 to 2023. No controlled study on multilingual or non-native users was found. WCAG 4.1.3, 1.4.1, 2.2.1, 3.1.2 and WebView2 support for `forced-colors` and `prefers-reduced-motion` were not verified in sources.

### Sources
- WCAG 2.2.2: https://www.w3.org/WAI/WCAG20/Understanding/pause-stop-hide.html
- WCAG 2.1.4: https://www.w3.org/WAI/WCAG21/Understanding/character-key-shortcuts
- WCAG 2.5.8: https://tabnav.com/academy/wcag/success-criterion-2.5.8
- WCAG 2.4.11: https://www.disabilityworld.org/toolkit/standards/wcag/2-4-11-focus-not-obscured-minimum/ ; https://kb.daisy.org/publishing/docs/wcag/focus-not-obscured-minimum.html
- Text resize and reflow: https://www.yatil.net/blog/resize-text-reflow
- Reduced motion on Windows: https://vispero.com/resources/short-note-on-prefers-reduced-motion-and-puzzled-windows-users/
- Forced colors: https://developer.mozilla.org/en-US/docs/Web/CSS/@media/forced-colors ; https://blogs.windows.com/msedgedev/2020/09/17/styling-for-windows-high-contrast-with-new-standards-for-forced-colors/
- UI Automation live setting: https://learn.microsoft.com/en-au/dotnet/api/system.windows.automation.automationlivesetting?view=net-10.0
- Streaming chat accessibility: https://azukiazusa.dev/en/blog/accessible-streaming-chat-ui ; https://tianpan.co/blog/2026/04/17/ai-accessibility-streaming-screen-readers
- Glass UI contrast: https://axesslab.com/?p=5111
- ChatGPT audits: https://iteach.commons.msu.edu/2023/12/20/addressing-chatgpt-accessibility-challenges/ ; https://captechconsulting.com/technical/artificial-intelligence-accessibility-a-look-into-chatgpt
- Expert study (PETRA 2025): https://ricerca.univaq.it/bitstream/11697/267220/1/PETRA25.pdf
- Be My AI: https://www.perkins.org/resource/be-my-eyes-app-facial-recognition-feature/ ; https://charonhub.deeplearning.ai/blind-users-can-use-ai-models-as-virtual-mirrors-but-dont-always-like-what-they-see/
- Win+H / Voice Access: https://forums.ankiweb.net/t/windows-voice-typing-functionality-is-limited-on-this-app/23255 ; https://support.microsoft.com/en-US/Accessibility/windows/voice-access/dictate-text-with-voice
- Seeing AI: https://employmentservices.visionaustralia.org/news/2019-08-23/tech-talk-david-woodbridge-reviews-seeing-ai ; https://apps.appfollow.io/ios/seeing-ai/999062298?country=us
- Plain-language rewriting: https://arxiv.org/abs/2405.07475?context=cs

---

## 6. Privacy-conscious user and enterprise IT/security admin

### Persona
An enterprise IT, security, or compliance lead (or privacy-minded employee) evaluating a Windows AI tool that can read screen contents. Every question is framed as data leakage: GDPR processor and transfer terms, DLP, SmartScreen and application control, Intune/GPO, and where API keys live. Shadow AI is assumed to be happening already, so the bar is a sanctioned, controllable option.

### Top needs (agent's ranking)
1. A central kill switch (GPO/Intune) to disable screen capture or the whole feature, like Recall's off-by-default model on managed devices.
2. A known data path: which provider receives a screenshot, under which contract, with what retention and residency.
3. Sensitive-window exclusion (password managers, banking, remote desktop, private browsing).
4. Trusted, signed distribution that passes SmartScreen, Smart App Control, and WDAC.
5. Secret handling: keys in Credential Manager or DPAPI, never plaintext config or logs.
6. Verifiable local-only mode and no unexplained telemetry.
7. Audit evidence: content-free logs exportable to a SIEM.

### Pain points with existing tools
- **Recall:** unresolved dispute over whether snapshots are stored in plaintext or encrypted, and reported filter gaps. Microsoft's documentation says filtering is on by default and snapshots never leave the device. Recall is Copilot+ PC only.
- **SmartScreen:** Microsoft says EV certificates no longer bypass warnings. New builds can take weeks and hundreds of installs to build reputation. Smart App Control can block unsigned executables regardless of origin.
- **Raycast:** its AI privacy page contradicts itself on whether BYOK requests pass through Raycast's servers.
- **Ollama:** binds to 127.0.0.1 by default, but `OLLAMA_HOST` widens exposure, there is no documented local API authentication, and it auto-downloads updates on Windows. It does not claim zero telemetry. `OLLAMA_NO_CLOUD` disables cloud features.
- **ChatGPT Windows app:** no documented admin control over screen capture in the companion window was found (not exhaustive).
- **Remote desktop:** Recall only filters RDP clients that implement capture protection.
- **Shadow AI:** vendor-sourced figures point to most employees using unapproved tools.

### Must-haves for this app
- Per-question opt-in capture, a visible "capturing" state, and a preview before send.
- Verify the bar is actually hidden from Recall and common capture tools. Microsoft says `WDA_EXCLUDEFROMCAPTURE` keeps content out of Recall and other screenshot apps, but it is not a security or DRM guarantee.
- A written egress table for each mode (CLI subscription, API key, local).
- Handle the CLI-subscription mode concern: it routes work content through employees' personal consumer accounts. This is the biggest enterprise blocker. Make it visible or support organizational accounts.
- Confirm the "no telemetry" claim with packet capture before publishing it.
- Signed installer, silent-install switches, and a winget manifest with SHA256. Winget requires the installer to come from the publisher's own site.
- Document each CLI's own session storage. The app's 10-minute reset does not control what the CLIs keep on disk (inferred, verify per CLI).

### Deal-breakers
- Unsigned binary on Windows 11 with Smart App Control enabled.
- No central way to disable capture or restrict providers.
- Personal consumer subscriptions on corporate devices with no organizational path.
- Plaintext keys, keys in logs or settings, or undocumented network endpoints.
- No sub-processor list or DPA route for any cloud provider.
- A local server listening beyond localhost without authentication.

### Feature suggestions beyond what's built
1. Policy file and ADMX template: HKLM keys read before user settings (`disableScreenCapture`, `allowedProviders`, `forceLocalOnly`, `disableBYOK`, locked idle-reset time), mirroring Recall's `AllowRecallEnablement` and `DisableAIDataAnalysis` split.
2. Capture blocklist (process name plus window-title regex), default-on for common password managers and private browsing, like Recall's `SetDenyAppListForRecall`.
3. Enforced local-only mode that refuses non-allowlisted outbound connections and shows the endpoint list.
4. Pre-send redaction for card numbers, SSNs, and API-key prefixes, with a preview.
5. Signed MSI/MSIX via Microsoft Artifact Signing (about $9.99/month per Microsoft's SmartScreen page).
6. Content-free audit log (timestamp, provider, capture yes/no, policy state) in JSON for SIEM.
7. A data-flow document with sub-processors, per-provider retention, and residency options.
8. Optional DLP hook (Recall's provider API supports Microsoft Purview).

### Confidence
- **High:** Recall admin policies and defaults, the EV/SmartScreen change, `WDA_EXCLUDEFROMCAPTURE` behavior, Ollama defaults, the Raycast contradiction. These were verified from fetched primary pages.
- **Medium:** Recall filter gaps and the plaintext dispute (secondary), shadow-AI percentages (vendor data), winget rules, Artifact Signing pricing.
- **Low/inferred:** the 2026 Azure CA rotation SmartScreen issue (single source), Anthropic direct-API EU residency (sources conflict), CLI-side session retention, whether `WDA_EXCLUDEFROMCAPTURE` stops third-party capture tools.
- **Not verified:** Purview endpoint-DLP coverage of general screen capture, and any GDPR legal conclusion. This is not legal advice.

### Sources
Fetched primary pages:
- Manage Recall: https://learn.microsoft.com/en-us/windows/client-management/manage-recall
- SmartScreen reputation: https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation
- SetWindowDisplayAffinity: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setwindowdisplayaffinity
- Ollama FAQ: https://docs.ollama.com/faq
- Raycast AI privacy: https://manual.raycast.com/raycast-ai-privacy-security

Search snippets only:
- https://www.darkreading.com/data-privacy/microsofts-recall-feature-draws-criticism-from-privacy-advocates
- https://www.techrepublic.com/article/news-microsoft-recall-expands-rollout/
- https://kpmg.com/kpmg-us/content/dam/kpmg/pdf/2025/shadow-ai-already-here-take-control-reduce-risk-unleash-innovation.pdf
- https://www.teramind.co/wp-content/uploads/2026/06/The-Shadow-AI-Behavior-Report.pdf
- https://labs.cloudsecurityalliance.org/research/csa-research-note-shadow-ai-apps-enterprise-20260530/
- https://openai.com/blog/introducing-data-residency-in-europe
- https://mc.merill.net/message/MC1269217
- https://learn.microsoft.com/vi-vn/windows/package-manager/package/repository
- https://learn.microsoft.com/azure/artifact-signing/faq
- https://docs.citrix.com/en-us/citrix-workspace-app/app-protection/app-protection-blocking-unauthorized-ai.html
- https://learn.microsoft.com/en-ie/Windows/Win32/secbp/handling-passwords
- https://help.openai.com/en/articles/9982051-using-the-chatgpt-windows-app

---

## 7. Creative / gamer / hobbyist

### Persona
Gamers, streamers, and users of Blender, Photoshop, DaVinci Resolve, and similar GPU-heavy tools. Mid-task, hands on keyboard and mouse, often fullscreen or rendering. They want an answer in seconds without alt-tabbing. Frame rate and render time come first. They are skeptical of anything that touches anti-cheat, steals GPU time, or sends screens to the cloud, and they prefer "show me where" to "explain what this is."

### Top needs (agent's ranking)
1. Instant answers that don't break flow.
2. "Where is this control?" help from a screenshot. This is the clearest gap, though third-party tools are starting to cover it.
3. In-game help (builds, counters, item and stat lookups, with spoiler control).
4. Offline or local models (inferred, thin evidence).
5. Creative workflow prompts for Photoshop and Blender.

### Pain points with existing tools
- **Overhead:** Overwolf's own support docs list dock lag and stutter in-game, and conflicts with RivaTuner. A G-Assist reviewer found it "completely unusable" while gaming and reported lower frame rates. Gaming Copilot took about 1 to 2 s for text, 3 to 5 s with a screenshot, and 7+ s for long questions.
- **Accuracy:** Gaming Copilot gives confident wrong answers, and G-Assist told one reviewer he wasn't playing a game.
- **Anti-cheat uncertainty:** FACEIT can block Overwolf from game events in CS2. Ban claims on EA forums conflict.
- **Privacy:** Gaming Copilot's screen access and default conversation saving.
- **Hotkey clashes:** Alt+Space opens the system menu and collides with Alt controls in windowed games.
- **Availability:** Gaming Copilot requires a Microsoft account and has age/region limits.

### Must-haves
- Rebindable hotkey with a setup-time conflict check.
- Overlay that doesn't steal focus, Esc to dismiss, Enter to send.
- Fullscreen awareness: detect a foreground game or fullscreen app and disable screen capture there by default.
- Clear preview of what will be sent before anything leaves the machine.
- Near-zero idle cost (no polling, no resident model unless needed).
- Local-model path with a VRAM warning and an option to unload the model when a game launches.

### Nice-to-haves
- A gaming mode that pauses inference, hides the bar, and suppresses captures.
- Per-app profiles (Blender, Photoshop, Resolve) with prompt presets.
- Crop-to-select capture to cut tokens and latency.
- Copy buttons for code and step lists.

### Deal-breakers
- Any FPS drop, stutter, or crash caused by the overlay or model.
- Anything that looks like an anti-cheat flag or leaves ban risk unaddressed.
- A hotkey that breaks games or other apps.
- Silent screen capture or conversations saved without notice.
- Wrong menu names or paths.
- Multi-second waits for simple questions.
- A local model that eats VRAM and makes the game stutter.

### Feature suggestions beyond what's built
- Game-aware safe mode with a visible "in-game safe" status.
- Point-at-it answers: exact menu paths with UI labels, such as "Edit > Preferences > Interface."
- Creative prompt packs for Blender, Photoshop, and Resolve (verify every menu path before shipping).
- VRAM budget readout plus auto-unload on game launch.
- "Local" vs "Cloud" chip so users never send a screenshot to the cloud by accident.
- Per-app session memory ("I'm in Blender 4.2, Cycles").
- Self-measured overhead readout in the tray.

### Confidence
No Reddit threads surfaced, so community evidence is thin. Many sources are dated: the Alt+Space material is from 2005 to 2016 and the G-Assist reviews are 440 to 860 days old. Several sources are biased (Overwolf, iTero, TryUncle are self-promotional or competitive). Exclusive fullscreen generally can't show a normal overlay, so borderless windowed is the practical target (inferred). The fullscreen detection and gaming-mode designs are the agent's recommendations, not sourced findings.

### Sources
- Overwolf support: https://support.overwolf.com/support/solutions/9000115816
- EA forum, Overwolf bans: https://forums.ea.com/discussions/apex-legends-general-discussion-en/are-people-still-getting-banned-for-using-the-overwolf-overlay/5723641
- Gaming Copilot: https://www.igorslab.de/en/microsoft-brings-gaming-copilot-to-the-xbox-game-bar-useful-helper-or-just-ballast/ ; https://finalboss.io/i-put-xboxs-gaming-copilot-through-hell-why ; https://www.techradar.com/computing/windows/microsofts-gaming-copilot-ai-is-now-in-windows-11s-game-bar-and-i-cant-decide-whether-this-is-a-good-or-bad-thing
- NVIDIA G-Assist: https://www.nvidia.com/en-my/software/nvidia-app/g-assist/ ; https://www.windowscentral.com/hardware/cpu-gpu-components/nvidia-project-g-assist-tested
- Blender AI add-ons: https://blenderartists.org/t/the-blender-ai-assistant/1601597
- ChatGPT screenshot-menu issue: https://github.com/OpenShot/openshot-qt/issues/6064
- Adobe Community: https://community.adobe.com/announcements-710/explore-new-possibilities-with-photoshop-for-chatgpt-1554299
- Porofessor: https://www.itero.gg/articles/what-is-the-best-league-of-legends-companion-app-in-2025 ; https://www.softpedia.com/get/Gaming-Related/Porofessor.shtml
- Alt+Space in windowed games: https://forums.ea.com/discussions/star-wars-battlefront-2-en/rolling-in-window-mode-activates-the-window-focus-if-you-press-alt-and-space/10734470/replies/10734476 ; https://portableapps.com/node/18863
- Vanguard: https://upcomer.com/valorants-mandatory-vanguard-anti-cheat-found-to-be-highly-invasive
- Resolve 21.1 AI assistant: https://www.notebookcheck.net/Blackmagic-DaVinci-Resolve-21-1-is-here-AI-assistant-and-more-than-100-new-tools.1396024.0.html ; https://ymcinema.com/2026/09/09/davinci-resolve-21-1-chatgpt-claude-ai-assistants/
- Point-at-control vendor claim: https://tryuncle.com/learn/davinci-resolve/best-ai-assistant-for-davinci-resolve-beginners
- Local LLM VRAM: https://www.sitepoint.com/10gb-vram-local-llm-the-complete-setup-guide-2026/

---

## 8. Tech-support "family IT person" and everyday non-technical user

### Persona
An older adult (50+), parent, or small-business owner who is not a technologist and wants to understand a dialog, bill, form, or suspicious message, and get the next click right. Scam-anxious and skeptical of AI. Often a family "IT person" does the real troubleshooting, so the app should produce something that person can follow or pass along.

### Top needs (agent's ranking)
1. "What does this mean and what do I click?" in plain language, with numbered steps and a safe default.
2. "Is this a scam?" for texts, emails, and calls, especially urgent ones or ones asking for remote access.
3. "How do I do X in this program?"
4. Explaining bills, forms, and notices without sending account numbers anywhere.
5. A clean summary to send to a relative or support person.

### Pain points with existing tools
- **Trust and privacy are the main barrier.** AARP's 2025 survey (about 3,838 adults) found roughly half of older adults cite trust, privacy, and data security as reasons to hesitate. About 51% of 50+ respondents called AI insecure or unsafe and 39% did not trust its accuracy (secondary coverage).
- **Screen-sharing assistants are uneven.** Gemini Live reviews describe inconsistent identification, misread text, and sessions that stall. Copilot Vision reviewers worry sensitive on-screen data can slip through.
- **Older-user UIs are poorly served.** NNG's 2013 research found senior task success of 55% versus 75%.
- **Polished AI text hurts support.** Staff report AI-rewritten problem descriptions bury the actual symptom.
- **Tech-support scams** are a top elder-fraud complaint, and Quick Assist is abused by impersonators.

### Must-haves
- Short plain-language answers by default, with numbered steps, one action per step, and "stop if you see X."
- A "What's this?" one-click prompt pre-filled with the screenshot.
- Before any screenshot is sent, show exactly what will be sent, and warn if it looks like it contains account numbers, card numbers, or verification codes.
- A copy button for error text.
- A large default text size (at least 14pt on dialogs) with high contrast. The current glass styling risks being too faint.
- A visible "Stop sharing" control.
- Scam mode that never recommends clicking links or calling numbers in a message, and always says to verify through an official number.

### Nice-to-haves
- Read-aloud for answers.
- A "Summary for my helper" button (symptom, program, version).
- Plain-language mode that says "I'm not sure" rather than guessing.

### Deal-breakers
- Onboarding that requires npm or a terminal. No direct survey evidence was found, so this is a strong inference and the top risk.
- Anything that silently captures or uploads the screen.
- Confident wrong answers on scams or bills.
- Tiny text, low contrast, or jargon.
- Anything that asks the user to install remote-access tools, which is exactly the scam pattern.
- Losing context too fast, or no way to delete history.

### Feature suggestions beyond what's built
- Scam-check preset: a fixed prompt with a checklist (urgency, unknown sender, links, phone numbers in pop-ups, remote access requests, gift cards), outputting a verdict plus the official way to verify.
- Auto-redaction preview that masks long digit strings and card- or account-like patterns before sending.
- Privacy-first history controls: "forget this conversation now" and a clear note on retention.
- Plain-language disclosure that it is AI and not a licensed professional for bills, banking, or medical decisions. This matches what a proposed Senate bill (S.5117, introduced July 2026, not law) would require.
- Shareable helper summary.
- Guided-setup status panel showing what is connected and what to do next.

### Confidence
Specifics are from search excerpts, not fetched pages. Several survey figures are secondary, so check the primary AARP report before quoting. No Reddit r/techsupport or r/sysadmin threads and no usability study of AI screen-explainers with older adults were found. The Quick Assist warning wording could not be verified against Microsoft's own pages. The npm-as-top-blocker claim, the redaction and read-aloud usage, and the 14pt default are inferred.

### Sources
- AARP: https://www.aarp.org/2026techtrends ; https://seniorhousingnews.com/2025/12/19/older-adults-embracing-tech-using-ai-new-aarp-report-shows/
- OpenAI scam-spotting (Truedot survey): https://openaiglobalaffairs.substack.com/p/scam-spotting-and-stopping
- SoUPS 2025 slides: https://www.usenix.org/sites/default/files/conference/protected-files/soups2025_slides_wang.pdf
- SeniorShield pilot: https://leadingage.org/new-cast-case-study-ai-powered-app-protects-residents-from-scam-messages/
- Copilot Vision: https://www.thurrott.com/a-i/322087/copilot-vision-on-windows-is-now-available-in-the-us ; https://support.microsoft.com/en-GB/accessibility/copilot/basic-tasks-using-a-screen-reader-with-copilot-vision ; https://www.pcworld.com/article/2974827/microsofts-own-windows-ad-shows-copilot-giving-wrong-instructions.html
- Gemini Live: https://beebom.com/tested-gemini-live-screen-sharing-and-camera-capabilities-pixel-9/amp/ ; https://9to5google.com/2025/04/08/gemini-live-video-hands-on-the-world-facing-camera-weve-waited-for-video/ ; https://discuss.ai.google.dev/t/gemini-2-0-not-accessing-live-stream-video-audio-inputs/54092
- Quick Assist abuse: https://www.bleepingcomputer.com/news/security/windows-quick-assist-abused-in-black-basta-ransomware-attacks/amp/ ; https://petri.com/microsoft-quick-assist-tech-support-scams/
- NNG seniors: https://www.nngroup.com/articles/usability-seniors-improvements/
- Senior Chatbot Protection Act: https://www.kelly.senate.gov/?p=86531 ; https://www.govinfo.gov/app/details/BILLS-119s5117is
- Oversharing: https://www.trendmicro.com/internet-safety/blog/is-it-safe-to-share-personal-info-with-an-ai-chatbot/ ; https://www.crimestoppersvic.com.au/current-focus/onlinesafety/screenshots-and-strangers/
- Support tickets: https://www.ustech.ninja/why-ai-generated-requests-are-slowing-down-your-tech-support-and-what-to-do-instead/
