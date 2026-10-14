<script lang="ts">
  import { send } from '../lib/bridge';
  import Button from '../lib/controls/Button.svelte';
  import Icon, { type IconName } from '../lib/Icon.svelte';
  import { setReducedMotion } from '../lib/motion';
  import { app, applyTheme } from '../lib/state.svelte';
  import GeneralSection from './GeneralSection.svelte';
  import ModelSection from './ModelSection.svelte';
  import PrivacySection from './PrivacySection.svelte';
  import ProvidersSection from './ProvidersSection.svelte';
  import SetupSection from './SetupSection.svelte';
  import UsageSection from './UsageSection.svelte';

  type Page = 'setup' | 'general' | 'providers' | 'model' | 'privacy' | 'usage' | 'about';
  const NAV: { id: Page; label: string; icon: IconName }[] = [
    { id: 'setup', label: 'Setup assistant', icon: 'sparkle' },
    { id: 'general', label: 'General', icon: 'gear' },
    { id: 'providers', label: 'Providers', icon: 'cloud' },
    { id: 'model', label: 'Model', icon: 'brain' },
    { id: 'privacy', label: 'Privacy', icon: 'shield' },
    { id: 'usage', label: 'Usage', icon: 'info' },
    { id: 'about', label: 'About', icon: 'info' },
  ];

  let page = $state<Page>('general');
  let chosenInitial = false;
  let content = $state<HTMLElement>();

  $effect(() => {
    applyTheme(app.dark, app.theme);
    setReducedMotion(app.reducedMotion);
  });

  // First run: land on Providers until something is connected.
  $effect(() => {
    if (!app.ready || chosenInitial) return;
    chosenInitial = true;
    if (!app.settings?.setupCompleted) page = 'setup';
  });

  function go(p: Page) {
    page = p;
    content?.scrollTo({ top: 0 });
  }
</script>

