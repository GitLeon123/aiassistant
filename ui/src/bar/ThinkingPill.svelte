<script lang="ts">
  import { onDestroy } from 'svelte';
  import { formatDuration } from '../lib/dom';
  import Icon from '../lib/Icon.svelte';
  import { renderMarkdownInto } from '../lib/markdown';
  import type { Turn } from './turn';

  interface Props {
    turn: Turn;
  }
  let { turn }: Props = $props();

  let expanded = $state(false);
  let body = $state<HTMLDivElement>();
  let now = $state(performance.now());

  const thinking = $derived(turn.state === 'streaming' && !turn.answer);
  const visible = $derived(thinking || !!turn.reasoning);

  // Live elapsed timer while the model thinks.
  const timer = setInterval(() => {
    if (thinking) now = performance.now();
  }, 250);
  onDestroy(() => clearInterval(timer));

  const elapsed = $derived((turn.answeredAt || turn.finishedAt || now) - turn.startedAt);
  const label = $derived.by(() => {
    if (thinking) {
      if (turn.reasoning) return 'Thinking';
      return turn.status || 'Thinking';
    }
    return `Thought for ${formatDuration(elapsed)}`;
  });

  $effect(() => {
    if (expanded && body) renderMarkdownInto(body, turn.reasoning || '…');
  });

  // Keep the newest reasoning in view while it streams in.
  $effect(() => {
    void turn.reasoning;
    if (expanded && thinking && body) body.scrollTop = body.scrollHeight;
  });
</script>

{#if visible}
  <div class="thinking" class:expanded>
    <button
      class="pill"
      class:live={thinking}
      type="button"
      disabled={!turn.reasoning}
      onclick={() => (expanded = !expanded)}
      aria-expanded={expanded}
    >
      <Icon name="brain" size={14} />
      <span class="label" class:shimmer={thinking}>{label}</span>
      {#if thinking && elapsed > 1500}
        <span class="time">{formatDuration(elapsed)}</span>
      {/if}
      {#if turn.reasoning}
        <span class="chev"><Icon name="chevronRight" size={12} stroke={2} /></span>
      {/if}
    </button>
    {#if expanded}
      <div class="body md" bind:this={body}></div>
    {/if}
  </div>
{/if}

<style>
  .thinking {
    margin: 0 0 10px;
  }
  .pill {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    min-height: 26px;
    padding-block: 4px;
    padding: 0 10px 0 9px;
    border: 0;
    border-radius: 13px;
    background: var(--fill);
    color: var(--fg-2);
    font-size: 0.89286rem;
    transition:
      background 140ms,
      transform 180ms var(--ease-spring);
  }
  .pill:not(:disabled):hover {
    background: var(--fill-hover);
  }
  .pill:not(:disabled):active {
    transform: scale(0.96);
  }
  .pill:disabled {
    opacity: 1;
  }
  .time {
    color: var(--fg-3);
    font-variant-numeric: tabular-nums;
  }
  .chev {
    display: flex;
    color: var(--fg-3);
    transition: transform 240ms var(--ease-out);
  }
  .expanded .chev {
    transform: rotate(90deg);
  }
  .shimmer {
    background: linear-gradient(
      100deg,
      var(--fg-3) 0%,
      var(--fg-3) 35%,
      var(--fg) 50%,
      var(--fg-3) 65%,
      var(--fg-3) 100%
    );
    background-size: 250% 100%;
    background-clip: text;
    -webkit-background-clip: text;
    color: transparent;
    animation: shimmer 1.6s linear infinite;
  }
  @keyframes shimmer {
    from {
      background-position: 100% 0;
    }
    to {
      background-position: -150% 0;
    }
  }
  .live :global(.icon) {
    animation: breathe 1.6s ease-in-out infinite;
  }
  @keyframes breathe {
    50% {
      opacity: 0.45;
    }
  }
  .body {
    margin: 8px 0 2px 4px;
    padding: 2px 0 2px 12px;
    border-left: 2px solid var(--line-strong);
    max-height: 180px;
    overflow-y: auto;
    font-size: 0.89286rem;
    line-height: 1.55;
    color: var(--fg-2);
    animation: reveal 260ms var(--ease-out) both;
  }
  @keyframes reveal {
    from {
      opacity: 0;
      transform: translateY(-3px);
    }
  }
  :global(:root[data-motion='reduced']) .shimmer {
    animation: none;
    color: var(--fg-2);
  }
</style>
