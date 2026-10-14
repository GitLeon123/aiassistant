// In-browser stand-in for the native host, used by `npm run dev` / previews.
// It mimics App.cpp's protocol closely enough to exercise every UI state.

import type { Transport } from './bridge';
import { mergePatch } from './state.svelte';
import type { CaptureRule, HostMessage, ModelInfo, ProviderInfo, ScreenInfo, SettingsPayload, ThemeInfo, UiMessage } from './types';

const effort = (levels: string[], def: string) => ({ mode: 'effort', levels, default: def });
const none = { mode: 'none', levels: [], default: '' };

const MODELS: Record<string, ModelInfo[]> = {
  anthropic: [
    { id: 'claude-opus-5-5', name: 'Claude Opus 5.5', vision: true, reasoning: effort(['low', 'medium', 'high', 'xhigh', 'max'], 'high') },
    { id: 'claude-sonnet-5-5', name: 'Claude Sonnet 5.5', vision: true, reasoning: effort(['low', 'medium', 'high', 'max'], 'medium') },
    { id: 'claude-haiku-5-5', name: 'Claude Haiku 5.5', vision: true, reasoning: { mode: 'effort', levels: ['low', 'medium', 'high', 'xhigh', 'max'], default: 'medium' } },
  ],
  'claude-cli': [
    { id: 'opus', name: 'Opus', vision: true, reasoning: effort(['low', 'medium', 'high', 'xhigh', 'max'], 'high') },
    { id: 'sonnet', name: 'Sonnet', vision: true, reasoning: effort(['low', 'medium', 'high', 'max'], 'medium') },
    { id: 'haiku', name: 'Haiku', vision: true, reasoning: { mode: 'effort', levels: ['low', 'medium', 'high', 'xhigh', 'max'], default: 'medium' } },
  ],
  'codex-cli': [
    { id: 'gpt-5.5', name: 'GPT-5.5', vision: true, reasoning: effort(['minimal', 'low', 'medium', 'high', 'xhigh'], 'medium') },
    { id: 'gpt-5.5-codex', name: 'GPT-5.5 Codex', vision: true, reasoning: effort(['low', 'medium', 'high', 'xhigh'], 'medium') },
  ],
  openai: [
    { id: 'gpt-5.5', name: 'GPT-5.5', vision: true, reasoning: effort(['minimal', 'low', 'medium', 'high'], 'medium') },
    { id: 'gpt-5.5-mini', name: 'GPT-5.5 mini', vision: true, reasoning: effort(['minimal', 'low', 'medium', 'high'], 'low') },
  ],
  ollama: [
    { id: 'llava:latest', name: 'LLaVA', vision: true, reasoning: none },
    { id: 'qwen3:14b', name: 'qwen3:14b', vision: false, reasoning: effort(['low', 'medium', 'high'], 'medium') },
    { id: 'llama3.3', name: 'llama3.3', vision: false, reasoning: none },
  ],
};

const DEFAULT_BLOCKLIST: CaptureRule[] = [
  ...['1password.exe', '1password*.exe', 'bitwarden.exe', 'keepass*.exe', 'dashlane*.exe', 'enpass.exe', 'nordpass.exe', 'keeper*.exe'].map((process) => ({ process, title: '', label: 'Password manager' })),
  ...['mstsc.exe', 'msrdc.exe', 'rdclient*.exe', 'windowsapp.exe', 'teamviewer*.exe', 'anydesk.exe', 'rustdesk.exe'].map((process) => ({ process, title: '', label: 'Remote desktop' })),
  ...['banking', 'online bank', 'internet bank', 'ebanking', 'e-banking', 'bank of', 'sparkasse', 'raiffeisen', 'erste bank'].map((title) => ({ process: '*', title, label: 'Banking window' })),
  ...['incognito', 'inprivate', 'private browsing', 'private window', 'inkognito', 'privates fenster', 'privater modus'].map((title) => ({ process: '*', title, label: 'Private browser window' })),
];

