<script lang="ts">
  import { send } from '../lib/bridge';
  import Button from '../lib/controls/Button.svelte';
  import Row from '../lib/controls/Row.svelte';
  import Segmented from '../lib/controls/Segmented.svelte';
  import Toggle from '../lib/controls/Toggle.svelte';
  import Icon from '../lib/Icon.svelte';
  import { app } from '../lib/state.svelte';
  import type { CaptureRule, ScreenMode } from '../lib/types';

  const s = $derived(app.settings!);
  let process = $state('');
  let title = $state('');
  let label = $state('');
  let ruleError = $state('');
  function addRule() {
    if (!process.trim() && !title.trim()) { ruleError = 'Enter a process name or part of a window title.'; return false; }
    if (s.screenCaptureBlocklist.length >= 128) { ruleError = 'The blocklist can contain up to 128 rules. Remove a rule first.'; return false; }
    const rule: CaptureRule = { process: process.trim() || '*', title: title.trim(), label: label.trim() || 'Blocked window' };
    app.saveSettings({ screenCaptureBlocklist: [...s.screenCaptureBlocklist, rule] });
    process = ''; title = ''; label = ''; ruleError = '';
    return true;
  }
  const timeouts = [
    { value: '0', label: 'Every time' },
    { value: '5', label: '5 min' },
    { value: '10', label: '10 min' },
    { value: '30', label: '30 min' },
    { value: '60', label: '1 hour' },
  ];
  const timeoutValue = $derived(
    timeouts.some((t) => Number(t.value) === s.conversationTimeoutMinutes) ? String(s.conversationTimeoutMinutes) : '10',
  );
</script>

