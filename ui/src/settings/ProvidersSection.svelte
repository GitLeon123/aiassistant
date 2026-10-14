<script lang="ts">
  import { app } from '../lib/state.svelte';
  import ProviderCard from './ProviderCard.svelte';
  import ConnectionsOverview from './ConnectionsOverview.svelte';

  const groups = $derived([
    {
      title: 'Subscriptions',
      hint: 'Use the plan you already pay for. Requires the provider’s official CLI, signed in.',
      items: app.providers.filter((p) => p.kind === 'cli'),
    },
    {
      title: 'API keys',
      hint: 'Pay per use. Keys are stored in Windows Credential Manager, never on disk or in the UI.',
      items: app.providers.filter((p) => p.kind === 'api'),
    },
    {
      title: 'Local',
      hint: 'Models served by Ollama or LM Studio. Use a localhost address to keep requests on this PC.',
      items: app.providers.filter((p) => p.kind === 'local'),
    },
  ]);

  let expanded = $state<Record<string, boolean>>({});
  function selectProvider(id: string) {
    expanded[id] = true;
    requestAnimationFrame(() => document.getElementById(`provider-${id}`)?.scrollIntoView({ block: 'start', behavior: app.reducedMotion ? 'instant' : 'smooth' }));
  }
</script>

<section>
  <h2>Providers</h2>
  <ConnectionsOverview onselect={selectProvider} />
  {#if app.enabledProviders.length === 0}
    <div class="banner">
      <strong>Connect a provider to get started.</strong>
      Sign in to a subscription CLI, or paste an API key below.
    </div>
  {/if}
  {#each groups as group (group.title)}
    {#if group.items.length}
      <h3>{group.title}</h3>
      <p class="hint">{group.hint}</p>
      <div class="cards">
        {#each group.items as provider (provider.id)}
          <div id="provider-{provider.id}"><ProviderCard
            {provider}
            bind:expanded={() => expanded[provider.id] ?? false, (v) => {
              expanded[provider.id] = v;
            }}
          /></div>
        {/each}
      </div>
    {/if}
  {/each}
</section>

<style>
  .cards {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  .hint {
    margin: -4px 0 10px;
    font-size: 0.85714rem;
    color: var(--fg-3);
  }
  .banner {
    padding: 12px 16px;
    border-radius: 12px;
    background: color-mix(in srgb, var(--accent) 14%, transparent);
    box-shadow: inset 0 0 0 0.5px color-mix(in srgb, var(--accent) 40%, transparent);
    font-size: 0.92857rem;
    color: var(--fg-2);
  }
  .banner strong {
    color: var(--fg);
    font-weight: 600;
    margin-right: 4px;
  }
</style>