function provider(p: Partial<ProviderInfo> & Pick<ProviderInfo, 'id' | 'name' | 'kind'>): ProviderInfo {
  return {
    description: '',
    defaultBaseUrl: '',
    needsKey: p.kind === 'api',
    keyOptional: false,
    baseUrlEditable: false,
    keyUrl: '',
    defaultModel: MODELS[p.id]?.[0]?.id ?? '',
    hasKey: false,
    ...p,
  };
}

function initialPayload(): SettingsPayload {
  return {
    settings: {
      setupCompleted: true,
      hotkey: 'Alt+Space',
      appearance: 'system',
      backdrop: 'liquid',
      liveBackdrop: true,
      solidSurfaces: false,
      textScale: 100,
      barPinned: false,
      apiDailyBudgetUsd: 0,
      apiRequestAllowanceUsd: 0.25,
      screenContextDefault: true,
      screenMode: 'monitor',
      screenCaptureProtection: true,
      screenCaptureBlocklist: structuredClone(DEFAULT_BLOCKLIST),
      barPosition: 'top-center',
      conversationTimeoutMinutes: 10,
      systemPrompt: '',
      defaultModel: { provider: 'anthropic', model: 'claude-opus-5-5', reasoning: 'high' },
      providers: { 'claude-cli': { enabled: true } },
    },
    providers: [
      provider({ id: 'claude-cli', name: 'Claude', kind: 'cli', description: 'Your Claude Pro/Max subscription, via Claude Code', cli: { installed: true, signedIn: true, path: 'C:\\Users\\you\\.local\\bin\\claude.exe', detail: 'Signed in as you@example.com (Claude Max)' } }),
      provider({ id: 'codex-cli', name: 'ChatGPT', kind: 'cli', description: 'Your ChatGPT Plus/Pro subscription, via the Codex CLI', cli: { installed: true, signedIn: false, path: 'C:\\Users\\you\\AppData\\Roaming\\npm\\codex.cmd', detail: 'Not signed in. Run "codex login".' } }),
      provider({ id: 'gemini-cli', name: 'Gemini', kind: 'cli', description: 'Your Google account, via the Gemini CLI', cli: { installed: false, signedIn: false, path: '', detail: 'Not installed' } }),
      provider({ id: 'anthropic', name: 'Anthropic', kind: 'api', description: 'Claude models with an Anthropic API key', defaultBaseUrl: 'https://api.anthropic.com', keyUrl: 'https://console.anthropic.com/settings/keys', hasKey: true }),
      provider({ id: 'openai', name: 'OpenAI', kind: 'api', description: 'GPT models with an OpenAI API key', defaultBaseUrl: 'https://api.openai.com/v1', keyUrl: 'https://platform.openai.com/api-keys' }),
      provider({ id: 'gemini', name: 'Google Gemini', kind: 'api', description: 'Gemini models with a Google AI Studio key', keyUrl: 'https://aistudio.google.com/apikey' }),
      provider({ id: 'openrouter', name: 'OpenRouter', kind: 'api', description: 'Hundreds of models with one key', keyUrl: 'https://openrouter.ai/keys' }),
      provider({ id: 'xai', name: 'xAI', kind: 'api', description: 'Grok models', keyUrl: 'https://console.x.ai' }),
      provider({ id: 'groq', name: 'Groq', kind: 'api', description: 'Very fast open models' }),
      provider({ id: 'mistral', name: 'Mistral', kind: 'api', description: 'Mistral and Magistral models' }),
      provider({ id: 'deepseek', name: 'DeepSeek', kind: 'api', description: 'DeepSeek chat and reasoner' }),
      provider({ id: 'ollama', name: 'Ollama', kind: 'local', description: 'Local models served by Ollama', defaultBaseUrl: 'http://localhost:11434/v1', baseUrlEditable: true, needsKey: false }),
      provider({ id: 'lmstudio', name: 'LM Studio', kind: 'local', description: 'Local models served by LM Studio', defaultBaseUrl: 'http://localhost:1234/v1', baseUrlEditable: true, needsKey: false }),
      provider({ id: 'custom', name: 'Custom endpoint', kind: 'api', description: 'Any OpenAI-compatible /chat/completions endpoint', baseUrlEditable: true, keyOptional: true }),
    ],
    autostart: true,
    hotkeyError: '',
    hotkeySuggestions: ['Ctrl+Alt+Space', 'Ctrl+Shift+Space', 'Alt+Shift+Space', 'Alt+Space'].map(hotkey => ({ hotkey, available: true })),
    discovery: { running: false, complete: true, nodeInstalled: true, npmInstalled: true, suggestedProvider: 'ollama' },
    defaultCaptureBlocklist: structuredClone(DEFAULT_BLOCKLIST),
    budget: { day: '2026-10-08', reservedUsd: 0, error: '' },
    defaultSystemPrompt:
      "You are AIAssistant, a fast, helpful assistant that lives on the user's Windows desktop. Answer directly and concisely; use Markdown when it improves clarity.",
  };
}

