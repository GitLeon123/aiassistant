<script lang="ts">
  import type { Snippet } from 'svelte';
  import Icon, { type IconName } from '../Icon.svelte';

  interface Props {
    title: string;
    description?: string;
    icon?: IconName;
    children?: Snippet;
    below?: Snippet;
  }
  let { title, description = '', icon, children, below }: Props = $props();
</script>

<div class="row">
  {#if icon}<span class="icon"><Icon name={icon} size={18} /></span>{/if}
  <div class="text">
    <div class="title">{title}</div>
    {#if description}<div class="desc">{description}</div>{/if}
  </div>
  <div class="control">{@render children?.()}</div>
</div>
{#if below}<div class="below">{@render below()}</div>{/if}

<style>
  .row {
    display: flex;
    align-items: center;
    gap: 14px;
    flex-wrap: wrap;
    min-height: 62px;
    padding: 12px 16px;
  }
  .row:not(:first-child) {
    border-top: 1px solid var(--line);
  }
  .icon {
    flex: none;
    display: grid;
    place-items: center;
    width: 32px;
    height: 32px;
    border-radius: 9px;
    background: var(--fill);
    color: var(--fg-2);
  }
  .text {
    flex: 1 1 160px;
    min-width: 0;
  }
  .title {
    font-size: 0.96429rem;
  }
  .desc {
    margin-top: 1px;
    font-size: 0.85714rem;
    color: var(--fg-3);
  }
  .control {
    display: flex;
    align-items: center;
    gap: 8px;
    max-width: 100%;
    flex-wrap: wrap;
  }
  .below {
    padding: 0 16px 14px;
  }
</style>
