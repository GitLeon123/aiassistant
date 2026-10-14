<script lang="ts">
  import { onDestroy } from 'svelte';
  import { on, send } from '../lib/bridge';
  import Button from '../lib/controls/Button.svelte';
  import { app } from '../lib/state.svelte';
  import { connectionFor, connectionLabel, setupModel } from '../lib/setup';
  import { destination as classifyDestination } from '../lib/privacy';
  import ProviderCard from './ProviderCard.svelte';
  import HotkeyRecorder from './HotkeyRecorder.svelte';

  let { oncomplete, gotoProviders }: { oncomplete: () => void; gotoProviders: () => void } = $props();
  let step = $state(1);
  let selected = $state('');
  let model = $state('');
  let userSelected = false;
  let expanded = $state(false);
  let busy = $state(false);
  let finishing = $state(false);
  let requestId = $state('');
  let answer = $state('');
  let error = $state('');
  let passed = $state(false);
  const provider = $derived(app.provider(selected));
  const listing = $derived(app.modelsFor(selected));
  const discovery = $derived(app.payload?.discovery);
  const suggested = $derived(discovery?.suggestedProvider ?? '');
  const candidates = $derived(app.providers.filter(p => p.kind === 'cli' || p.kind === 'local' || p.hasKey));
  const destination = $derived(classifyDestination(provider, app.config(selected)));

  $effect(() => {
    if (!userSelected && discovery?.complete && suggested) selected = suggested;
  });
  $effect(() => {
    if (selected) app.requestModels(selected);
  });
  $effect(() => {
    if (provider && listing.list.length && !listing.list.some(m => m.id === model)) model = setupModel(provider, listing.list);
  });

  function choose(id: string) {
    if (busy) send({ type: 'cancelSetupTest' });
    userSelected = true;
    selected = id;
    model = '';
    requestId = '';
    passed = false;
    answer = error = '';
    busy = false;
    const chosen = app.provider(id);
    expanded = !!chosen && !['connected', 'ready'].includes(connectionFor(chosen).state);
  }
  function test() {
    if (!selected || !model) return;
    requestId = crypto.randomUUID();
    busy = true;
    passed = false;
    answer = error = '';
    send({ type: 'testSetup', id: requestId, provider: selected, model });
  }
  const offTest = on('setupTestResult', m => {
    if (m.id !== requestId || m.provider !== selected || m.model !== model) return;
    busy = false;
    passed = m.ok;
    answer = m.answer;
    error = m.error;
  });
  const offComplete = on('setupComplete', m => {
    finishing = false;
    if (m.ok) oncomplete();
    else error = m.error;
  });
  onDestroy(() => {
    offTest(); offComplete();
    if (busy) send({ type: 'cancelSetupTest' });
  });
</script>

