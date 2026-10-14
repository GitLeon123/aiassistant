<script lang="ts">
  import { onMount, tick } from 'svelte';
  import { send } from '../lib/bridge';
  import Glass from '../lib/glass/Glass.svelte';
  import Icon from '../lib/Icon.svelte';
  import { destination } from '../lib/privacy';
  import { app, levelFor, levelLabel } from '../lib/state.svelte';
  import type { ModelChoice, ModelInfo, ProviderInfo } from '../lib/types';

  interface Props {
    backdrop: string;
    mode: 'liquid' | 'system' | 'solid';
    x: number;
    y: number;
    current: ModelChoice | null;
    element?: HTMLDivElement;
    onpick: (choice: ModelChoice) => void;
    onlevel: (level: string) => void;
    onclose: () => void;
  }
  let { backdrop, mode, x, y, current, element = $bindable(), onpick, onlevel, onclose }: Props = $props();

  let query = $state('');
  let active = $state(0);
  let search = $state<HTMLInputElement>();
  let list = $state<HTMLDivElement>();

  interface Row {
    provider: ProviderInfo;
    model: ModelInfo;
  }

  const groups = $derived.by(() => {
    const q = query.trim().toLowerCase();
    return app.enabledProviders.map((provider) => {
      const state = app.modelsFor(provider.id);
      const models = state.list.filter(
        (m) =>
          !q ||
          m.name.toLowerCase().includes(q) ||
          m.id.toLowerCase().includes(q) ||
          provider.name.toLowerCase().includes(q),
      );
      const scope = destination(provider, app.config(provider.id)).scope;
      return { provider, scope, models, loading: state.loading, error: state.error };
    });
  });

  const rows = $derived<Row[]>(groups.flatMap((g) => g.models.map((model) => ({ provider: g.provider, model }))));

  const currentModel = $derived(current ? app.findModel(current.provider, current.model) : undefined);
  const currentLevel = $derived(levelFor(currentModel, current?.reasoning ?? ''));

  onMount(() => {
    for (const p of app.enabledProviders) app.requestModels(p.id);
    search?.focus();
  });

  // Start on the current model (its list may arrive after mount); a search starts at the top.
  let landed = false;
  let lastQuery = '';
  $effect(() => {
    if (query !== lastQuery) {
      lastQuery = query;
      landed = true;
      active = 0;
      return;
    }
    if (landed) return;
    const idx = rows.findIndex((r) => r.provider.id === current?.provider && r.model.id === current?.model);
    if (idx < 0) return;
    landed = true;
    active = idx;
    tick().then(scrollActive);
  });

  function scrollActive() {
    list?.querySelector<HTMLElement>(`[data-index="${active}"]`)?.scrollIntoView({ block: 'nearest' });
  }

  function pick(row: Row) {
    onpick({ provider: row.provider.id, model: row.model.id, reasoning: levelFor(row.model, current?.reasoning ?? '') });
  }

  function onkeydown(e: KeyboardEvent) {
    if (e.key === 'ArrowDown' || e.key === 'ArrowUp') {
      e.preventDefault();
      if (!rows.length) return;
      active = (active + (e.key === 'ArrowDown' ? 1 : rows.length - 1)) % rows.length;
      tick().then(scrollActive);
    } else if (e.key === 'Enter') {
      e.preventDefault();
      if (rows[active]) pick(rows[active]);
    } else if (e.key === 'Escape') {
      e.preventDefault();
      e.stopPropagation();
      onclose();
    } else if ((e.key === 'ArrowLeft' || e.key === 'ArrowRight') && e.altKey && currentModel) {
      // Alt+←/→ steps the reasoning level.
      e.preventDefault();
      const levels = currentModel.reasoning.levels;
      const i = levels.indexOf(currentLevel);
      const next = levels[Math.min(levels.length - 1, Math.max(0, i + (e.key === 'ArrowRight' ? 1 : -1)))];
      if (next) onlevel(next);
    }
  }
</script>

