<script lang="ts">
  import { app } from '../lib/state.svelte';
  import { send } from '../lib/bridge';
  import Row from '../lib/controls/Row.svelte';
  const settings = $derived(app.settings!);
  const budget = $derived(app.payload!.budget);
  function save(key: 'apiDailyBudgetUsd' | 'apiRequestAllowanceUsd', input: HTMLInputElement) {
    if (!input.checkValidity()) { input.reportValidity(); return; }
    app.saveSettings({ [key]: Number(input.value) });
  }
</script>

<section>
  <h2>Usage</h2>
  <h3>Subscription limits</h3>
  <p>CLI questions use your provider account and share its usage limits. Remaining quota is unknown unless the provider reports a warning. Check your provider’s usage page before a long session.</p>
  <p>Claude warnings are shown when its CLI reports them. In the provider’s own app, use <code>/usage</code> for Claude, <code>/status</code> for Codex, or <code>/stats model</code> for Gemini.</p>
  <h3>API spending guard</h3>
  <p>Reserve a fixed dollar allowance before each API question, retry or setup test. Requests stop when the next allowance would exceed the daily total. This is a planning guard, not a measurement or cap of your actual bill. Set billing limits with your provider for an actual spending cap.</p>
  <div class="group">
    <Row title="Daily allowance (USD)" description="0 turns the guard off. Resets at midnight on this PC.">
      <input aria-label="Daily API allowance (USD)" type="number" min="0" max="100000" step="0.01" value={settings.apiDailyBudgetUsd} onchange={(e) => save('apiDailyBudgetUsd', e.currentTarget)} />
    </Row>
    <Row title="Allowance per request (USD)" description="Choose a conservative amount for your model and conversation length. Failed or stopped requests keep their allowance.">
      <input aria-label="API allowance per request (USD)" type="number" min="0.01" max="100000" step="0.01" value={settings.apiRequestAllowanceUsd} onchange={(e) => save('apiRequestAllowanceUsd', e.currentTarget)} />
    </Row>
  </div>
  <p class="total" role="status">{budget.error || `Reserved today: $${budget.reservedUsd.toFixed(2)}${settings.apiDailyBudgetUsd ? ` of $${settings.apiDailyBudgetUsd.toFixed(2)}` : ' · guard off'}`}</p>
  <p>Loopback Ollama and LM Studio requests are exempt. Remote local-provider endpoints count toward the guard. The allowance record contains only the day and reserved total; it survives restarts.</p>
  <button type="button" class="folder" onclick={() => send({ type: 'openDataFolder' })}>Open data folder</button>
</section>

<style>
  p { color: var(--fg-2); overflow-wrap: anywhere; }
  input { width: 7rem; max-width: 100%; padding: 6px 8px; background: var(--surface-strong); border: 1px solid var(--line-strong); border-radius: 6px; }
  .total { color: var(--fg); font-variant-numeric: tabular-nums; }
  .folder { background: var(--fill); border: 1px solid var(--line-strong); border-radius: 6px; padding: 6px 10px; }
</style>
