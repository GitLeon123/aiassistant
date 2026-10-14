<script lang="ts">
  interface Props {
    checked: boolean;
    disabled?: boolean;
    label?: string;
    onchange: (checked: boolean) => void;
  }
  let { checked, disabled = false, label = '', onchange }: Props = $props();
</script>

<button
  type="button"
  role="switch"
  class="toggle"
  class:on={checked}
  aria-checked={checked}
  aria-label={label}
  {disabled}
  onclick={() => onchange(!checked)}
>
  <span class="knob"></span>
</button>

<style>
  .toggle {
    position: relative;
    flex: none;
    width: 40px;
    height: 24px;
    padding: 0;
    border: 0;
    border-radius: 11px;
    background: var(--fill-active);
    box-shadow: inset 0 0 0 1px var(--line);
    transition: background 220ms var(--ease-out);
  }
  .toggle.on {
    background: var(--accent);
    box-shadow: none;
  }
  .toggle:disabled {
    opacity: 0.4;
  }
  .knob {
    position: absolute;
    top: 4px;
    left: 3px;
    width: 16px;
    height: 16px;
    border-radius: 8px;
    background: #fff;
    box-shadow: 0 1px 3px rgba(0, 0, 0, 0.3);
    transition:
      transform 320ms var(--ease-spring),
      width 160ms var(--ease-out);
  }
  .on .knob {
    transform: translateX(18px);
  }
  .toggle:not(:disabled):active .knob {
    width: 20px;
  }
  .on:not(:disabled):active .knob {
    transform: translateX(14px);
  }
</style>
