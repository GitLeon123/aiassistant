<script lang="ts">
  import { onDestroy } from 'svelte';
  import { on, send } from '../lib/bridge';

  interface Props {
    hotkey: string;
    error: string;
  }
  let { hotkey, error }: Props = $props();

  let recording = $state(false);
  let preview = $state<string[]>([]);
  let message = $state('');
  let messageKind = $state<'error' | 'ok' | ''>('');
  let button = $state<HTMLButtonElement>();

  // Names must match core/HotkeySpec.cpp (KeyFromName).
  const NAMED: Record<string, string> = {
    Space: 'Space', Enter: 'Enter', Tab: 'Tab', Backspace: 'Backspace', Delete: 'Delete', Insert: 'Insert',
    Home: 'Home', End: 'End', PageUp: 'PageUp', PageDown: 'PageDown', ArrowUp: 'Up', ArrowDown: 'Down',
    ArrowLeft: 'Left', ArrowRight: 'Right', Backquote: 'Backquote', Minus: 'Minus', Equal: 'Equal',
    Comma: 'Comma', Period: 'Period', Slash: 'Slash', Semicolon: 'Semicolon', Quote: 'Quote',
    BracketLeft: 'BracketLeft', BracketRight: 'BracketRight', Backslash: 'Backslash', Pause: 'Pause',
    PrintScreen: 'PrintScreen', ScrollLock: 'ScrollLock',
  };
  const DISPLAY: Record<string, string> = {
    Space: 'Space', Backquote: '`', Minus: '-', Equal: '=', Comma: ',', Period: '.', Slash: '/',
    Semicolon: ';', Quote: "'", BracketLeft: '[', BracketRight: ']', Backslash: '\\', Up: '↑', Down: '↓',
    Left: '←', Right: '→', Enter: '↵',
  };

  function keyName(code: string): string | null {
    let m = /^Key([A-Z])$/.exec(code);
    if (m) return m[1];
    m = /^Digit([0-9])$/.exec(code);
    if (m) return m[1];
    if (/^F([1-9]|1[0-9]|2[0-4])$/.test(code)) return code;
    return NAMED[code] ?? null;
  }

  function modifiers(e: KeyboardEvent): string[] {
    const mods: string[] = [];
    if (e.ctrlKey) mods.push('Ctrl');
    if (e.altKey) mods.push('Alt');
    if (e.shiftKey) mods.push('Shift');
    if (e.metaKey) mods.push('Win');
    return mods;
  }

  function start() {
    if (recording) return;
    recording = true;
    preview = [];
    message = '';
    send({ type: 'suspendHotkey' });
    button?.focus();
  }

  function stop(apply: string | null) {
    if (!recording) return;
    recording = false;
    preview = [];
    if (apply) send({ type: 'setHotkey', hotkey: apply });
    else send({ type: 'resumeHotkey' });
  }

  function onkeydown(e: KeyboardEvent) {
    if (!recording) return;
    e.preventDefault();
    e.stopPropagation();
    const mods = modifiers(e);
    if (e.key === 'Escape' && mods.length === 0) {
      stop(null);
      return;
    }
    if (['Control', 'Alt', 'Shift', 'Meta', 'OS'].includes(e.key)) {
      preview = mods;
      return;
    }
    const key = keyName(e.code);
    if (!key) {
      message = `${e.key} can't be used in a shortcut`;
      messageKind = 'error';
      return;
    }
    if (mods.length === 0) {
      preview = [key];
      message = 'Add Ctrl, Alt, Shift or Win';
      messageKind = 'error';
      return;
    }
    stop([...mods, key].join('+'));
  }

  function onkeyup(e: KeyboardEvent) {
    if (recording) preview = modifiers(e);
  }

  const off = on('hotkeyResult', (m) => {
    message = m.ok ? 'Shortcut updated' : m.error;
    messageKind = m.ok ? 'ok' : 'error';
    if (m.ok) setTimeout(() => (message === 'Shortcut updated' ? (message = '') : null), 2000);
  });
  onDestroy(() => {
    off();
    if (recording) send({ type: 'resumeHotkey' });
  });

  const keys = $derived(recording ? preview : hotkey.split('+'));
  const shownError = $derived(message ? '' : error);
</script>

<div class="recorder">
  <button
    bind:this={button}
    type="button"
    class="field"
    class:recording
    onclick={start}
    {onkeydown}
    {onkeyup}
    onblur={() => stop(null)}
    aria-label="Change shortcut"
  >
    {#if keys.length}
      {#each keys as key, i (i + key)}
        <kbd class="cap">{DISPLAY[key] ?? key}</kbd>
      {/each}
    {/if}
    {#if recording}
      <span class="hint">{keys.length ? '' : 'Press a shortcut…'}</span>
    {/if}
  </button>
  {#if message}
    <span class="msg {messageKind}">{message}</span>
  {:else if shownError}
    <span class="msg error">{shownError}</span>
  {/if}
</div>

<style>
  .recorder {
    display: flex;
    flex-direction: column;
    align-items: flex-end;
    gap: 4px;
  }
  .field {
    display: inline-flex;
    align-items: center;
    gap: 4px;
    min-width: 140px;
    min-height: 34px;
    padding-block: 5px;
    padding: 0 6px;
    justify-content: center;
    border: 0;
    border-radius: 9px;
    background: var(--fill);
    box-shadow: inset 0 0 0 0.5px var(--line-strong);
    transition:
      box-shadow 180ms,
      background 180ms;
  }
  .field:hover {
    background: var(--fill-hover);
  }
  .field.recording {
    box-shadow: inset 0 0 0 1.5px var(--accent);
    background: color-mix(in srgb, var(--accent) 10%, transparent);
    outline: none;
  }
  .cap {
    min-width: 24px;
    min-height: 24px;
    height: auto;
    padding: 0 7px;
    display: inline-grid;
    place-items: center;
    font-size: 0.85714rem;
    color: var(--fg);
    background: var(--surface-strong);
    box-shadow:
      inset 0 -1.5px 0 var(--line-strong),
      0 0 0 0.5px var(--line-strong);
    border-radius: 6px;
    animation: cap-in 220ms var(--ease-spring) both;
  }
  @keyframes cap-in {
    from {
      transform: scale(0.7);
      opacity: 0;
    }
  }
  .hint {
    font-size: 0.89286rem;
    color: var(--fg-3);
    padding: 0 6px;
  }
  .msg {
    font-size: 0.82143rem;
  }
  .msg.error {
    color: var(--danger);
  }
  .msg.ok {
    color: var(--success);
  }
</style>
