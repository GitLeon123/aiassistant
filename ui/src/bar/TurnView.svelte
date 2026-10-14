<script lang="ts">
  import { onDestroy } from 'svelte';
  import { send } from '../lib/bridge';
  import Icon from '../lib/Icon.svelte';
  import { formattedAnswer, handleMarkdownClick, patchMarkdown } from '../lib/markdown';
  import { copyToHost } from '../lib/clipboard';
  import { reducedMotion } from '../lib/motion';
  import ThinkingPill from './ThinkingPill.svelte';
  import type { Turn } from './turn';

  interface Props {
    turn: Turn;
    onretry: (turn: Turn) => void;
  }
  let { turn, onretry }: Props = $props();

  let answerEl = $state<HTMLDivElement>();
  let frame = 0;
  let copyStatus = $state('');
  let copyTimer: ReturnType<typeof setTimeout>;

  // Patch the rendered markdown at most once per frame while text streams in.
  $effect(() => {
    const text = turn.answer;
    const streaming = turn.state === 'streaming';
    if (!answerEl) return;
    cancelAnimationFrame(frame);
    frame = requestAnimationFrame(() => {
      if (answerEl) patchMarkdown(answerEl, text, streaming && !reducedMotion(), streaming);
    });
  });
  onDestroy(() => { cancelAnimationFrame(frame); clearTimeout(copyTimer); });

  const firstToken = $derived(turn.streamed && turn.firstTokenAt ? turn.firstTokenAt - turn.startedAt : 0);
  // Streamed: throughput over the generation window (first token to finish), like provider
  // dashboards. Whole-message providers (Codex) give no per-token timing, so average over the
  // full response time instead of the few milliseconds between the message and the finish.
  const tokensPerSecond = $derived.by(() => {
    const from = turn.streamed ? turn.firstTokenAt : turn.startedAt;
    const ms = turn.finishedAt - from;
    return turn.outputTokens && from && ms > 50 ? turn.outputTokens / (ms / 1000) : 0;
  });

  function seconds(ms: number) {
    return `${(ms / 1000).toFixed(ms < 10_000 ? 2 : 1)}s`;
  }

  const needsSettings = $derived(/settings|api key|sign in|signed in|not installed|choose a model/i.test(turn.error));

  async function copyAnswer(formatted = false) {
    const data = formatted ? formattedAnswer(turn.answer) : { text: turn.answer, html: undefined };
    const ok = await copyToHost(data.text, data.html);
    copyStatus = ok ? 'Copied' : 'Copy failed. Try again.';
    clearTimeout(copyTimer);
    copyTimer = setTimeout(() => (copyStatus = ''), 2200);
  }
</script>

