<script lang="ts">
  import type { BarPosition } from '../lib/types';

  interface Props {
    value: BarPosition;
    onchange: (v: BarPosition) => void;
  }
  let { value, onchange }: Props = $props();

  // A miniature screen: each cell is one place the bar can open.
  const rows = ['top', 'middle', 'bottom'] as const;
  const cols = ['left', 'center', 'right'] as const;
  const labels: Record<BarPosition, string> = {
    'top-left': 'Top left',
    'top-center': 'Top center',
    'top-right': 'Top right',
    'middle-left': 'Middle left',
    'middle-center': 'Middle center',
    'middle-right': 'Middle right',
    'bottom-left': 'Bottom left',
    'bottom-center': 'Bottom center',
    'bottom-right': 'Bottom right',
  };
</script>

<div class="screen" role="radiogroup" aria-label="Bar position">
  {#each rows as r (r)}
    {#each cols as c (c)}
      {@const pos = `${r}-${c}` as BarPosition}
      <button
        type="button"
        role="radio"
        class="cell"
        class:on={value === pos}
        aria-checked={value === pos}
        aria-label={labels[pos]}
        title={labels[pos]}
        onclick={() => onchange(pos)}
      >
        <span class="dot"></span>
      </button>
    {/each}
  {/each}
</div>

<style>
  .screen {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    grid-template-rows: repeat(3, 1fr);
    width: 96px;
    height: 86px;
    gap: 2px;
    padding: 5px;
    border-radius: 8px;
    background: var(--fill);
    box-shadow: inset 0 0 0 0.5px var(--line-strong);
  }
  .cell {
    display: grid;
    place-items: center;
    padding: 0;
    border: 0;
    border-radius: 4px;
    background: transparent;
    cursor: pointer;
  }
  .cell:hover .dot {
    background: var(--fg-2);
    opacity: 0.6;
  }
  .cell:focus-visible {
    outline: 2px solid var(--accent);
    outline-offset: 1px;
  }
  .dot {
    width: 5px;
    height: 5px;
    border-radius: 50%;
    background: var(--fg-2);
    opacity: 0.35;
    transition:
      background 160ms var(--ease-spring),
      opacity 160ms var(--ease-spring),
      transform 220ms var(--ease-spring);
  }
  .on .dot {
    width: 9px;
    height: 9px;
    background: var(--accent);
    opacity: 1;
    box-shadow: 0 0 6px color-mix(in srgb, var(--accent) 50%, transparent);
  }
</style>
