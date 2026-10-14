<script lang="ts">
  import type { Snippet } from 'svelte';

  interface Props {
    variant?: 'primary' | 'secondary' | 'ghost' | 'danger';
    size?: 'sm' | 'md';
    disabled?: boolean;
    busy?: boolean;
    title?: string;
    type?: 'button' | 'submit';
    onclick?: (e: MouseEvent) => void;
    children: Snippet;
  }
  let {
    variant = 'secondary',
    size = 'md',
    disabled = false,
    busy = false,
    title,
    type = 'button',
    onclick,
    children,
  }: Props = $props();
</script>

<button {type} class="btn {variant} {size}" class:busy disabled={disabled || busy} {title} {onclick}>
  {#if busy}<span class="spinner" aria-hidden="true"></span>{/if}
  {@render children()}
</button>

<style>
  .btn {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    gap: 6px;
    min-height: 32px;
    padding: 0 14px;
    border: 0;
    border-radius: 8px;
    font-size: 0.92857rem;
    font-weight: 500;
    white-space: nowrap;
    transition:
      background 140ms,
      transform 200ms var(--ease-spring),
      opacity 140ms;
  }
  .sm {
    min-height: 28px;
    padding: 0 10px;
    font-size: 0.89286rem;
  }
  .btn:not(:disabled):active {
    transform: scale(0.96);
  }
  .btn:disabled {
    opacity: 0.5;
  }
  .busy:disabled {
    opacity: 0.8;
  }
  .primary {
    background: var(--accent);
    color: var(--accent-fg);
    box-shadow: inset 0 1px 0 rgba(255, 255, 255, 0.2);
  }
  .primary:not(:disabled):hover {
    background: color-mix(in srgb, var(--accent) 88%, white);
  }
  .secondary {
    background: var(--surface-strong);
    color: var(--fg);
    box-shadow:
      inset 0 0 0 0.5px var(--line-strong),
      0 1px 2px rgba(0, 0, 0, 0.08);
  }
  .secondary:not(:disabled):hover {
    background: color-mix(in srgb, var(--surface-strong) 80%, var(--fg) 6%);
  }
  .ghost {
    background: transparent;
    color: var(--fg-2);
  }
  .ghost:not(:disabled):hover {
    background: var(--fill-hover);
    color: var(--fg);
  }
  .danger {
    background: transparent;
    color: var(--danger);
  }
  .danger:not(:disabled):hover {
    background: color-mix(in srgb, var(--danger) 12%, transparent);
  }
  .spinner {
    width: 12px;
    height: 12px;
    border-radius: 50%;
    border: 1.5px solid currentColor;
    border-right-color: transparent;
    animation: spin 0.7s linear infinite;
  }
  @keyframes spin {
    to {
      transform: rotate(360deg);
    }
  }
</style>