const REASONING = [
  'The user is asking about the code visible in their editor. ',
  'I can see a C++ file with a function that uses `std::vector` and a raw loop. ',
  'They probably want a clearer, idiomatic version. ',
  'I should explain the issue briefly and then show the rewrite with ranges.',
];

const ANSWER = `Here's what's going on in the function on your screen:

1. **The loop copies every element** because \`auto item\` takes each value by copy.
2. The \`erase\` inside the loop invalidates the iterator, which is *undefined behaviour*.

A safer, idiomatic version:

\`\`\`cpp
#include <algorithm>
#include <vector>

void RemoveExpired(std::vector<Session>& sessions, Clock::time_point now) {
  std::erase_if(sessions, [&](const Session& s) { return s.expires < now; });
}
\`\`\`

| Approach | Complexity | Safe? |
|---|---|---|
| \`erase\` in loop | O(n²) | No |
| \`std::erase_if\` | O(n) | Yes |

> Tip: \`std::erase_if\` is C++20; on older compilers use the erase–remove idiom.

See the [cppreference page](https://en.cppreference.com/w/cpp/container/vector/erase2) for details.`;

/** Paints a plausible desktop so the glass has something to refract. */
function fakeDesktop(width: number, height: number, dark: boolean): HTMLCanvasElement {
  const c = document.createElement('canvas');
  c.width = width;
  c.height = height;
  const g = c.getContext('2d')!;
  const bg = g.createLinearGradient(0, 0, width, height);
  if (dark) {
    bg.addColorStop(0, '#0d1b3d');
    bg.addColorStop(0.5, '#3a1f5c');
    bg.addColorStop(1, '#0f3b4a');
  } else {
    bg.addColorStop(0, '#9cc8ff');
    bg.addColorStop(0.5, '#f5b8d6');
    bg.addColorStop(1, '#ffe2a8');
  }
  g.fillStyle = bg;
  g.fillRect(0, 0, width, height);
  const blob = (x: number, y: number, r: number, color: string) => {
    const rg = g.createRadialGradient(x, y, 0, x, y, r);
    rg.addColorStop(0, color);
    rg.addColorStop(1, 'transparent');
    g.fillStyle = rg;
    g.fillRect(0, 0, width, height);
  };
  blob(width * 0.2, height * 0.3, 260, dark ? 'rgba(80,140,255,.7)' : 'rgba(80,140,255,.55)');
  blob(width * 0.8, height * 0.25, 240, 'rgba(255,90,160,.55)');
  blob(width * 0.6, height * 0.85, 300, 'rgba(255,190,60,.5)');

  // A "code editor" window behind the bar.
  const wx = width * 0.08;
  const wy = height * 0.08;
  const ww = width * 0.84;
  const wh = height * 0.78;
  g.fillStyle = dark ? '#1e1e24' : '#ffffff';
  g.beginPath();
  g.roundRect(wx, wy, ww, wh, 10);
  g.fill();
  g.fillStyle = dark ? '#2b2b33' : '#eef0f4';
  g.beginPath();
  g.roundRect(wx, wy, ww, 34, [10, 10, 0, 0]);
  g.fill();
  g.font = '13px Segoe UI, sans-serif';
  g.fillStyle = dark ? '#ccc' : '#333';
  g.fillText('session_manager.cpp - Visual Studio Code', wx + 16, wy + 22);
  const colors = dark
    ? ['#ff7b72', '#79c0ff', '#d2a8ff', '#a5d6ff', '#8b949e', '#e6edf3']
    : ['#cf222e', '#0550ae', '#8250df', '#0a3069', '#6e7781', '#1f2328'];
  g.font = '14px Cascadia Code, Consolas, monospace';
  for (let line = 0; line < 22; line++) {
    let x = wx + 54;
    const y = wy + 64 + line * 22;
    if (y > wy + wh - 12) break;
    g.fillStyle = dark ? '#555' : '#aaa';
    g.fillText(String(line + 1).padStart(2, ' '), wx + 16, y);
    const indent = [0, 2, 2, 4, 4, 6, 4, 2, 0][line % 9];
    x += indent * 9;
    const words = 2 + ((line * 7) % 5);
    for (let w = 0; w < words; w++) {
      const len = 3 + ((line * 13 + w * 7) % 9);
      g.fillStyle = colors[(line + w * 3) % colors.length];
      g.fillText('x'.repeat(len).replace(/x/g, () => 'abcdefghijklmnopqrstuvwxyz'[(line * w + len) % 26]), x, y);
      x += (len + 1) * 8.4;
    }
  }
  return c;
}

