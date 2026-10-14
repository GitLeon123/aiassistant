import type { ProviderConfig, ProviderInfo } from './types';

/** Classify the actual configured endpoint, not the provider's marketing category. */
export function destination(provider: ProviderInfo | undefined, config: ProviderConfig) {
  if (!provider) return { scope: 'Unknown', host: '' };
  if (provider.kind === 'cli') return { scope: 'Cloud', host: '' };
  const base = (provider.baseUrlEditable && config.baseUrl) || provider.defaultBaseUrl;
  try {
    const url = new URL(base);
    if (url.protocol !== 'http:' && url.protocol !== 'https:') return { scope: 'Unknown', host: '' };
    const host = url.hostname.toLowerCase();
    const local = host === 'localhost' || host === '[::1]' || /^127\.\d+\.\d+\.\d+$/.test(host);
    return { scope: local ? 'Local' : 'Cloud', host: url.host };
  } catch {
    return { scope: 'Unknown', host: '' };
  }
}