<section class="setup">
  <h2>Let’s get you connected</h2>
  <p class="intro">Choose an AI, try one question, and set your shortcut.</p>
  <ol class="progress" aria-label="Setup progress">
    {#each ['Choose an AI', 'Try a question', 'Set your shortcut'] as label, i}
      <li class:current={step === i + 1} class:done={step > i + 1} aria-current={step === i + 1 ? 'step' : undefined}>{i + 1}. {label}</li>
    {/each}
  </ol>

  {#if step === 1}
    <div class="section-head">
      <h3>Found on this PC</h3>
      <div class="header-actions">
        <Button size="sm" busy={discovery?.running} onclick={() => send({ type: 'discoverProviders' })}>Check again</Button>
        <Button variant="primary" disabled={!provider || !model || listing.loading || (provider.kind === 'cli' && !provider.cli?.installed) || (provider.needsKey && !provider.keyOptional && !provider.hasKey)} onclick={() => { step = 2; passed = false; answer = error = ''; }}>Continue</Button>
      </div>
    </div>
    <p class="muted" role="status">{discovery?.running ? 'Checking saved sign-ins and local model servers…' : suggested ? 'A ready provider is suggested below. You can choose another.' : 'Nothing ready yet. Choose a provider below to see the next step.'}</p>
    <div class="choices" role="group" aria-label="Choose a provider">
      {#each candidates as p (p.id)}
        {@const status = connectionFor(p)}
        <button type="button" class="choice" class:selected={selected === p.id} aria-pressed={selected === p.id} onclick={() => choose(p.id)}>
          <span><strong>{p.name}</strong><span class="kind">{p.kind === 'local' ? 'Local server' : p.kind === 'cli' ? 'Existing subscription' : 'API key'}</span></span>
          <span class="choice-status">{p.id === suggested ? 'Suggested · ' : ''}{connectionLabel(status.state)}</span>
        </button>
      {/each}
    </div>
    <label class="other">Use an API key or another provider
      <select value={selected} onchange={e => choose(e.currentTarget.value)}>
        <option value="">Choose a provider…</option>
        {#each app.providers as p (p.id)}<option value={p.id}>{p.name}{p.kind === 'api' ? ' · API key' : ''}</option>{/each}
      </select>
    </label>
    {#if provider}
      {#key selected}<ProviderCard {provider} bind:expanded />{/key}
      <div class="actions">
        <span class="muted">{listing.loading ? 'Loading models…' : !model ? 'Connect this provider and load a model to continue.' : ''}</span>
      </div>
    {/if}
    <button class="link skip" type="button" onclick={gotoProviders}>I’ll set it up in Settings</button>
  {:else if step === 2 && provider}
    <h3>Confirm {provider.name} can answer</h3>
    <label class="other">Model
      <select bind:value={model} disabled={busy} onchange={() => { passed = false; answer = error = ''; }}>
        {#each listing.list as m (m.id)}<option value={m.id}>{m.name}</option>{/each}
      </select>
    </label>
    <p class="muted">Test → {provider.name} ({destination.scope}). This uses your plan or API credit. Your screen and chat history are not sent.</p>
    <div class="question">Say hello in one short sentence.</div>
    <div class="actions">
      <Button variant="primary" busy={busy} onclick={test}>{passed ? 'Test again' : 'Send test question'}</Button>
      {#if busy}<Button onclick={() => send({ type: 'cancelSetupTest' })}>Stop test</Button>{/if}
    </div>
    <div aria-live="polite" aria-atomic="true">
      {#if busy}<p class="muted">Waiting for an answer…</p>{/if}
      {#if passed}<p class="success">Connected. {provider.name} answered:</p><blockquote>{answer}</blockquote>{/if}
    </div>
    <div class="actions">
      <Button disabled={busy} onclick={() => { step = 1; passed = false; }}>Back</Button>
      <Button variant="primary" disabled={!passed || busy} onclick={() => { step = 3; error = ''; }}>Continue to shortcut</Button>
    </div>
  {:else if step === 3 && app.settings}
    <h3>Open the assistant from any app</h3>
    <p>Press <kbd>{app.settings.hotkey.replaceAll('+', ' + ')}</kbd> to ask a question.</p>
    {#if app.settings.hotkey === 'Alt+Space'}
      <p class="shortcut-warning">Alt + Space is also used by the Windows system menu, PowerToys Run and Flow Launcher. We suggest Ctrl + Alt + Space to avoid interfering with them.</p>
    {/if}
    <HotkeyRecorder hotkey={app.settings.hotkey} error={app.payload?.hotkeyError ?? ''} />
    <div class="suggestions" role="group" aria-label="Suggested shortcuts">
      {#each app.payload?.hotkeySuggestions ?? [] as suggestion}
        <Button disabled={!suggestion.available} onclick={() => send({ type: 'setHotkey', hotkey: suggestion.hotkey })}>{suggestion.hotkey.replaceAll('+', ' + ')}{suggestion.available ? '' : ' (in use)'}</Button>
      {/each}
    </div>
    <p class="muted">Windows checked these shortcuts. You can change yours later in General.</p>
    <div class="actions">
      <Button disabled={finishing} onclick={() => { step = 2; error = ''; }}>Back</Button>
      <Button variant="primary" busy={finishing} disabled={!passed || !!app.payload?.hotkeyError} onclick={() => { finishing = true; send({ type: 'completeSetup', id: requestId }); }}>Finish setup</Button>
    </div>
  {/if}
  {#if error}<p class="error" role="alert">{error}</p>{/if}
</section>

<style>
  .intro, .muted { color: var(--fg-2); }
  .intro { margin: -8px 0 24px; }
  .muted { font-size: 0.92857rem; }
  .progress { display: flex; flex-wrap: wrap; gap: 16px; list-style: none; padding: 0 0 16px; margin: 0 0 24px; border-bottom: 1px solid var(--line); color: var(--fg-2); font-size: 0.92857rem; }
  .progress .current { color: var(--fg); font-weight: 600; }
  .progress .done { color: var(--fg); }
  .section-head { display: flex; align-items: center; justify-content: space-between; gap: 12px; }
  .section-head h3 { margin: 0; }
  .header-actions { display: flex; flex-wrap: wrap; align-items: center; gap: 8px; }
  .choices { display: flex; flex-direction: column; margin-bottom: 24px; }
  .choice { display: flex; justify-content: space-between; align-items: center; gap: 16px; min-height: 54px; width: 100%; padding: 8px 12px; text-align: left; border: 0; border-bottom: 1px solid var(--line); background: transparent; border-radius: 6px; }
  .choice:hover { background: var(--fill); }
  .choice.selected { background: var(--fill-hover); box-shadow: inset 0 0 0 1.5px var(--accent); }
  .choice strong { font-weight: 600; }
  .kind { display: block; color: var(--fg-2); font-size: 0.85714rem; }
  .choice-status { color: var(--fg-2); font-size: 0.85714rem; text-align: right; }
  .other { display: grid; gap: 6px; margin-bottom: 20px; font-size: 0.92857rem; color: var(--fg-2); }
  select { min-height: 36px; width: 100%; padding: 6px 10px; border: 1px solid var(--line-strong); border-radius: 8px; background: var(--surface-strong); color: var(--fg); }
  .actions, .suggestions { display: flex; align-items: center; flex-wrap: wrap; gap: 10px; margin-top: 20px; }
  .question { padding: 16px; background: var(--fill); border-radius: 12px; margin: 20px 0; }
  blockquote { margin: 0 0 24px; white-space: pre-wrap; overflow-wrap: anywhere; user-select: text; }
  .success { color: var(--fg); font-weight: 600; }
  .error { color: var(--danger); }
  .shortcut-warning { color: var(--fg); background: var(--fill); border-radius: 12px; padding: 14px; }
  .link { border: 0; background: transparent; color: var(--fg-2); padding: 0; text-decoration: underline; text-underline-offset: 3px; }
  .skip { margin-top: 24px; }
</style>