export function createMockTransport(page: 'bar' | 'settings'): Transport {
  let deliver: (msg: HostMessage) => void = () => {};
  const emit = (msg: HostMessage, delay = 0) => setTimeout(() => deliver(msg), delay);
  const payload = initialPayload();
  const params = new URLSearchParams(location.search);
  payload.settings.solidSurfaces = params.has('solid');
  payload.settings.textScale = [100, 125, 150, 200].includes(Number(params.get('text-scale'))) ? Number(params.get('text-scale')) : 100;
  payload.settings.barPinned = params.has('pinned');
  if (params.has('budget')) { payload.settings.apiDailyBudgetUsd = Number(params.get('budget')) || 1; }
  if (params.has('first-run')) {
    payload.settings.setupCompleted = false;
    payload.settings.defaultModel = { provider: '', model: '', reasoning: '' };
    payload.settings.providers = {};
    payload.providers.forEach(p => { if (p.kind === 'api') p.hasKey = false; });
  }
  if (params.has('no-node')) payload.discovery.nodeInstalled = payload.discovery.npmInstalled = false;
  if (params.has('no-providers')) {
    payload.discovery.suggestedProvider = '';
    payload.providers.forEach(p => {
      p.hasKey = false;
      if (p.kind === 'cli') p.cli = { installed: false, signedIn: false, path: '', detail: 'Not installed' };
    });
  }
  payload.providers.forEach(p => {
    if (p.kind === 'local') p.connection = {
      state: p.id === 'ollama' && !params.has('no-providers') ? 'connected' : 'not-connected',
      detail: p.id === 'ollama' && !params.has('no-providers') ? '3 models available' : 'Local server not running',
      nextStep: p.id === 'ollama' && !params.has('no-providers') ? 'Send a test question.' : 'Start the local server and load a model.',
    };
  });
  if (params.has('hotkey-conflict')) {
    payload.hotkeyError = 'Alt+Space is already used by another app';
    payload.hotkeySuggestions.find(s => s.hotkey === 'Alt+Space')!.available = false;
  }
  let setupId = '';
  let setupProvider = '', setupModelId = '';
  let setupPassed: { id: string; provider: string; model: string } | null = null;
  let setupTimer: ReturnType<typeof setTimeout> | null = null;
  const media = matchMedia('(prefers-color-scheme: dark)');
  const theme = (): ThemeInfo => ({
    dark: params.get('theme') ? params.get('theme') === 'dark' : media.matches,
    accent: params.get('accent') ? `#${params.get('accent')}` : '#4c8dff',
    reducedMotion: params.has('reduced'),
  });
  let visible = false;
  let screenMode: 'monitor' | 'window' = 'monitor';
  let screenId = 0;
  let reviewedScreenId = -1;
  let currentScreen: ScreenInfo;
  let desktop = '';
  let desktopCanvas: HTMLCanvasElement;
  let stream: { id: string; timer: number } | null = null;

  const paintDesktop = () => {
    desktopCanvas = fakeDesktop(window.innerWidth, window.innerHeight, effectiveDark());
    desktop = desktopCanvas.toDataURL('image/jpeg', 0.85);
    if (page === 'bar') {
      document.body.style.background = `url(${desktop}) 0 0 / 100% 100% no-repeat`;
    } else {
      document.body.style.background = effectiveDark() ? '#202024' : '#f3f3f3';
    }
  };
  const effectiveDark = () => {
    const a = payload.settings.appearance;
    return a === 'system' ? theme().dark : a === 'dark';
  };

  const fixture = params.get('blocked');
  const activeApp = fixture === 'password' ? '1Password.exe' : fixture === 'remote' ? 'mstsc.exe' : 'Code.exe';
  const activeTitle = fixture === 'private' ? 'Incognito - Browser' : fixture === 'banking' ? 'Online banking - Browser' : 'session_manager.cpp - Visual Studio Code';
  const blockReason = () => {
    if (!payload.settings.screenCaptureProtection) return '';
    const rule = payload.settings.screenCaptureBlocklist.find((r) => {
      const glob = r.process.replace(/[.+^${}()|[\]\\]/g, '\\$&').replace(/\*/g, '.*').replace(/\?/g, '.');
      return new RegExp(`^${glob}$`, 'i').test(activeApp) && activeTitle.toLowerCase().includes(r.title.toLowerCase());
    });
    return rule ? `Screen capture blocked: ${rule.label}. Review the blocklist in Settings > Privacy.` : '';
  };
  const screenPayload = (): ScreenInfo => {
    const reason = blockReason();
    let width = desktopCanvas.width, height = desktopCanvas.height;
    let thumb = desktop;
    if (!reason && screenMode === 'window') {
      const cropped = document.createElement('canvas');
      width = cropped.width = Math.round(width * 0.84);
      height = cropped.height = Math.round(height * 0.78);
      cropped.getContext('2d')!.drawImage(desktopCanvas, Math.round(desktopCanvas.width * 0.08), Math.round(desktopCanvas.height * 0.08), width, height, 0, 0, width, height);
      thumb = cropped.toDataURL('image/jpeg', 0.85);
    }
    currentScreen = { available: !reason, title: activeTitle, app: activeApp, hasWindow: true, mode: screenMode,
      id: ++screenId, monitor: 2, width, height,
      thumb: reason ? '' : params.has('preview-error') ? '/missing-preview.jpg' : thumb, status: reason ? 'blocked' : 'captured', reason };
    reviewedScreenId = -1;
    return currentScreen;
  };
  if (params.get('provider') === 'ollama') {
    payload.settings.providers.ollama = { enabled: true, baseUrl: params.get('endpoint') || 'http://localhost:11434/v1' };
    payload.settings.defaultModel = { provider: 'ollama', model: 'llava:latest', reasoning: '' };
  }

  media.addEventListener('change', () => {
    paintDesktop();
    deliver({ type: 'theme', theme: theme() });
  });

  const show = () => {
    visible = true;
    paintDesktop();
    deliver({
      type: 'show',
      backdrop: !blockReason() && payload.settings.backdrop === 'liquid' ? desktop : '',
      screen: screenPayload(),
      screenDefault: payload.settings.screenContextDefault,
      position: payload.settings.barPosition,
      conversationReset: false,
      scale: devicePixelRatio,
    });
  };

  // When the bar is "hidden", click anywhere or press Alt+Space to bring it back.
  if (page === 'bar') {
    window.addEventListener('keydown', (e) => {
      if (e.altKey && e.code === 'Space') {
        e.preventDefault();
        if (visible) deliver({ type: 'dismiss' });
        else show();
      }
    });
    window.addEventListener('pointerdown', () => {
      if (!visible) setTimeout(show, 0);
    });
  }

  const settingsChanged = () => emit({ type: 'settings', payload: structuredClone(payload) });
  const reserveApiAllowance = (providerId: string) => {
    const p = payload.providers.find((p) => p.id === providerId);
    const endpoint = payload.settings.providers[providerId]?.baseUrl || p?.defaultBaseUrl;
    if (p?.kind === 'cli' || p?.kind === 'local' && /^http:\/\/(localhost|127\.0\.0\.1|\[::1\])(:|\/|$)/.test(endpoint ?? '') || !payload.settings.apiDailyBudgetUsd) return '';
    if (payload.settings.apiRequestAllowanceUsd <= 0) return 'Set a positive allowance per API request in Settings → Usage.';
    if (payload.budget.reservedUsd + payload.settings.apiRequestAllowanceUsd > payload.settings.apiDailyBudgetUsd + 1e-9) return 'Daily API allowance reached. Wait until tomorrow or change Settings → Usage.';
    payload.budget.reservedUsd += payload.settings.apiRequestAllowanceUsd;
    settingsChanged();
    return '';
  };
  if (page === 'bar') window.addEventListener('blur', () => {
    if (visible && !payload.settings.barPinned) deliver({ type: 'dismiss' });
  });

  const startStream = (id: string, withReasoning: boolean) => {
    if (stream) clearInterval(stream.timer);
    let phase: 'status' | 'reasoning' | 'text' = 'status';
    let ri = 0;
    let ti = 0;
    let ticks = 0;
    const timer = window.setInterval(() => {
      ticks++;
      if (phase === 'status') {
        deliver({ type: 'delta', id, status: 'Connecting…' });
        if (ticks > 8) phase = withReasoning ? 'reasoning' : 'text';
        return;
      }
      if (phase === 'reasoning') {
        const chunk = REASONING.join('').slice(ri, ri + 6);
        ri += 6;
        if (chunk) deliver({ type: 'delta', id, reasoning: chunk });
        if (ri >= REASONING.join('').length) phase = 'text';
        return;
      }
      const answer = params.has('math') ? '# Formula examples\n\nInline $x^2 + y^2 = z^2$.\n\n$$\\frac{1}{2} + \\sqrt{4} = 2.5$$\n\n| Item | Value |\n|---|---|\n| Total | 42 |\n| Formula | $E=mc^2$ |' : ANSWER;
      const n = params.has('fast') ? 1000 : 3 + Math.floor(Math.random() * 6);
      const chunk = answer.slice(ti, ti + n);
      ti += n;
      if (chunk) deliver({ type: 'delta', id, text: chunk });
      if (ti >= answer.length) {
        clearInterval(timer);
        stream = null;
        deliver({
          type: 'done',
          id,
          ok: true,
          cancelled: false,
          error: '',
          stopReason: 'end_turn',
          usage: { input_tokens: 1832, output_tokens: 412 },
        });
      }
    }, 24);
    stream = { id, timer };
  };

  const handle = (m: UiMessage) => {
    switch (m.type) {
      case 'ready':
        paintDesktop();
        emit({ type: 'init', page, version: '0.1.0-dev', theme: theme(), payload: structuredClone(payload), liveBackdrop: true });
        for (const p of ['anthropic', 'claude-cli']) {
          emit({ type: 'models', provider: p, models: MODELS[p] ?? [], error: '' }, 120);
        }
        if (page === 'bar' && !params.has('hidden')) setTimeout(show, 180);
        break;
      case 'hidden':
        visible = false;
        if (stream) {
          clearInterval(stream.timer);
          stream = null;
        }
        break;
      case 'dismiss':
        if (visible) emit({ type: 'dismiss' });
        break;
      case 'ask': {
        if (m.screen && (blockReason() || m.screenId !== currentScreen?.id || reviewedScreenId !== currentScreen?.id)) {
          emit({ type: 'done', id: m.id, ok: false, cancelled: false, error: blockReason() || 'Review the current screenshot before sending.' });
          break;
        }
        const budgetError = reserveApiAllowance(m.provider);
        if (budgetError) { emit({ type: 'done', id: m.id, ok: false, cancelled: false, error: budgetError }); break; }
        if (/error/i.test(m.text)) {
          emit({ type: 'done', id: m.id, ok: false, cancelled: false, error: 'Anthropic API error 529: Overloaded. Try again in a moment.' }, 600);
          break;
        }
        const model = MODELS[m.provider]?.find((x) => x.id === m.model);
        startStream(m.id, !params.has('fast') && !!m.reasoning && m.reasoning !== 'none' && model?.reasoning.mode !== 'none');
        if (params.has('limit-warning')) emit({ type: 'delta', id: m.id, limitWarning: 'Claude reports that account usage is nearing a limit. Check /usage in Claude Code.' }, 80);
        break;
      }
      case 'cancel':
        if (stream?.id === m.id) {
          clearInterval(stream.timer);
          stream = null;
          emit({ type: 'done', id: m.id, ok: false, cancelled: true, error: '' });
        }
        break;
      case 'newChat':
        if (stream) {
          clearInterval(stream.timer);
          stream = null;
        }
        break;
      case 'screenPreviewReady':
        if (m.screenId === currentScreen?.id) reviewedScreenId = m.screenId;
        break;
      case 'setScreenMode':
        screenMode = m.mode;
        emit({ type: 'screen', screen: screenPayload() });
        break;
      case 'listModels': {
        const p = payload.providers.find((x) => x.id === m.provider);
        const list = MODELS[m.provider] ?? (p?.defaultModel ? [{ id: p.defaultModel, name: p.defaultModel, vision: true, reasoning: none }] : []);
        emit({ type: 'models', provider: m.provider, models: list, error: '' }, m.refresh ? 700 : 250);
        break;
      }
      case 'openSettings':
        window.open('./settings.html', '_blank');
        break;
      case 'saveSettings':
        if ('providers' in m.patch) setupPassed = null;
        payload.settings = mergePatch(payload.settings, m.patch) as SettingsPayload['settings'];
        if ('appearance' in m.patch) {
          paintDesktop();
          emit({ type: 'theme', theme: theme() });
        }
        settingsChanged();
        if (visible && ('screenCaptureProtection' in m.patch || 'screenCaptureBlocklist' in m.patch || 'screenMode' in m.patch)) {
          screenMode = payload.settings.screenMode;
          emit({ type: 'screen', screen: screenPayload() });
          if (blockReason()) emit({ type: 'backdrop', url: '' });
        }
        break;
      case 'setApiKey': {
        setupPassed = null;
        const p = payload.providers.find((x) => x.id === m.provider);
        if (p) p.hasKey = true;
        settingsChanged();
        emit({ type: 'testResult', provider: m.provider, ok: true, message: `Connected · ${(MODELS[m.provider] ?? []).length || 12} models` }, 800);
        emit({ type: 'models', provider: m.provider, models: MODELS[m.provider] ?? [], error: '' }, 800);
        break;
      }
      case 'removeApiKey': {
        setupPassed = null;
        const p = payload.providers.find((x) => x.id === m.provider);
        if (p) p.hasKey = false;
        settingsChanged();
        break;
      }
      case 'testProvider': {
        const p = payload.providers.find((x) => x.id === m.provider);
        const ok = !!p && (p.kind !== 'cli' || !!p.cli?.signedIn) && (p.kind !== 'local' || p.id === 'ollama');
        emit({ type: 'testResult', provider: m.provider, ok, message: ok ? 'Connected · 3 models' : p?.kind === 'local' ? 'Couldn\'t connect to http://localhost:1234/v1' : 'Not signed in' }, 900);
        break;
      }
      case 'detectCli': {
        const p = payload.providers.find((x) => x.id === m.provider);
        const cli = p?.cli ?? { installed: false, signedIn: false, path: '', detail: 'Not found on PATH' };
        emit({ type: 'cliStatus', provider: m.provider, ...cli }, 500);
        break;
      }
      case 'discoverProviders':
        payload.discovery.running = true;
        settingsChanged();
        setTimeout(() => { payload.discovery.running = false; payload.discovery.complete = true; settingsChanged(); }, 500);
        break;
      case 'testSetup': {
        const budgetError = reserveApiAllowance(m.provider);
        if (budgetError) { emit({ ...m, type: 'setupTestResult', ok: false, answer: '', error: budgetError }); break; }
        if (setupTimer) clearTimeout(setupTimer);
        setupId = m.id;
        setupProvider = m.provider;
        setupModelId = m.model;
        setupPassed = null;
        setupTimer = setTimeout(() => {
          setupTimer = null;
          const ok = !params.has('setup-error');
          if (ok) setupPassed = { id: m.id, provider: m.provider, model: m.model };
          const p = payload.providers.find(p => p.id === m.provider);
          if (p) p.connection = { state: ok ? 'connected' : 'not-connected', detail: ok ? 'Answered the test question' : 'Test failed', nextStep: ok ? 'Ready to ask.' : 'Check setup and try again.' };
          emit({ ...m, type: 'setupTestResult', ok, answer: ok ? 'Hello! Your assistant is ready to help.' : '', error: ok ? '' : 'The provider could not answer. Check your sign-in and try again.' });
          settingsChanged();
        }, params.has('slow-test') ? 5000 : 600);
        break;
      }
      case 'cancelSetupTest':
        if (setupTimer) clearTimeout(setupTimer);
        setupTimer = null;
        setupPassed = null;
        emit({ type: 'setupTestResult', id: setupId, provider: setupProvider, model: setupModelId, ok: false, answer: '', error: 'Test stopped. You can try again.' });
        break;
      case 'completeSetup':
        if (!setupPassed || setupPassed.id !== m.id || payload.hotkeyError) emit({ type: 'setupComplete', ok: false, error: 'Confirm a test question and an available shortcut first.' });
        else {
          payload.settings.setupCompleted = true;
          payload.settings.defaultModel = { provider: setupPassed.provider, model: setupPassed.model, reasoning: '' };
          payload.settings.providers[setupPassed.provider] = { ...payload.settings.providers[setupPassed.provider], enabled: true };
          settingsChanged();
          emit({ type: 'setupComplete', ok: true, error: '' });
        }
        break;
      case 'setHotkey': {
        const taken = /^Win\+L$|^Ctrl\+Alt\+Delete$/i.test(m.hotkey);
        if (!taken) payload.settings.hotkey = m.hotkey;
        if (!taken) payload.hotkeyError = '';
        emit({ type: 'hotkeyResult', ok: !taken, hotkey: payload.settings.hotkey, error: taken ? `${m.hotkey} is already used by another app` : '' }, 120);
        settingsChanged();
        break;
      }
      case 'setAutostart':
        payload.autostart = m.enabled;
        settingsChanged();
        break;
      case 'copy': {
        const write = m.html && typeof ClipboardItem !== 'undefined' ? navigator.clipboard.write([
          new ClipboardItem({ 'text/plain': new Blob([m.text], { type: 'text/plain' }), 'text/html': new Blob([m.html], { type: 'text/html' }) }),
        ]) : navigator.clipboard.writeText(m.text);
        void write.then(() => emit({ type: 'copyResult', id: m.id ?? '', ok: true }), () => emit({ type: 'copyResult', id: m.id ?? '', ok: false }));
        break;
      }
      case 'openUrl':
        window.open(m.url, '_blank', 'noopener');
        break;
      default:
        console.debug('[mock host]', m);
    }
  };

  return {
    // Svelte settings patches may contain reactive proxies. Match WebView2's JSON
    // wire format rather than structuredClone, which rejects Proxy objects.
    post: (m) => handle(JSON.parse(JSON.stringify(m)) as UiMessage),
    start: (d) => (deliver = d),
  };
}
