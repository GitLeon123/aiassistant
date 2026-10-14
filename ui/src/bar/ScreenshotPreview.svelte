<script lang="ts">
  import { onMount } from 'svelte';
  import Icon from '../lib/Icon.svelte';
  import type { ScreenInfo } from '../lib/types';

  interface Props {
    screen: ScreenInfo;
    expanded?: boolean;
    onready: (id: number) => void;
    onerror: (id: number) => void;
    onremove: () => void;
  }
  let { screen, expanded = $bindable(false), onready, onerror, onremove }: Props = $props();
  let justCaptured = $state(true);
  let failed = $state(false);
  const area = $derived(screen.mode === 'window' ? `${screen.app || 'Active window'} · Monitor ${screen.monitor}` : `Monitor ${screen.monitor}`);

  onMount(() => {
    const timer = setTimeout(() => { justCaptured = false; }, 2400);
    return () => clearTimeout(timer);
  });
</script>

<div class="attachment" class:expanded>
  <div class="summary">
    <button
      type="button"
      class="preview-toggle"
      aria-label={failed ? 'Screenshot preview unavailable' : expanded ? 'Collapse screenshot preview' : 'Enlarge screenshot preview'}
      aria-expanded={expanded}
      disabled={failed}
      onclick={() => { expanded = !expanded; }}
    >
      {#if failed}
        <span class="thumbnail-placeholder" aria-hidden="true"><Icon name="warning" size={20} /></span>
      {:else}
      <img
        src={screen.thumb}
        alt="Screenshot to be sent"
        draggable="false"
        onload={() => onready(screen.id)}
        onerror={() => { failed = true; expanded = false; onerror(screen.id); }}
      />
      {/if}
      <span class="area">{area}, {screen.width}×{screen.height}</span>
    </button>
    <button type="button" class="remove" aria-label="Remove screenshot" title="Remove screenshot" onclick={onremove}>
      <Icon name="x" size={14} />
    </button>
  </div>
  <span class="sr-only" role="status">
    {failed ? 'Preview failed · remove to send text only' : justCaptured ? 'Screen captured · review before sending' : 'Captured when the bar opened · click to inspect'}
  </span>
  {#if expanded && !failed}
    <div class="full-preview">
      <img src={screen.thumb} alt="Full screenshot to be sent" draggable="false" />
      <span class="capture-state">Captured when the bar opened · review before sending</span>
    </div>
  {/if}
</div>

<style>
  .attachment { margin: 0 14px 8px; }
  .summary { display: flex; align-items: center; gap: 8px; }
  .preview-toggle { display: flex; align-items: center; gap: 8px; flex: 1; min-width: 0; min-height: 30px; padding: 2px 4px; border: 0; border-radius: 6px; background: transparent; text-align: left; }
  .preview-toggle:not(:disabled):hover .area { text-decoration: underline; text-underline-offset: 3px; }
  .preview-toggle > img, .thumbnail-placeholder { width: 36px; height: 24px; flex: none; border-radius: 4px; background: var(--code-bg); }
  .preview-toggle > img { object-fit: contain; }
  .thumbnail-placeholder { display: grid; place-items: center; color: var(--fg-2); }
  .area { color: var(--fg-2); font-size: 0.85714rem; overflow-wrap: anywhere; }
  .capture-state { display: block; margin-top: 6px; color: var(--fg-2); font-size: 0.85714rem; }
  .remove { display: grid; place-items: center; width: 30px; height: 30px; flex: none; border: 0; border-radius: 8px; background: transparent; color: var(--fg-2); }
  .remove:hover { background: var(--fill-hover); color: var(--fg); }
  .full-preview { padding: 8px 4px 4px; }
  .full-preview img { display: block; width: 100%; max-height: 280px; object-fit: contain; border-radius: 6px; background: var(--code-bg); }
</style>