<section>
  <h2>Privacy</h2>
  <div class="group">
    <Row icon="screen" title="Share screen by default" description="Pre-select the Screen chip when the bar opens">
      <Toggle
        checked={s.screenContextDefault}
        label="Share screen by default"
        onchange={(v) => app.saveSettings({ screenContextDefault: v })}
      />
    </Row>
    <Row icon="window" title="Capture area" description="What the screenshot covers when screen sharing is on">
      <Segmented
        label="Capture area"
        value={s.screenMode}
        options={[
          { value: 'monitor', label: 'Whole screen' },
          { value: 'window', label: 'Active window' },
        ]}
        onchange={(v: ScreenMode) => app.saveSettings({ screenMode: v })}
      />
    </Row>
    <Row icon="refresh" title="Start a new conversation" description="When the bar opens after being idle for">
      <Segmented
        label="Conversation timeout"
        value={timeoutValue}
        options={timeouts}
        onchange={(v) => app.saveSettings({ conversationTimeoutMinutes: Number(v) })}
      />
    </Row>
  </div>

  <h3>Screen capture blocklist</h3>
  <div class="group">
    <Row icon="shield" title="Block sensitive windows" description="Check the active app before taking a screenshot or refreshing the glass backdrop">
      <Toggle checked={s.screenCaptureProtection} label="Block sensitive windows" onchange={(v) => app.saveSettings({ screenCaptureProtection: v })} />
    </Row>
    <div class="blocklist">
      <p>Password managers, remote desktop apps, and recognizable banking or private-browser titles are blocked by default. Title matching cannot detect every sensitive window, including private tabs without a visible title marker. Review your screenshot before sending.</p>
      {#if !s.screenCaptureProtection}<p class="protection-off" role="status">Protection is off. These rules will not prevent capture.</p>{/if}
      <ul class="rules" aria-label="Screen capture blocklist">
        {#each s.screenCaptureBlocklist as rule, i}
          <li>
            <div class="rule-text"><b>{rule.label}</b><span>{rule.process}{rule.title ? ` · title contains “${rule.title}”` : ' · any title'}</span></div>
            <button type="button" class="remove-rule" aria-label={`Remove rule ${i + 1}: ${rule.label}`} onclick={() => app.saveSettings({ screenCaptureBlocklist: s.screenCaptureBlocklist.filter((_, index) => index !== i) })}><Icon name="x" size={14} /></button>
          </li>
        {:else}
          <li>No windows are blocked. Add a rule or restore the defaults.</li>
        {/each}
      </ul>
      <form onsubmit={(e) => { e.preventDefault(); if (addRule()) e.currentTarget.reset(); }}>
        <label>Process name<input aria-label="Blocked process name" bind:value={process} placeholder="e.g. password-app.exe" maxlength="260" /></label>
        <label>Window title contains<input aria-label="Blocked window title" bind:value={title} placeholder="e.g. My bank" maxlength="256" /></label>
        <label>Reason shown in the bar<input aria-label="Block reason" bind:value={label} placeholder="e.g. Banking window" maxlength="120" /></label>
        <p class="rule-help">Process names accept * and ? wildcards. Matching ignores case. When both fields are filled, both must match.</p>
        {#if ruleError}<p class="rule-error" role="alert">{ruleError}</p>{/if}
        <div class="rule-actions">
          <Button type="submit" size="sm">Add rule</Button>
          <Button variant="ghost" size="sm" onclick={() => app.saveSettings({ screenCaptureBlocklist: app.payload!.defaultCaptureBlocklist })}>Restore defaults</Button>
        </div>
      </form>
    </div>
  </div>

  <h3>How your data is handled</h3>
  <ul class="facts">
    <li>
      <Icon name="eye" size={15} />
      <span>Review the screenshot before sending. The <b>Screen</b> chip attaches it to the next question. Earlier screenshots remain in the conversation until you start a new chat. Codex and Gemini use temporary image files, removed after the CLI run.</span>
    </li>
    <li>
      <Icon name="shield" size={15} />
      <span>The bar requests Windows capture exclusion. Support varies by capture tool and Windows version; this is not a security guarantee.</span>
    </li>
    <li>
      <Icon name="key" size={15} />
      <span>API keys are stored in Windows Credential Manager. They're never written to disk or shown again.</span>
    </li>
    <li>
      <Icon name="terminal" size={15} />
      <span>Subscriptions run through the official CLIs. AIAssistant never reads or stores their sign-in tokens.</span>
    </li>
    <li>
      <Icon name="cloud" size={15} />
      <span>AIAssistant keeps conversation history in memory until it resets. Provider CLIs may store their own data. Network and telemetry behavior still requires a packet-capture audit for each mode.</span>
    </li>
  </ul>

  <div class="actions">
    <Button size="sm" onclick={() => send({ type: 'openDataFolder' })}>
      <Icon name="folder" size={13} /> Open app data folder
    </Button>
  </div>
</section>

<style>
  .blocklist { padding: 0 16px 16px; }
  .blocklist p { margin: 0 0 12px; font-size: 0.92857rem; color: var(--fg-2); }
  .blocklist .protection-off { color: var(--warning); }
  .rules { list-style: none; margin: 0 0 16px; padding: 0; max-height: 240px; overflow-y: auto; }
  .rules li { display: flex; align-items: center; gap: 12px; padding: 9px 0; border-bottom: 1px solid var(--line); }
  .rule-text { display: flex; flex-direction: column; gap: 2px; min-width: 0; flex: 1; font-size: 0.92857rem; overflow-wrap: anywhere; }
  .rule-text b { font-weight: 600; }
  .rule-text span { color: var(--fg-2); font-size: 0.85714rem; }
  .remove-rule { display: grid; place-items: center; width: 30px; height: 30px; border: 0; border-radius: 8px; background: var(--fill); color: var(--fg-2); }
  .remove-rule:hover { background: var(--fill-hover); color: var(--fg); }
  form { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
  form label { display: flex; flex-direction: column; gap: 5px; font-size: 0.85714rem; color: var(--fg-2); }
  form label:nth-child(3), .rule-help, .rule-error, .rule-actions { grid-column: 1 / -1; }
  form input { min-width: 0; padding: 8px 10px; border: 1px solid var(--line-strong); border-radius: 6px; background: var(--fill); color: var(--fg); }
  form input::placeholder { color: var(--fg-2); opacity: 1; }
  .blocklist .rule-help { margin: 0; font-size: 0.85714rem; }
  .blocklist .rule-error { margin: 0; color: var(--danger); }
  .rule-actions { display: flex; gap: 8px; }
  .facts {
    list-style: none;
    margin: 0;
    padding: 6px 16px;
    border-radius: 12px;
    background: var(--surface);
    box-shadow: inset 0 0 0 0.5px var(--line-strong);
  }
  .facts li {
    display: flex;
    gap: 12px;
    padding: 10px 0;
    font-size: 0.92857rem;
    color: var(--fg-2);
  }
  .facts li + li {
    border-top: 1px solid var(--line);
  }
  .facts :global(.icon) {
    margin-top: 2px;
    color: var(--accent);
  }
  .actions {
    margin-top: 14px;
  }
</style>
