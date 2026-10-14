import assert from 'node:assert/strict';
import { test } from 'node:test';
import { destination } from '../src/lib/privacy.ts';
import type { ProviderInfo } from '../src/lib/types.ts';

const provider = (overrides: Partial<ProviderInfo> = {}): ProviderInfo => ({
  id: 'ollama', name: 'Ollama', kind: 'local', description: '', defaultBaseUrl: 'http://localhost:11434/v1',
  needsKey: false, keyOptional: true, baseUrlEditable: true, keyUrl: '', defaultModel: '', hasKey: false, ...overrides,
});

test('Destination only says Local for the actual loopback endpoint', () => {
  for (const baseUrl of ['http://localhost:11434/v1', 'http://127.0.0.1/v1', 'http://127.2.3.4/v1', 'http://[::1]:1234/v1']) {
    assert.equal(destination(provider(), { baseUrl }).scope, 'Local');
  }
  for (const baseUrl of ['https://ollama.example/v1', 'http://192.168.1.10:11434/v1', 'http://localhost.evil.example/v1', 'http://localhost@remote.example/v1']) {
    assert.equal(destination(provider(), { baseUrl }).scope, 'Cloud');
  }
  assert.equal(destination(provider(), {}).scope, 'Local');
  assert.equal(destination(provider({ id: 'custom', kind: 'api' }), { baseUrl: 'http://127.0.0.1:8080/v1' }).scope, 'Local');
});

test('CLI and fixed API providers remain Cloud; invalid destinations stay Unknown', () => {
  assert.equal(destination(provider({ kind: 'cli' }), {}).scope, 'Cloud');
  const api = provider({ kind: 'api', baseUrlEditable: false, defaultBaseUrl: 'https://api.example/v1' });
  assert.equal(destination(api, { baseUrl: 'http://localhost' }).scope, 'Cloud');
  for (const baseUrl of ['', 'file:///secret', 'javascript:alert(1)', 'bad url']) {
    assert.equal(destination(provider({ defaultBaseUrl: '' }), { baseUrl }).scope, 'Unknown');
  }
  assert.equal(destination(undefined, {}).scope, 'Unknown');
  assert.equal(destination(provider(), { baseUrl: 'https://user:secret@remote.example/v1?key=secret' }).host, 'remote.example');
});
