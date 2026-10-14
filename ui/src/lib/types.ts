// Shapes exchanged with the native host (src/app/App.cpp). Keep in sync.

export interface ThemeInfo {
  dark: boolean;
  accent: string; // "#rrggbb"
  reducedMotion: boolean;
}

export interface ReasoningSpec {
  mode: 'none' | 'effort' | 'budget' | string;
  levels: string[];
  default: string;
}

export interface ModelInfo {
  id: string;
  name: string;
  vision: boolean;
  reasoning: ReasoningSpec;
}

export interface CliInfo {
  installed: boolean;
  signedIn: boolean;
  path: string;
  detail: string;
}

export type ProviderKind = 'api' | 'local' | 'cli';

export interface ConnectionInfo {
  state: 'checking' | 'unchecked' | 'ready' | 'connected' | 'not-connected';
  detail: string;
  nextStep: string;
}

export interface DiscoveryInfo {
  running: boolean;
  complete: boolean;
  nodeInstalled: boolean;
  npmInstalled: boolean;
  suggestedProvider: string;
}

export interface ProviderInfo {
  id: string;
  name: string;
  kind: ProviderKind;
  description: string;
  defaultBaseUrl: string;
  needsKey: boolean;
  keyOptional: boolean;
  baseUrlEditable: boolean;
  keyUrl: string;
  defaultModel: string;
  hasKey: boolean;
  cli?: CliInfo;
  connection?: ConnectionInfo;
}

export interface ProviderConfig {
  enabled?: boolean;
  baseUrl?: string;
  customModels?: string[];
  reasoningOverrides?: Record<string, boolean>;
}

export interface ModelChoice {
  provider: string;
  model: string;
  reasoning: string;
}

export type Appearance = 'system' | 'light' | 'dark';
export type BackdropMode = 'liquid' | 'system';
export type ScreenMode = 'monitor' | 'window';
export interface CaptureRule {
  process: string;
  title: string;
  label: string;
}
export type BarPosition =
  | 'top-left'
  | 'top-center'
  | 'top-right'
  | 'middle-left'
  | 'middle-center'
  | 'middle-right'
  | 'bottom-left'
  | 'bottom-center'
  | 'bottom-right';

export interface SettingsData {
  setupCompleted: boolean;
  hotkey: string;
  appearance: Appearance;
  backdrop: BackdropMode;
  liveBackdrop: boolean;
  solidSurfaces: boolean;
  textScale: number;
  barPinned: boolean;
  apiDailyBudgetUsd: number;
  apiRequestAllowanceUsd: number;
  screenContextDefault: boolean;
  screenMode: ScreenMode;
  screenCaptureProtection: boolean;
  screenCaptureBlocklist: CaptureRule[];
  barPosition: BarPosition;
  conversationTimeoutMinutes: number;
  systemPrompt: string;
  defaultModel: ModelChoice;
  providers: Record<string, ProviderConfig>;
  [key: string]: unknown;
}

export interface SettingsPayload {
  settings: SettingsData;
  providers: ProviderInfo[];
  autostart: boolean;
  hotkeyError: string;
  hotkeySuggestions: { hotkey: string; available: boolean }[];
  discovery: DiscoveryInfo;
  defaultSystemPrompt: string;
  defaultCaptureBlocklist: CaptureRule[];
  budget: { day: string; reservedUsd: number; error: string };
}

export interface ScreenInfo {
  available: boolean;
  title: string;
  app: string;
  hasWindow: boolean;
  mode: ScreenMode;
  thumb: string;
  id: number;
  monitor: number;
  width: number;
  height: number;
  status: 'captured' | 'blocked';
  reason: string;
}

export interface Usage {
  input_tokens?: number;
  output_tokens?: number;
  [key: string]: unknown;
}

// ---- Host -> UI ---------------------------------------------------------------

export type HostMessage =
  | { type: 'init'; page: string; version: string; theme: ThemeInfo; payload: SettingsPayload; liveBackdrop: boolean }
  | { type: 'theme'; theme: ThemeInfo }
  | {
      type: 'show';
      backdrop: string;
      screen: ScreenInfo;
      screenDefault: boolean;
      conversationReset: boolean;
      position: BarPosition;
      scale: number;
    }
  | { type: 'backdrop'; url: string }
  | { type: 'screen'; screen: ScreenInfo }
  | { type: 'dismiss' }
  | { type: 'copyResult'; id: string; ok: boolean }
  | { type: 'delta'; id: string; text?: string; reasoning?: string; status?: string; limitWarning?: string }
  | {
      type: 'done';
      id: string;
      ok: boolean;
      cancelled: boolean;
      error: string;
      stopReason?: string;
      usage?: Usage | null;
      limitWarning?: string;
      /** False when the provider delivers whole messages, so per-token timing is unavailable. */
      streamed?: boolean;
    }
  | { type: 'models'; provider: string; models: ModelInfo[]; error: string }
  | { type: 'settings'; payload: SettingsPayload }
  | { type: 'hotkeyResult'; ok: boolean; hotkey: string; error: string }
  | { type: 'testResult'; provider: string; ok: boolean; message: string }
  | { type: 'setupTestResult'; id: string; provider: string; model: string; ok: boolean; answer: string; error: string }
  | { type: 'setupComplete'; ok: boolean; error: string }
  | ({ type: 'cliStatus'; provider: string } & CliInfo);

// ---- UI -> host ---------------------------------------------------------------

export type UiMessage =
  | { type: 'ready' }
  | { type: 'hidden'; restoreFocus: boolean }
  | { type: 'dismiss' }
  | { type: 'setHitRegion'; x: number; y: number; width: number; height: number; radius: number }
  | { type: 'ask'; id: string; text: string; provider: string; model: string; reasoning: string; screen: boolean; screenId?: number }
  | { type: 'screenPreviewReady'; screenId: number }
  | { type: 'prepare'; provider: string; model: string; reasoning: string; screen: boolean }
  | { type: 'cancel'; id: string }
  | { type: 'newChat' }
  | { type: 'setScreenMode'; mode: ScreenMode }
  | { type: 'listModels'; provider: string; refresh: boolean }
  | { type: 'openSettings' }
  | { type: 'saveSettings'; patch: Record<string, unknown> }
  | { type: 'setApiKey'; provider: string; key: string }
  | { type: 'removeApiKey'; provider: string }
  | { type: 'testProvider'; provider: string }
  | { type: 'detectCli'; provider: string }
  | { type: 'discoverProviders' }
  | { type: 'testSetup'; id: string; provider: string; model: string }
  | { type: 'cancelSetupTest' }
  | { type: 'completeSetup'; id: string }
  | { type: 'setHotkey'; hotkey: string }
  | { type: 'suspendHotkey' }
  | { type: 'resumeHotkey' }
  | { type: 'setAutostart'; enabled: boolean }
  | { type: 'copy'; id?: string; text: string; html?: string }
  | { type: 'openUrl'; url: string }
  | { type: 'openDataFolder' }
  | { type: 'quit' };

export type HostMessageOf<T extends HostMessage['type']> = Extract<HostMessage, { type: T }>;
