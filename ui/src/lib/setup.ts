import type { ConnectionInfo, ModelInfo, ProviderInfo } from './types.ts';

export function connectionFor(p: ProviderInfo): ConnectionInfo {
  if (p.connection) return p.connection;
  if (p.kind === 'cli') {
    if (!p.cli) return { state: 'checking', detail: 'Looking for an existing sign-in…', nextStep: '' };
    return { state: p.cli.signedIn ? 'ready' : 'not-connected', detail: p.cli.detail,
      nextStep: !p.cli.installed ? 'Install the provider’s app.' : !p.cli.signedIn ? 'Sign in, then check again.' : 'Send a test question.' };
  }
  return { state: p.hasKey ? 'ready' : 'not-connected', detail: '',
    nextStep: p.hasKey ? 'Test the connection.' : p.kind === 'local' ? 'Start the server and load a model.' : 'Add an API key.' };
}

export function connectionLabel(state: ConnectionInfo['state']): string {
  return { checking: 'Checking…', unchecked: 'Not checked', ready: 'Ready to test', connected: 'Connected', 'not-connected': 'Not connected' }[state];
}

export function setupModel(provider: ProviderInfo, models: ModelInfo[]): string {
  const preferred = provider.id === 'claude-cli' ? 'haiku' : provider.id === 'codex-cli' ? 'gpt-6-luna' : provider.defaultModel;
  return models.find(m => m.id === preferred)?.id ?? models[0]?.id ?? '';
}
