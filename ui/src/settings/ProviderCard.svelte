<script lang="ts">
  import { onDestroy } from 'svelte';
  import { on, send } from '../lib/bridge';
  import Button from '../lib/controls/Button.svelte';
  import Toggle from '../lib/controls/Toggle.svelte';
  import Icon from '../lib/Icon.svelte';
  import { app } from '../lib/state.svelte';
  import type { ProviderInfo } from '../lib/types';
  import { connectionFor, connectionLabel } from '../lib/setup';

  interface Props {
    provider: ProviderInfo;
    expanded?: boolean;
  }
  let { provider, expanded = $bindable(false) }: Props = $props();

  const CLI_HELP: Record<string, { install: string; login: string; url: string }> = {
    'claude-cli': {
      install: 'npm install -g @anthropic-ai/claude-code',
      login: 'Run claude in a terminal and sign in with your Claude account',
      url: 'https://docs.claude.com/en/docs/claude-code/setup',
    },
    'codex-cli': {
      install: 'npm install -g @openai/codex',
      login: 'Run codex login and sign in with ChatGPT',
      url: 'https://developers.openai.com/codex/cli',
    },
    'gemini-cli': {
      install: 'npm install -g @google/gemini-cli',
      login: 'Run gemini in a terminal and choose "Login with Google"',
      url: 'https://github.com/google-gemini/gemini-cli',
    },
  };

  let keyInput = $state('');
  let replacing = $state(false);
  let testing = $state(false);
  let test = $state<{ ok: boolean; message: string } | null>(null);
  let checking = $state(false);
  let newModel = $state('');

  const config = $derived(app.config(provider.id));
  const enabled = $derived(app.isEnabled(provider));
  const help = $derived(CLI_HELP[provider.id]);
  const customModels = $derived(config.customModels ?? []);
  let baseUrl = $state('');
  $effect(() => {
    baseUrl = config.baseUrl ?? '';
  });

  const status = $derived.by((): { label: string; tone: 'ok' | 'warn' | 'muted' } | null => {
    if (provider.connection) {
      const state = connectionFor(provider).state;
      return { label: connectionLabel(state), tone: state === 'connected' ? 'ok' : state === 'not-connected' ? 'warn' : 'muted' };
    }
    if (provider.kind === 'cli') {
      const cli = provider.cli;
      if (!cli) return { label: 'Checking…', tone: 'muted' };
      if (!cli.installed) return { label: 'Not installed', tone: 'muted' };
      if (!cli.signedIn) return { label: 'Not signed in', tone: 'warn' };
      return { label: 'Signed in', tone: 'ok' };
    }
    if (provider.hasKey) return { label: 'Connected', tone: 'ok' };
    if (provider.kind === 'local' && enabled) return { label: 'On', tone: 'ok' };
    return null;
  });

  const MONO_COLORS = ['#d97757', '#10a37f', '#4285f4', '#8b5cf6', '#ef4444', '#f59e0b', '#06b6d4', '#ec4899'];
  const monoColor = $derived(
    MONO_COLORS[[...provider.id].reduce((a, ch) => (a * 31 + ch.charCodeAt(0)) >>> 0, 7) % MONO_COLORS.length],
  );

  const offTest = on('testResult', (m) => {
    if (m.provider !== provider.id) return;
    testing = false;
    test = { ok: m.ok, message: m.message };
  });
  const offCli = on('cliStatus', (m) => {
    if (m.provider === provider.id) checking = false;
  });
  onDestroy(() => {
    offTest();
    offCli();
  });

  function recheck() {
    checking = true;
    send({ type: 'detectCli', provider: provider.id });
  }

  function runTest() {
    testing = true;
    test = null;
    send({ type: 'testProvider', provider: provider.id });
  }

  function saveKey() {
    const key = keyInput.trim();
    if (!key) return;
    send({ type: 'setApiKey', provider: provider.id, key });
    keyInput = '';
    replacing = false;
    testing = true; // the host tests the key right after saving it
    test = null;
  }

  function removeKey() {
    send({ type: 'removeApiKey', provider: provider.id });
    test = null;
  }

  function setEnabled(on: boolean) {
    app.saveProviderConfig(provider.id, { enabled: on });
    if (on) {
      app.requestModels(provider.id, true);
      if (!expanded && provider.kind === 'cli' && !provider.cli?.signedIn) expanded = true;
    }
  }

  function saveBaseUrl() {
    const value = baseUrl.trim().replace(/\/+$/, '');
    if (value === (config.baseUrl ?? '')) return;
    app.saveProviderConfig(provider.id, { baseUrl: value }); // empty = provider default
  }

  function addModel() {
    const id = newModel.trim();
    if (!id || customModels.includes(id)) return;
    app.saveProviderConfig(provider.id, { customModels: [...customModels, id] });
    newModel = '';
  }

  function removeModel(id: string) {
    app.saveProviderConfig(provider.id, { customModels: customModels.filter((m) => m !== id) });
  }

  const showToggle = $derived(provider.kind === 'cli' || provider.kind === 'local' || (provider.keyOptional && !provider.needsKey));
