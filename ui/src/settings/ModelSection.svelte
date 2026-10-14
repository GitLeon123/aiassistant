<script lang="ts">
  import Button from '../lib/controls/Button.svelte';
  import Row from '../lib/controls/Row.svelte';
  import Segmented from '../lib/controls/Segmented.svelte';
  import Toggle from '../lib/controls/Toggle.svelte';
  import Icon from '../lib/Icon.svelte';
  import { app, levelFor, levelLabel } from '../lib/state.svelte';
  import type { ModelInfo, ProviderInfo } from '../lib/types';

  interface Props {
    gotoProviders: () => void;
  }
  let { gotoProviders }: Props = $props();

  let filter = $state('');
  const choice = $derived(app.defaultChoice);
  const model = $derived(choice ? app.findModel(choice.provider, choice.model) : undefined);
  const level = $derived(levelFor(model, choice?.reasoning ?? ''));
  const provider = $derived(choice ? app.provider(choice.provider) : undefined);

  $effect(() => {
    for (const p of app.enabledProviders) app.requestModels(p.id);
  });

  const groups = $derived.by(() => {
    const q = filter.trim().toLowerCase();
    return app.enabledProviders.map((p) => {
      const state = app.modelsFor(p.id);
      return {
        provider: p,
        state,
        models: state.list.filter((m) => !q || m.name.toLowerCase().includes(q) || m.id.toLowerCase().includes(q)),
      };
    });
  });

  function select(p: ProviderInfo, m: ModelInfo) {
    app.saveSettings({ defaultModel: { provider: p.id, model: m.id, reasoning: levelFor(m, choice?.reasoning ?? '') } });
  }

  function setLevel(reasoning: string) {
    if (choice) app.saveSettings({ defaultModel: { ...choice, reasoning } });
  }

  function setReasoningOverride(on: boolean) {
    if (!choice || !model) return;
    const overrides = { ...(app.config(choice.provider).reasoningOverrides ?? {}), [model.id]: on };
    app.saveProviderConfig(choice.provider, { reasoningOverrides: overrides });
    app.requestModels(choice.provider, true);
  }

  // System prompt: saved when the field loses focus.
  let prompt = $state('');
  let editingPrompt = false;
  $effect(() => {
    const value = app.settings?.systemPrompt ?? '';
    if (!editingPrompt) prompt = value;
  });
  function savePrompt() {
    editingPrompt = false;
    const value = prompt.trim() === (app.payload?.defaultSystemPrompt ?? '').trim() ? '' : prompt;
    if (value !== (app.settings?.systemPrompt ?? '')) app.saveSettings({ systemPrompt: value });
  }
</script>

