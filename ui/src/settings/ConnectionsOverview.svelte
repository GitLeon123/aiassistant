<script lang="ts">
  import { app } from '../lib/state.svelte';
  import { send } from '../lib/bridge';
  import Button from '../lib/controls/Button.svelte';
  import { connectionFor, connectionLabel } from '../lib/setup';
  let { onselect }: { onselect: (id: string) => void } = $props();
</script>

<div class="overview">
  <div class="heading"><h3>What’s connected</h3><Button size="sm" busy={app.payload?.discovery.running} onclick={() => send({ type: 'discoverProviders' })}>Check again</Button></div>
  <p class="hint">Saved credentials are ready to test. Connected providers have passed a connection or answer test.</p>
  <ul>
    {#each app.providers as provider (provider.id)}
      {@const connection = connectionFor(provider)}
      <li><button type="button" onclick={() => onselect(provider.id)}>
        <span class="name">{provider.name}</span>
        <span class="status">{connectionLabel(connection.state)}{app.isEnabled(provider) ? ' · Enabled' : ''}</span>
        <span class="next">{connection.nextStep || connection.detail}</span>
      </button></li>
    {/each}
  </ul>
</div>

<style>
  .overview { margin-bottom: 28px; }
  .heading { display: flex; justify-content: space-between; align-items: center; gap: 12px; }
  .heading h3 { margin: 0; }
  .hint { color: var(--fg-2); font-size: 0.85714rem; }
  ul { list-style: none; padding: 0; margin: 0; }
  li { border-bottom: 1px solid var(--line); }
  button { width: 100%; display: grid; grid-template-columns: minmax(110px, 1fr) minmax(150px, 1.6fr); gap: 2px 12px; padding: 10px 8px; border: 0; border-radius: 6px; background: transparent; text-align: left; }
  button:hover { background: var(--fill); }
  .name { font-size: 0.92857rem; font-weight: 500; }
  .status { font-size: 0.85714rem; text-align: right; color: var(--fg-2); }
  .next { grid-column: 1 / -1; font-size: 0.85714rem; color: var(--fg-2); overflow-wrap: anywhere; }
</style>