<div class="shell">
  <nav aria-label="Settings">
    <div class="brand">
      <span class="logo"><Icon name="sparkle" size={18} fill /></span>
      <span>AIAssistant</span>
    </div>
    {#each NAV as item (item.id)}
      <button type="button" class="nav-item" class:active={page === item.id} aria-current={page === item.id ? 'page' : undefined} onclick={() => go(item.id)}>
        <Icon name={item.icon} size={16} />
        <span>{item.label}</span>
      </button>
    {/each}
    <div class="nav-foot">
      {#if app.settings}
        <span class="hint">Press <kbd>{app.settings.hotkey.replaceAll('+', ' + ')}</kbd> to ask</span>
      {/if}
    </div>
  </nav>

  <main bind:this={content}>
    {#if !app.ready}
      <div class="loading">Loading…</div>
    {:else}
      {#key page}
        <div class="page">
          {#if page === 'setup'}
            <SetupSection oncomplete={() => go('general')} gotoProviders={() => go('providers')} />
          {:else if page === 'general'}
            <GeneralSection />
          {:else if page === 'providers'}
            <ProvidersSection />
          {:else if page === 'model'}
            <ModelSection gotoProviders={() => go('providers')} />
          {:else if page === 'privacy'}
            <PrivacySection />
          {:else if page === 'usage'}
            <UsageSection />
          {:else}
            <section>
              <h2>About</h2>
              <div class="about">
                <span class="about-logo"><Icon name="sparkle" size={34} fill /></span>
                <div>
                  <div class="about-name">AIAssistant</div>
                  <div class="about-ver">Version {app.version}</div>
                </div>
              </div>
              <p class="about-text">
                A quick, private AI assistant for Windows. Press your shortcut anywhere to ask a question, optionally
                with your screen as context.
              </p>
              <div class="about-actions">
                <Button variant="danger" onclick={() => send({ type: 'quit' })}>
                  <Icon name="power" size={14} /> Quit AIAssistant
                </Button>
              </div>
            </section>
          {/if}
        </div>
      {/key}
    {/if}
  </main>
</div>

<style>
  .shell {
    display: grid;
    grid-template-columns: minmax(160px, 15rem) minmax(0, 1fr);
    height: 100vh;
  }
  nav {
    display: flex;
    flex-direction: column;
    gap: 2px;
    padding: 16px 10px 14px;
  }
  .brand {
    display: flex;
    align-items: center;
    gap: 10px;
    padding: 4px 10px 18px;
    font-family: var(--font-display);
    font-size: 1.0714rem;
    font-weight: 600;
  }
  .logo {
    display: grid;
    place-items: center;
    width: 30px;
    height: 30px;
    border-radius: 9px;
    color: #fff;
    background: linear-gradient(150deg, color-mix(in srgb, var(--accent) 70%, white), var(--accent));
    box-shadow:
      inset 0 1px 0 rgba(255, 255, 255, 0.4),
      0 4px 12px -4px color-mix(in srgb, var(--accent) 70%, transparent);
  }
  .nav-item {
    position: relative;
    display: flex;
    align-items: center;
    gap: 12px;
    min-height: 36px;
    padding-block: 6px;
    padding: 0 12px;
    border: 0;
    border-radius: 8px;
    background: transparent;
    color: var(--fg-2);
    font-size: 0.96429rem;
    text-align: left;
    transition:
      background 140ms,
      color 140ms;
  }
  .nav-item:hover {
    background: var(--fill);
    color: var(--fg);
  }
  .nav-item.active {
    background: var(--fill-hover);
    color: var(--fg);
  }
  .nav-item.active::before {
    content: '';
    position: absolute;
    left: 0;
    top: 10px;
    bottom: 10px;
    width: 3px;
    border-radius: 2px;
    background: var(--accent);
    animation: bar-in 260ms var(--ease-spring) both;
  }
  @keyframes bar-in {
    from {
      transform: scaleY(0);
    }
  }
  .nav-foot {
    margin-top: auto;
    padding: 0 10px;
  }
  .hint {
    font-size: 0.82143rem;
    color: var(--fg-3);
  }
  main {
    min-width: 0;
    overflow-y: auto;
    padding: 18px 28px 40px 12px;
  }
  .page {
    max-width: 720px;
    animation: page-in 300ms var(--ease-out) both;
  }
  @keyframes page-in {
    from {
      opacity: 0;
      transform: translateY(6px);
    }
  }
  :global(:root[data-motion='reduced']) .page {
    animation: none;
  }
  .loading {
    padding: 40px;
    color: var(--fg-3);
  }

  /* Shared section styles */
  .page :global(h2) {
    margin: 4px 0 16px;
    font-family: var(--font-display);
    font-size: 1.8571rem;
    font-weight: 600;
    letter-spacing: -0.015em;
  }
  .page :global(h3) {
    margin: 26px 0 8px;
    font-size: 0.92857rem;
    font-weight: 600;
    color: var(--fg-2);
  }
  .page :global(.group) {
    border-radius: 12px;
    background: var(--surface);
    box-shadow:
      inset 0 0 0 0.5px var(--line-strong),
      0 1px 2px rgba(0, 0, 0, 0.05);
  }

  .about {
    display: flex;
    align-items: center;
    gap: 16px;
  }
  .about-logo {
    display: grid;
    place-items: center;
    width: 64px;
    height: 64px;
    border-radius: 18px;
    color: #fff;
    background: linear-gradient(150deg, color-mix(in srgb, var(--accent) 65%, white), var(--accent));
    box-shadow:
      inset 0 1px 0 rgba(255, 255, 255, 0.4),
      0 8px 24px -8px color-mix(in srgb, var(--accent) 80%, transparent);
  }
  .about-name {
    font-family: var(--font-display);
    font-size: 1.4286rem;
    font-weight: 600;
  }
  .about-ver {
    color: var(--fg-3);
    font-size: 0.89286rem;
    user-select: text;
    -webkit-user-select: text;
  }
  .about-text {
    max-width: 520px;
    color: var(--fg-2);
    font-size: 0.92857rem;
  }
  .about-actions {
    margin-top: 18px;
  }
  @media (max-width: 680px) {
    .shell { grid-template-columns: minmax(0, 1fr); grid-template-rows: auto minmax(0, 1fr); }
    nav { flex-direction: row; flex-wrap: wrap; padding: 8px; }
    .brand, .nav-foot { display: none; }
    .nav-item { height: auto; }
    main { padding: 12px 16px 32px; }
  }
</style>