<div class="picker" style="left:{x}px;top:{y}px" bind:this={element} role="dialog" aria-modal="true" aria-label="Choose model">
  <Glass {backdrop} {mode} radius={18} refraction={24} blur={22} tone="popover">
    <div class="inner">
      <div class="search">
        <Icon name="search" size={14} />
        <input
          class="quiet-input"
          bind:this={search}
          bind:value={query}
          {onkeydown}
          placeholder="Search models"
          spellcheck="false"
          autocomplete="off"
          aria-label="Search models"
          role="combobox"
          aria-expanded="true"
          aria-controls="model-list"
          aria-activedescendant={rows[active] ? `model-option-${active}` : undefined}
        />
      </div>

      <div class="list" id="model-list" bind:this={list} role="listbox" aria-label="Models">
        {#if app.enabledProviders.length === 0}
          <div class="empty">
            <p>No providers connected yet.</p>
            <button type="button" class="primary" onclick={() => send({ type: 'openSettings' })}>Connect a provider</button>
          </div>
        {/if}
        {#each groups as group (group.provider.id)}
          <div class="group-head">
            <span>{group.provider.name}</span>
            {#if group.scope !== 'Unknown'}
              <span class="kind" title={group.scope === 'Local' ? 'Runs on this computer' : 'Sent to the provider’s servers'}>
                <Icon name={group.scope === 'Local' ? 'cpu' : 'cloud'} size={12} />{group.scope}
              </span>
            {/if}
            {#if group.loading}<span class="spinner" aria-label="Loading"></span>{/if}
          </div>
          {#if group.error && group.models.length === 0}
            <div class="note">{group.error}</div>
          {:else if !group.loading && group.models.length === 0}
            <div class="note">{query ? 'No matches' : 'No models'}</div>
          {/if}
          {#each group.models as model (model.id)}
            {@const index = rows.findIndex((r) => r.provider.id === group.provider.id && r.model.id === model.id)}
            {@const selected = current?.provider === group.provider.id && current?.model === model.id}
            <button
              type="button"
              class="row"
              class:active={index === active}
              data-index={index}
              id="model-option-{index}"
              tabindex="-1"
              role="option"
              aria-selected={selected}
              onpointermove={() => (active = index)}
              onclick={() => pick({ provider: group.provider, model })}
            >
              <span class="name">{model.name}</span>
              {#if model.name !== model.id}<span class="id">{model.id}</span>{/if}
              <span class="caps">
                {#if model.reasoning.mode !== 'none'}<span title="Reasoning"><Icon name="brain" size={13} /></span>{/if}
                {#if model.vision}<span title="Can see your screen"><Icon name="eye" size={13} /></span>{/if}
              </span>
              <span class="check">{#if selected}<Icon name="check" size={14} stroke={2} />{/if}</span>
            </button>
          {/each}
        {/each}
      </div>

      {#if currentModel && currentModel.reasoning.mode !== 'none' && currentModel.reasoning.levels.length > 1}
        <div class="reasoning">
          <span class="r-label"><Icon name="brain" size={13} /> Reasoning</span>
          <div class="segmented" role="radiogroup" aria-label="Reasoning level">
            {#each currentModel.reasoning.levels as level (level)}
              <button
                type="button"
                role="radio"
                aria-checked={level === currentLevel}
                class:on={level === currentLevel}
                onclick={() => onlevel(level)}>{levelLabel(level)}</button
              >
            {/each}
          </div>
        </div>
      {/if}

      <div class="foot">
        <span><kbd>↑</kbd><kbd>↓</kbd> choose · <kbd>Alt</kbd>+<kbd>←</kbd><kbd>→</kbd> reasoning</span>
        <button type="button" class="manage" onclick={() => send({ type: 'openSettings' })}>Manage…</button>
      </div>
    </div>
  </Glass>
</div>

<style>
  .picker {
    position: absolute;
    width: 372px;
    max-width: calc(100vw - 16px);
    max-height: calc(100vh - 16px);
    overflow-y: auto;
    z-index: 20;
    transform-origin: top left;
    animation: pop 260ms var(--ease-spring) both;
  }
  @keyframes pop {
    from {
      opacity: 0;
      transform: scale(0.94) translateY(-6px);
      filter: blur(4px);
    }
  }
  :global(:root[data-motion='reduced']) .picker {
    animation: fade 120ms linear both;
  }
  @keyframes fade {
    from {
      opacity: 0;
    }
  }
  .inner {
    display: flex;
    flex-direction: column;
    max-height: min(460px, calc(100vh - 16px));
  }
  .search {
    flex: none;
    display: flex;
    align-items: center;
    gap: 8px;
    margin: 8px 8px 4px;
    padding: 0 10px;
    min-height: 34px;
    padding-block: 4px;
    border-radius: 10px;
    background: var(--fill);
    color: var(--fg-3);
  }
  .search input {
    flex: 1;
    min-width: 0;
    border: 0;
    background: transparent;
    font-size: 0.96429rem;
    color: var(--fg);
  }
  .search input::placeholder {
    color: var(--fg-3);
  }
  .list {
    flex: 1 1 auto;
    min-height: 80px;
    overflow-y: auto;
    padding: 2px 6px 6px;
  }
  .group-head {
    display: flex;
    align-items: center;
    gap: 8px;
    flex-wrap: wrap;
    padding: 10px 8px 4px;
    font-size: 0.82143rem;
    font-weight: 600;
    color: var(--fg-2);
  }
  .kind {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    font-weight: 500;
    color: var(--fg-3);
  }
  .note {
    padding: 4px 8px 6px;
    font-size: 0.85714rem;
    color: var(--fg-3);
  }
  .row {
    display: flex;
    align-items: center;
    gap: 8px;
    width: 100%;
    min-height: 34px;
    padding-block: 5px;
    flex-wrap: wrap;
    padding: 0 8px 0 10px;
    border: 0;
    border-radius: 9px;
    background: transparent;
    text-align: left;
    font-size: 0.96429rem;
  }
  .row.active {
    background: var(--fill-hover);
  }
  .name {
    overflow-wrap: anywhere;
    min-width: 0;
  }
  .id {
    flex: 1;
    min-width: 0;
    overflow: hidden;
    text-overflow: ellipsis;
    white-space: nowrap;
    font-size: 0.82143rem;
    color: var(--fg-3);
  }
  .name:last-of-type:not(:has(+ .id)) {
    flex: 1;
  }
  .caps {
    display: flex;
    gap: 4px;
    margin-left: auto;
    color: var(--fg-3);
  }
  .check {
    width: 16px;
    display: flex;
    color: var(--accent);
  }
  .empty {
    display: grid;
    justify-items: center;
    gap: 10px;
    padding: 22px 12px;
    color: var(--fg-2);
    font-size: 0.92857rem;
  }
  .empty p {
    margin: 0;
  }
  .primary {
    border: 0;
    height: 30px;
    padding: 0 14px;
    border-radius: 15px;
    background: var(--accent);
    color: var(--accent-fg);
    font-weight: 600;
    font-size: 0.89286rem;
  }
  .reasoning {
    display: flex;
    flex: none;
    flex-wrap: wrap;
    align-items: center;
    justify-content: space-between;
    gap: 10px;
    padding: 8px 10px;
    border-top: 1px solid var(--line);
  }
  .r-label {
    display: flex;
    align-items: center;
    gap: 6px;
    font-size: 0.85714rem;
    color: var(--fg-2);
    white-space: nowrap;
  }
  .segmented {
    display: flex;
    flex-wrap: wrap;
    gap: 3px;
    max-width: 100%;
    padding: 2px;
    border-radius: 9px;
    background: var(--fill);
    overflow-x: auto;
  }
  .segmented button {
    border: 0;
    min-height: 24px;
    padding-block: 4px;
    padding: 0 8px;
    border-radius: 7px;
    background: transparent;
    font-size: 0.82143rem;
    color: var(--fg-2);
    white-space: nowrap;
    transition:
      background 160ms,
      color 160ms;
  }
  .segmented button.on {
    background: var(--surface-strong);
    color: var(--fg);
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.18);
  }
  .foot {
    display: flex;
    flex: none;
    flex-wrap: wrap;
    gap: 8px;
    align-items: center;
    justify-content: space-between;
    padding: 6px 10px 8px;
    border-top: 1px solid var(--line);
    font-size: 0.78571rem;
    color: var(--fg-3);
  }
  .foot kbd {
    font-size: 0.71429rem;
    padding: 0 4px;
    margin: 0 1px;
  }
  .manage {
    border: 0;
    background: none;
    color: var(--accent);
    font-size: 0.85714rem;
    font-weight: 600;
    padding: 2px 4px;
  }
  .spinner {
    width: 10px;
    height: 10px;
    border-radius: 50%;
    border: 1.5px solid var(--fg-4);
    border-top-color: var(--fg-2);
    animation: spin 0.8s linear infinite;
  }
  @keyframes spin {
    to {
      transform: rotate(360deg);
    }
  }
</style>