</script>

<div class="card" class:expanded class:enabled>
  <div class="head">
    <button type="button" class="head-main" onclick={() => (expanded = !expanded)} aria-expanded={expanded}>
      <span class="mono" style="--mono:{monoColor}">{provider.name.charAt(0)}</span>
      <span class="text">
        <span class="name">
          {provider.name}
          {#if status}<span class="badge {status.tone}">{status.label}</span>{/if}
        </span>
        <span class="desc">{provider.description}</span>
      </span>
      <span class="chev"><Icon name="chevronDown" size={14} stroke={2} /></span>
    </button>
    {#if showToggle}
      <Toggle
        checked={!!config.enabled}
        label="Use {provider.name}"
        disabled={provider.kind === 'cli' && provider.cli !== undefined && !provider.cli.installed}
        onchange={setEnabled}
      />
    {/if}
  </div>

  {#if expanded}
    <div class="body">
      {#if provider.kind === 'cli'}
        {#if provider.cli?.installed}
          <p class="detail">{provider.cli.detail || (provider.cli.signedIn ? 'Ready' : 'Not signed in')}</p>
          <p class="path" title={provider.cli.path}>{provider.cli.path}</p>
        {/if}
        {#if help && !provider.cli?.installed && app.payload?.discovery.complete && (!app.payload.discovery.nodeInstalled || !app.payload.discovery.npmInstalled)}
          <p class="runtime-help">For the install command below, you’ll need Node.js and its included installer (npm). They weren’t both found on this PC. Download the LTS version, then reopen AIAssistant. The setup guide may offer another installation option. You can also use an API key or a local model.</p>
          <Button size="sm" onclick={() => send({ type: 'openUrl', url: 'https://nodejs.org/en/download' })}>Download Node.js <Icon name="external" size={12} /></Button>
        {/if}
        {#if help && (!provider.cli?.installed || !provider.cli?.signedIn)}
          <ol class="steps">
            {#if !provider.cli?.installed}
              <li>Install: <code>{help.install}</code></li>
            {/if}
            <li>{help.login}</li>
            <li>Come back here and press <b>Check again</b></li>
          </ol>
        {/if}
        <p class="note">
          <Icon name="shield" size={13} /> AIAssistant never sees your subscription credentials. It runs the official
          CLI with tools disabled, in an empty temporary folder.
        </p>
        <div class="actions">
          <Button size="sm" busy={checking} onclick={recheck}>Check again</Button>
          {#if provider.cli?.installed && provider.cli?.signedIn}
            <Button size="sm" variant="ghost" busy={testing} onclick={runTest}>Test</Button>
          {/if}
          {#if help}
            <Button size="sm" variant="ghost" onclick={() => send({ type: 'openUrl', url: help.url })}>
              Setup guide <Icon name="external" size={12} />
            </Button>
          {/if}
        </div>
      {/if}

      {#if provider.needsKey || provider.keyOptional}
        <div class="field">
          <span class="label">API key{provider.keyOptional ? ' (optional)' : ''}</span>
          {#if provider.hasKey && !replacing}
            <div class="saved">
              <span class="dots">••••••••••••••••</span>
              <span class="where"><Icon name="key" size={12} /> Stored in Windows Credential Manager</span>
              <span class="grow"></span>
              <Button size="sm" variant="ghost" onclick={() => (replacing = true)}>Replace</Button>
              <Button size="sm" variant="danger" onclick={removeKey}>Remove</Button>
            </div>
          {:else}
            <form
              class="key-form"
              onsubmit={(e) => {
                e.preventDefault();
                saveKey();
              }}
            >
              <input
                type="password"
                bind:value={keyInput}
                placeholder="Paste your {provider.name} API key"
                aria-label="{provider.name} API key"
                autocomplete="off"
                spellcheck="false"
              />
              <Button type="submit" variant="primary" size="sm" disabled={!keyInput.trim()}>Save</Button>
              {#if replacing}
                <Button size="sm" variant="ghost" onclick={() => (replacing = false)}>Cancel</Button>
              {/if}
            </form>
            {#if provider.keyUrl}
              <button type="button" class="link" onclick={() => send({ type: 'openUrl', url: provider.keyUrl })}>
                Get a key <Icon name="external" size={11} />
              </button>
            {/if}
          {/if}
        </div>
      {/if}

      {#if provider.baseUrlEditable}
        <div class="field">
          <span class="label">Base URL</span>
          <input
            type="url"
            aria-label="{provider.name} base URL"
            bind:value={baseUrl}
            placeholder={provider.defaultBaseUrl || 'https://example.com/v1'}
            spellcheck="false"
            onblur={saveBaseUrl}
            onkeydown={(e) => e.key === 'Enter' && saveBaseUrl()}
          />
        </div>
      {/if}

      {#if provider.kind !== 'cli' || provider.id !== 'gemini-cli'}
        <div class="field">
          <span class="label">Extra model IDs</span>
          <div class="tags">
            {#each customModels as id (id)}
              <span class="tag">{id}<button type="button" aria-label="Remove {id}" onclick={() => removeModel(id)}><Icon name="x" size={11} stroke={2} /></button></span>
            {/each}
            <input
              class="tag-input"
              bind:value={newModel}
              placeholder={customModels.length ? 'Add…' : 'e.g. a new model ID, press Enter'}
              spellcheck="false"
              onkeydown={(e) => {
                if (e.key === 'Enter') {
                  e.preventDefault();
                  addModel();
                }
              }}
            />
          </div>
        </div>
      {/if}

      {#if provider.kind !== 'cli'}
        <div class="actions">
          <Button size="sm" busy={testing} disabled={provider.needsKey && !provider.keyOptional && !provider.hasKey} onclick={runTest}>
            Test connection
          </Button>
        </div>
      {/if}

      {#if test}
        <p class="test {test.ok ? 'ok' : 'err'}">
          <Icon name={test.ok ? 'check' : 'warning'} size={13} stroke={2} />
          {test.message}
        </p>
      {/if}
      {#if provider.connection?.nextStep}<p class="detail">Next step: {provider.connection.nextStep}</p>{/if}
    </div>
  {/if}
</div>

<style>
  .runtime-help { margin: 0; color: var(--fg-2); font-size: 0.92857rem; }
  .card {
    border-radius: 12px;
    background: var(--surface);
    box-shadow:
      inset 0 0 0 0.5px var(--line-strong),
      0 1px 2px rgba(0, 0, 0, 0.05);
    transition: background 200ms;
  }
  .head {
    display: flex;
    align-items: center;
    gap: 10px;
    padding-right: 16px;
  }
  .head-main {
    flex: 1;
    min-width: 0;
    display: flex;
    align-items: center;
    gap: 12px;
    padding: 12px 8px 12px 14px;
    border: 0;
    background: none;
    text-align: left;
    border-radius: 12px;
  }
  .mono {
    flex: none;
    display: grid;
    place-items: center;
    width: 34px;
    height: 34px;
    border-radius: 10px;
    background: linear-gradient(160deg, color-mix(in srgb, var(--mono) 88%, white), var(--mono));
    color: #fff;
    font-weight: 700;
    font-size: 1.0714rem;
    box-shadow:
      inset 0 1px 0 rgba(255, 255, 255, 0.35),
      0 2px 6px -2px color-mix(in srgb, var(--mono) 60%, transparent);
  }
  .text {
    flex: 1;
    min-width: 0;
    display: flex;
    flex-direction: column;
  }
  .name {
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 0.96429rem;
    font-weight: 500;
  }
  .desc {
    font-size: 0.85714rem;
    color: var(--fg-3);
    white-space: nowrap;
    overflow: hidden;
    text-overflow: ellipsis;
  }
  .badge {
    padding: 1px 7px;
    border-radius: 8px;
    font-size: 0.78571rem;
    font-weight: 500;
  }
  .badge.ok {
    color: var(--success);
    background: color-mix(in srgb, var(--success) 14%, transparent);
  }
  .badge.warn {
    color: var(--warning);
    background: color-mix(in srgb, var(--warning) 14%, transparent);
  }
  .badge.muted {
    color: var(--fg-3);
    background: var(--fill);
  }
  .chev {
    color: var(--fg-3);
    display: flex;
    transition: transform 260ms var(--ease-out);
  }
  .expanded .chev {
    transform: rotate(180deg);
  }
  .body {
    padding: 4px 16px 16px 60px;
    display: flex;
    flex-direction: column;
    gap: 12px;
    animation: body-in 260ms var(--ease-out) both;
  }
  @keyframes body-in {
    from {
      opacity: 0;
      transform: translateY(-4px);
    }
  }
  .detail {
    margin: 0;
    font-size: 0.92857rem;
  }
  .path {
    margin: -8px 0 0;
    font-family: var(--font-mono);
    font-size: 0.82143rem;
    color: var(--fg-3);
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    user-select: text;
    -webkit-user-select: text;
  }
  .steps {
    margin: 0;
    padding-left: 18px;
    font-size: 0.89286rem;
    color: var(--fg-2);
    display: grid;
    gap: 4px;
  }
  .steps code {
    font-family: var(--font-mono);
    font-size: 0.82143rem;
    padding: 1px 6px;
    border-radius: 5px;
    background: var(--fill);
    user-select: all;
    -webkit-user-select: all;
  }
  .note {
    display: flex;
    gap: 6px;
    margin: 0;
    font-size: 0.85714rem;
    color: var(--fg-3);
  }
  .note :global(.icon) {
    margin-top: 2px;
  }
  .actions {
    display: flex;
    gap: 8px;
    flex-wrap: wrap;
  }
  .field {
    display: flex;
    flex-direction: column;
    gap: 6px;
  }
  .label {
    font-size: 0.85714rem;
    font-weight: 500;
    color: var(--fg-2);
  }
  input {
    height: 32px;
    padding: 0 10px;
    border: 0;
    border-radius: 8px;
    background: var(--fill);
    box-shadow: inset 0 0 0 0.5px var(--line-strong);
    font-size: 0.92857rem;
    outline: none;
    user-select: text;
    -webkit-user-select: text;
    transition: box-shadow 160ms;
  }
  input:focus {
    box-shadow: inset 0 0 0 1.5px var(--accent);
  }
  .key-form {
    display: flex;
    gap: 8px;
  }
  .key-form input {
    flex: 1;
    font-family: var(--font-mono);
  }
  .saved {
    display: flex;
    align-items: center;
    gap: 10px;
    height: 32px;
  }
  .dots {
    letter-spacing: 1px;
    color: var(--fg-2);
  }
  .where {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    font-size: 0.82143rem;
    color: var(--fg-3);
  }
  .grow {
    flex: 1;
  }
  .link {
    align-self: flex-start;
    display: inline-flex;
    align-items: center;
    gap: 4px;
    border: 0;
    padding: 0;
    background: none;
    color: var(--accent);
    font-size: 0.89286rem;
    font-weight: 500;
  }
  .link:hover {
    text-decoration: underline;
  }
  .tags {
    display: flex;
    flex-wrap: wrap;
    gap: 6px;
    align-items: center;
    min-height: 34px;
    padding: 4px;
    border-radius: 8px;
    background: var(--fill);
    box-shadow: inset 0 0 0 0.5px var(--line-strong);
  }
  .tag {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    height: 24px;
    padding: 0 4px 0 8px;
    border-radius: 6px;
    background: var(--surface-strong);
    font-family: var(--font-mono);
    font-size: 0.82143rem;
    box-shadow: 0 0 0 0.5px var(--line-strong);
  }
  .tag button {
    display: grid;
    place-items: center;
    width: 18px;
    height: 18px;
    border: 0;
    border-radius: 4px;
    background: none;
    color: var(--fg-3);
  }
  .tag button:hover {
    background: var(--fill-hover);
    color: var(--fg);
  }
  .tag-input {
    flex: 1;
    min-width: 160px;
    height: 24px;
    background: none;
    box-shadow: none;
    font-family: var(--font-mono);
    font-size: 0.85714rem;
  }
  .tag-input:focus {
    box-shadow: none;
  }
  .test {
    display: flex;
    align-items: center;
    gap: 6px;
    margin: 0;
    font-size: 0.89286rem;
    animation: body-in 220ms var(--ease-out) both;
  }
  .test.ok {
    color: var(--success);
  }
  .test.err {
    color: var(--danger);
  }
</style>
