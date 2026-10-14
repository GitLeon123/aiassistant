// Reactive app state shared by the bar and the settings window, fed by host messages.

import { on, send } from './bridge';
import type {
  ModelChoice,
  ModelInfo,
  ProviderConfig,
  ProviderInfo,
  SettingsData,
  SettingsPayload,
  ThemeInfo,
} from './types';

export interface ModelList {
  list: ModelInfo[];
  error: string;
  loading: boolean;
}

class AppState {
  ready = $state(false);
  version = $state('');
  liveBackdrop = $state(true);
  theme = $state<ThemeInfo>({ dark: true, accent: '#4c8dff', reducedMotion: false });
  payload = $state<SettingsPayload | null>(null);
  models = $state<Record<string, ModelList>>({});

  get settings(): SettingsData | null {
    return this.payload?.settings ?? null;
  }

  get providers(): ProviderInfo[] {
    return this.payload?.providers ?? [];
  }

  get dark(): boolean {
    const appearance = this.settings?.appearance ?? 'system';
    return appearance === 'system' ? this.theme.dark : appearance === 'dark';
  }

  get reducedMotion(): boolean {
    return this.theme.reducedMotion;
  }

  provider(id: string): ProviderInfo | undefined {
    return this.providers.find((p) => p.id === id);
  }

  config(id: string): ProviderConfig {
    return this.settings?.providers?.[id] ?? {};
  }

  /** Mirrors App::SendInit: CLI/local providers are opted in; API providers are on once they have a key. */
  isEnabled(p: ProviderInfo): boolean {
    return !!this.config(p.id).enabled || ((p.needsKey || p.keyOptional) && p.hasKey);
  }

  get enabledProviders(): ProviderInfo[] {
    return this.providers.filter((p) => this.isEnabled(p));
  }

  modelsFor(provider: string): ModelList {
    return this.models[provider] ?? { list: [], error: '', loading: false };
  }

  requestModels(provider: string, refresh = false) {
    const current = this.models[provider];
    if (current?.loading) return;
    if (current && current.list.length && !refresh) return;
    this.models[provider] = { list: current?.list ?? [], error: '', loading: true };
    send({ type: 'listModels', provider, refresh });
  }

  findModel(provider: string, model: string): ModelInfo | undefined {
    return this.models[provider]?.list.find((m) => m.id === model);
  }

  /** The configured default, falling back to the first enabled provider's default model. */
  get defaultChoice(): ModelChoice | null {
    const dm = this.settings?.defaultModel;
    if (dm?.provider && dm.model && this.provider(dm.provider)) return dm;
    const first = this.enabledProviders[0];
    if (!first) return null;
    const model = first.defaultModel || this.models[first.id]?.list[0]?.id || '';
    return model ? { provider: first.id, model, reasoning: '' } : null;
  }

  saveSettings(patch: Record<string, unknown>) {
    // Optimistic local merge so controls respond instantly; the host echoes the authoritative copy.
    if (this.payload) {
      this.payload.settings = mergePatch(this.payload.settings, patch) as SettingsData;
    }
    send({ type: 'saveSettings', patch });
  }

  saveProviderConfig(id: string, patch: ProviderConfig) {
    this.saveSettings({ providers: { [id]: patch } });
  }
}

export function mergePatch(target: unknown, patch: unknown): unknown {
  if (!isObject(patch)) return patch;
  const out: Record<string, unknown> = isObject(target) ? { ...target } : {};
  for (const [k, v] of Object.entries(patch)) {
    if (v === null) delete out[k];
    else out[k] = mergePatch(out[k], v);
  }
  return out;
}

function isObject(v: unknown): v is Record<string, unknown> {
  return typeof v === 'object' && v !== null && !Array.isArray(v);
}

export const app = new AppState();

on('init', (m) => {
  app.version = m.version;
  app.theme = m.theme;
  app.payload = m.payload;
  app.liveBackdrop = m.liveBackdrop;
  app.ready = true;
});
on('theme', (m) => {
  app.theme = m.theme;
});
on('settings', (m) => {
  app.payload = m.payload;
});
on('models', (m) => {
  app.models[m.provider] = { list: m.models, error: m.error, loading: false };
});
on('cliStatus', (m) => {
  const p = app.payload?.providers.find((x) => x.id === m.provider);
  if (p) p.cli = { installed: m.installed, signedIn: m.signedIn, path: m.path, detail: m.detail };
});

/** Picks the reasoning level to use for a model given a preferred one. */
export function levelFor(model: ModelInfo | undefined, preferred: string): string {
  if (!model || model.reasoning.mode === 'none' || model.reasoning.levels.length === 0) return '';
  if (preferred && model.reasoning.levels.includes(preferred)) return preferred;
  return model.reasoning.default || model.reasoning.levels[0];
}

const LEVEL_LABELS: Record<string, string> = {
  none: 'Off',
  minimal: 'Minimal',
  low: 'Low',
  medium: 'Medium',
  high: 'High',
  xhigh: 'Extra high',
  max: 'Max',
};

export function levelLabel(level: string): string {
  return LEVEL_LABELS[level] ?? level.charAt(0).toUpperCase() + level.slice(1);
}

/** Applies theme + accent + motion preferences to the document root. */
export function applyTheme(dark: boolean, theme: ThemeInfo) {
  const root = document.documentElement;
  root.dataset.theme = dark ? 'dark' : 'light';
  root.dataset.motion = theme.reducedMotion ? 'reduced' : 'full';
  root.style.colorScheme = dark ? 'dark' : 'light';
  const accent = app.settings?.solidSurfaces ? (dark ? '#8db6ff' : '#174ea6') : /^#[0-9a-f]{6}$/i.test(theme.accent) ? theme.accent : '#4c8dff';
  root.style.setProperty('--accent', accent);
  root.style.setProperty('--accent-fg', luminance(accent) > 0.179 ? '#111' : '#fff');
  root.dataset.solid = app.settings?.solidSurfaces ? 'true' : 'false';
  root.style.setProperty('--text-scale', String((app.settings?.textScale ?? 100) / 100));
}

function luminance(hex: string): number {
  const n = parseInt(hex.slice(1), 16);
  const c = [(n >> 16) & 255, (n >> 8) & 255, n & 255].map((v) => {
    const s = v / 255;
    return s <= 0.03928 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4;
  });
  return 0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2];
}