<article class="turn">
  <div class="question">
    {#if turn.screen}
      <span class="ctx" title={turn.screenLabel}><Icon name="screen" size={12} /></span>
    {/if}
    <p>{turn.question}</p>
  </div>

  <ThinkingPill {turn} />

  <!-- svelte-ignore a11y_click_events_have_key_events, a11y_no_static_element_interactions -->
  <div
    class="md answer"
    aria-live="off"
    bind:this={answerEl}
    onclick={(e) =>
      handleMarkdownClick(e, {
        copy: (text) => copyToHost(text),
        open: (url) => send({ type: 'openUrl', url }),
      })}
  ></div>

  {#if turn.state === 'streaming' && turn.answer}
    <span class="caret" aria-hidden="true"></span>
  {/if}

  {#if turn.state === 'error'}
    <div class="error">
      <Icon name="warning" size={16} />
      <p>{turn.error || 'Something went wrong.'}</p>
      <div class="error-actions">
        {#if needsSettings}
          <button type="button" class="link" onclick={() => send({ type: 'openSettings' })}>Open Settings</button>
        {/if}
        <button type="button" class="link" onclick={() => onretry(turn)}>Retry</button>
      </div>
    </div>
  {/if}
  {#if turn.limitWarning}<p class="usage-warning">{turn.limitWarning}</p>{/if}

  {#if turn.state !== 'streaming'}
    <footer class="meta">
      {#if turn.answer}
        <button type="button" class="copy-action" title="Copy Markdown, including LaTeX" onclick={() => copyAnswer()}>
          <Icon name="copy" size={14} /> Copy
        </button>
        <button type="button" class="copy-action" title="Copy formatted text for Word or Outlook" onclick={() => copyAnswer(true)}>Formatted</button>
      {/if}
      {#if copyStatus}<span>{copyStatus}</span>{/if}
      <span>{turn.providerLabel} · {turn.modelLabel}</span>
      {#if turn.state === 'cancelled'}
        <span class="dot">·</span><span>Stopped</span>
      {/if}
      {#if turn.outputTokens}
        <span class="dot">·</span><span>{turn.outputTokens.toLocaleString()} tokens</span>
      {/if}
      {#if firstToken}
        <span class="dot">·</span><span title="Time from sending to the first token">First token {seconds(firstToken)}</span>
      {:else if !turn.streamed && turn.finishedAt}
        <span class="dot">·</span><span title="This provider sends the answer all at once, so the first token can't be timed">Response {seconds(turn.finishedAt - turn.startedAt)}</span>
      {/if}
      {#if tokensPerSecond}
        <span class="dot">·</span><span title={turn.streamed ? 'Output tokens per second after the first token' : 'Average output tokens per second over the whole response'}>{turn.streamed ? '' : '≈'}{Math.round(tokensPerSecond).toLocaleString()} tok/s</span>
      {/if}
    </footer>
  {/if}
</article>

<style>
  .turn {
    padding: 14px 0 6px;
    animation: turn-in 360ms var(--ease-out) both;
  }
  .turn + :global(.turn) {
    border-top: 1px solid var(--line);
  }
  @keyframes turn-in {
    from {
      opacity: 0;
      transform: translateY(6px);
    }
  }
  .question {
    display: flex;
    align-items: flex-start;
    gap: 8px;
    margin-bottom: 10px;
    color: var(--fg-2);
    font-size: 0.92857rem;
    font-weight: 500;
  }
  .question p {
    margin: 0;
    white-space: pre-wrap;
    overflow-wrap: anywhere;
    user-select: text;
    -webkit-user-select: text;
    display: -webkit-box;
    -webkit-line-clamp: 3;
    line-clamp: 3;
    -webkit-box-orient: vertical;
    overflow: hidden;
  }
  .ctx {
    flex: none;
    display: grid;
    place-items: center;
    width: 20px;
    height: 20px;
    margin-top: -1px;
    border-radius: 6px;
    background: color-mix(in srgb, var(--accent) 18%, transparent);
    color: var(--accent);
  }
  .answer:empty {
    display: none;
  }
  .caret {
    display: inline-block;
    width: 7px;
    height: 7px;
    margin: 4px 0 0 2px;
    border-radius: 50%;
    background: var(--accent);
    animation: pulse 1s ease-in-out infinite;
  }
  @keyframes pulse {
    50% {
      transform: scale(0.6);
      opacity: 0.5;
    }
  }
  .error {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 2px 10px;
    align-items: start;
    padding: 10px 12px;
    border-radius: 12px;
    background: color-mix(in srgb, var(--danger) 12%, transparent);
    box-shadow: inset 0 0 0 0.5px color-mix(in srgb, var(--danger) 40%, transparent);
    color: var(--danger);
    font-size: 0.92857rem;
    animation: turn-in 280ms var(--ease-out) both;
  }
  .error p {
    margin: 0;
    color: var(--fg);
    user-select: text;
    -webkit-user-select: text;
  }
  .error-actions {
    grid-column: 2;
    display: flex;
    gap: 14px;
    margin-top: 4px;
  }
  .link {
    border: 0;
    padding: 0;
    background: none;
    color: var(--accent);
    font-weight: 600;
    font-size: 0.89286rem;
  }
  .link:hover {
    text-decoration: underline;
  }
  .meta {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 6px;
    margin-top: 8px;
    color: var(--fg-3);
    font-size: 0.82143rem;
    animation: turn-in 300ms var(--ease-out) both;
  }
  .usage-warning { font-size: 0.9rem; color: var(--fg-2); }
  .dot {
    color: var(--fg-4);
  }
  .copy-action {
    display: inline-flex;
    gap: 5px;
    place-items: center;
    min-height: 28px;
    padding: 3px 7px;
    border: 0;
    border-radius: 7px;
    background: transparent;
    color: var(--fg-2);
    transition:
      background 120ms,
      transform 160ms var(--ease-spring);
  }
  .copy-action:hover {
    background: var(--fill-hover);
  }
  .copy-action:active {
    transform: scale(0.92);
  }
</style>
