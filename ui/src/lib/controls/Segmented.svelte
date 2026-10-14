<script lang="ts" generics="T extends string">
  interface Option {
    value: T;
    label: string;
  }
  interface Props {
    value: T;
    options: Option[];
    label?: string;
    disabled?: boolean;
    onchange: (value: T) => void;
  }
  let { value, options, label = '', disabled = false, onchange }: Props = $props();

  let root = $state<HTMLDivElement>();
  let indicator = $state({ x: 0, w: 0, ready: false });

  // Slide a highlight under the selected segment.
  $effect(() => {
    void value;
    void options.length;
    if (!root) return;
    const el = root.querySelector<HTMLElement>('button[aria-checked="true"]');
    if (!el) return;
    const update = () => (indicator = { x: el.offsetLeft, w: el.offsetWidth, ready: true });
    update();
    const ro = new ResizeObserver(update);
    ro.observe(el);
    return () => ro.disconnect();
  });
</script>

<div class="segmented" role="radiogroup" aria-label={label} bind:this={root} class:disabled>
  <span
    class="indicator"
    class:ready={indicator.ready}
    style="transform:translateX({indicator.x}px);width:{indicator.w}px"
  ></span>
  {#each options as option (option.value)}
    <button
      type="button"
      role="radio"
      aria-checked={option.value === value}
      class:on={option.value === value}
      {disabled}
      onclick={() => onchange(option.value)}>{option.label}</button
    >
  {/each}
</div>

<style>
  .segmented {
    position: relative;
    display: inline-flex;
    padding: 2px;
    border-radius: 9px;
    background: var(--fill);
    box-shadow: inset 0 0 0 0.5px var(--line);
  }
  .segmented.disabled {
    opacity: 0.45;
  }
  .indicator {
    position: absolute;
    top: 2px;
    left: 0;
    bottom: 2px;
    border-radius: 7px;
    background: var(--surface-strong);
    box-shadow:
      0 1px 3px rgba(0, 0, 0, 0.16),
      inset 0 0 0 0.5px var(--line);
    opacity: 0;
  }
  .indicator.ready {
    opacity: 1;
    transition:
      transform 360ms var(--ease-spring),
      width 360ms var(--ease-spring);
  }
  button {
    position: relative;
    z-index: 1;
    border: 0;
    min-height: 26px;
    padding-block: 4px;
    padding: 0 12px;
    border-radius: 7px;
    background: transparent;
    font-size: 0.89286rem;
    color: var(--fg-2);
    white-space: nowrap;
    transition: color 160ms;
  }
  button.on {
    color: var(--fg);
    font-weight: 500;
  }
  button:not(:disabled):not(.on):hover {
    color: var(--fg);
  }
</style>