<section>
  <h2>Model</h2>

  <div class="current">
    {#if choice}
      <div class="current-head">
        <div>
          <div class="eyebrow">Default model</div>
          <div class="current-name">{model?.name ?? choice.model}</div>
          <div class="current-sub">
            {provider?.name ?? choice.provider}
            {#if model}
              · {model.vision ? 'Sees your screen' : 'Text only (window title is shared instead)'}
            {/if}
          </div>
        </div>
      </div>
      {#if model && model.reasoning.mode !== 'none' && model.reasoning.levels.length > 1}
        <div class="reasoning">
          <span class="r-label"><Icon name="brain" size={14} /> Reasoning</span>
          <Segmented
            label="Reasoning level"
            value={level}
            options={model.reasoning.levels.map((l) => ({ value: l, label: levelLabel(l) }))}
            onchange={setLevel}
          />
        </div>
      {/if}
      {#if model && provider && provider.kind !== 'cli'}
        <div class="override">
          <span>Treat as a reasoning model</span>
          <Toggle checked={model.reasoning.mode !== 'none'} label="Reasoning model" onchange={setReasoningOverride} />
        </div>
      {/if}
    {:else}
      <div class="empty">
        <p>No model selected yet. Connect a provider first.</p>
        <Button variant="primary" onclick={gotoProviders}>Connect a provider</Button>
      </div>
    {/if}
  </div>

  {#if app.enabledProviders.length}
    <div class="list-head">
      <h3>Available models</h3>
      <div class="search">
        <Icon name="search" size={13} />
        <input bind:value={filter} placeholder="Filter" spellcheck="false" />
      </div>
    </div>
    <div class="group models">
      {#each groups as g (g.provider.id)}
        <div class="provider-head">
          <span>{g.provider.name}</span>
          {#if g.state.loading}<span class="loading">Loading…</span>{/if}
          {#if g.state.error}<span class="error" title={g.state.error}>Using built-in list · {g.state.error}</span>{/if}
          <span class="grow"></span>
          <button type="button" class="refresh" title="Refresh" onclick={() => app.requestModels(g.provider.id, true)}>
            <Icon name="refresh" size={13} />
          </button>
        </div>
        {#each g.models as m (m.id)}
          {@const selected = choice?.provider === g.provider.id && choice?.model === m.id}
          <button type="button" class="model" class:selected onclick={() => select(g.provider, m)}>
            <span class="radio" aria-hidden="true"></span>
            <span class="m-name">{m.name}</span>
            {#if m.name !== m.id}<span class="m-id">{m.id}</span>{/if}
            <span class="caps">
              {#if m.reasoning.mode !== 'none'}<span title="Reasoning"><Icon name="brain" size={13} /></span>{/if}
              {#if m.vision}<span title="Vision"><Icon name="eye" size={13} /></span>{/if}
            </span>
          </button>
        {:else}
          {#if !g.state.loading}<div class="none">No models{filter ? ' match' : ''}</div>{/if}
        {/each}
      {/each}
    </div>
  {/if}

  <h3>Instructions</h3>
  <div class="group prompt">
    <Row title="System prompt" description="Sent at the start of every conversation. Leave empty for the default.">
      {#if app.settings?.systemPrompt}
        <Button
          size="sm"
          variant="ghost"
          onclick={() => {
            prompt = '';
            app.saveSettings({ systemPrompt: '' });
          }}>Reset</Button
        >
      {/if}
    </Row>
    <textarea
      bind:value={prompt}
      placeholder={app.payload?.defaultSystemPrompt}
      rows="5"
      spellcheck="true"
      onfocus={() => (editingPrompt = true)}
      onblur={savePrompt}
    ></textarea>
  </div>
</section>

<style>
  .current {
    padding: 18px 20px;
    border-radius: 14px;
    background:
      radial-gradient(120% 140% at 0% 0%, color-mix(in srgb, var(--accent) 20%, transparent), transparent 60%),
      var(--surface);
    box-shadow:
      inset 0 0 0 0.5px var(--line-strong),
      0 1px 2px rgba(0, 0, 0, 0.05);
  }
  .eyebrow {
    font-size: 0.82143rem;
    font-weight: 600;
    letter-spacing: 0.02em;
    text-transform: uppercase;
    color: var(--fg-3);
  }
  .current-name {
    font-family: var(--font-display);
    font-size: 1.5714rem;
    font-weight: 600;
    letter-spacing: -0.01em;
    margin-top: 2px;
  }
  .current-sub {
    font-size: 0.89286rem;
    color: var(--fg-2);
  }
  .reasoning,
  .override {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
    margin-top: 14px;
    padding-top: 14px;
    border-top: 1px solid var(--line);
    font-size: 0.92857rem;
  }
  .override {
    margin-top: 10px;
    padding-top: 10px;
    color: var(--fg-2);
    font-size: 0.89286rem;
  }
  .r-label {
    display: flex;
    align-items: center;
    gap: 6px;
  }
  .empty {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 12px;
  }
  .empty p {
    margin: 0;
    color: var(--fg-2);
  }
  .list-head {
    display: flex;
    align-items: flex-end;
    justify-content: space-between;
  }
  .search {
    display: flex;
    align-items: center;
    gap: 6px;
    height: 28px;
    margin-bottom: 8px;
    padding: 0 8px;
    border-radius: 8px;
    background: var(--fill);
    color: var(--fg-3);
  }
  .search input {
    width: 140px;
    border: 0;
    outline: 0;
    background: transparent;
    font-size: 0.89286rem;
  }
  .models {
    padding: 4px;
    max-height: 340px;
    overflow-y: auto;
  }
  .provider-head {
    display: flex;
    align-items: center;
    gap: 8px;
    padding: 10px 10px 4px;
    font-size: 0.85714rem;
    font-weight: 600;
    color: var(--fg-2);
  }
  .loading,
  .error {
    font-weight: 400;
    color: var(--fg-3);
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    max-width: 380px;
  }
  .error {
    color: var(--warning);
  }
  .grow {
    flex: 1;
  }
  .refresh {
    display: grid;
    place-items: center;
    width: 24px;
    height: 24px;
    border: 0;
    border-radius: 6px;
    background: none;
    color: var(--fg-3);
  }
  .refresh:hover {
    background: var(--fill-hover);
    color: var(--fg);
  }
  .model {
    display: flex;
    align-items: center;
    gap: 10px;
    width: 100%;
    height: 36px;
    padding: 0 10px;
    border: 0;
    border-radius: 8px;
    background: none;
    text-align: left;
    font-size: 0.92857rem;
  }
  .model:hover {
    background: var(--fill);
  }
  .radio {
    flex: none;
    width: 16px;
    height: 16px;
    border-radius: 50%;
    box-shadow: inset 0 0 0 1.5px var(--fg-4);
    transition: box-shadow 200ms var(--ease-out);
  }
  .selected .radio {
    box-shadow:
      inset 0 0 0 5px var(--accent),
      inset 0 0 0 16px #fff;
  }
  .m-id {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font-size: 0.82143rem;
    color: var(--fg-3);
  }
  .caps {
    display: flex;
    gap: 5px;
    margin-left: auto;
    color: var(--fg-3);
  }
  .none {
    padding: 4px 10px 8px;
    font-size: 0.85714rem;
    color: var(--fg-3);
  }
  .prompt textarea {
    display: block;
    width: calc(100% - 32px);
    margin: 0 16px 16px;
    padding: 10px 12px;
    border: 0;
    border-radius: 10px;
    background: var(--fill);
    box-shadow: inset 0 0 0 0.5px var(--line-strong);
    outline: none;
    resize: vertical;
    font-size: 0.92857rem;
    line-height: 1.5;
    user-select: text;
    -webkit-user-select: text;
  }
  .prompt textarea:focus {
    box-shadow: inset 0 0 0 1.5px var(--accent);
  }
  .prompt textarea::placeholder {
    color: var(--fg-3);
  }
</style>
