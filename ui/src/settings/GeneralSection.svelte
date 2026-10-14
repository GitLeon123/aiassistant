<script lang="ts">
  import { send } from '../lib/bridge';
  import Row from '../lib/controls/Row.svelte';
  import Segmented from '../lib/controls/Segmented.svelte';
  import Toggle from '../lib/controls/Toggle.svelte';
  import { app } from '../lib/state.svelte';
  import type { Appearance, BackdropMode } from '../lib/types';
  import HotkeyRecorder from './HotkeyRecorder.svelte';
  import PositionPicker from './PositionPicker.svelte';

  const s = $derived(app.settings!);
</script>

<section>
  <h2>General</h2>
  <div class="group">
    <Row icon="keyboard" title="Shortcut" description="Opens the assistant from anywhere">
      <HotkeyRecorder hotkey={s.hotkey} error={app.payload?.hotkeyError ?? ''} />
    </Row>
    <Row icon="power" title="Start with Windows" description="Run quietly in the tray after you sign in">
      <Toggle
        checked={app.payload?.autostart ?? false}
        label="Start with Windows"
        onchange={(enabled) => {
          if (app.payload) app.payload.autostart = enabled;
          send({ type: 'setAutostart', enabled });
        }}
      />
    </Row>
  </div>

  <h3>Appearance</h3>
  <div class="group">
    <Row icon="eye" title="Solid surfaces" description="Opaque backgrounds with stronger text, control borders and focus rings">
      <Toggle checked={s.solidSurfaces} label="Solid surfaces" onchange={(v) => app.saveSettings({ solidSurfaces: v })} />
    </Row>
    <Row icon="eye" title="Text size" description="Applies to the bar and Settings, in addition to Windows scaling">
      <select aria-label="Text size" value={s.textScale} onchange={(e) => app.saveSettings({ textScale: Number(e.currentTarget.value) })}>
        <option value="100">100%</option><option value="125">125%</option><option value="150">150%</option><option value="200">200%</option>
      </select>
    </Row>
    <Row icon="window" title="Keep bar open" description="Keep the bar open when another app gets focus. Escape or your shortcut still closes it.">
      <Toggle checked={s.barPinned} label="Keep bar open" onchange={(v) => app.saveSettings({ barPinned: v })} />
    </Row>
    <Row icon="palette" title="Theme" description="System follows your Windows light/dark setting">
      <Segmented
        label="Theme"
        value={s.appearance}
        options={[
          { value: 'system', label: 'System' },
          { value: 'light', label: 'Light' },
          { value: 'dark', label: 'Dark' },
        ]}
        onchange={(v: Appearance) => app.saveSettings({ appearance: v })}
      />
    </Row>
    <Row
      icon="sparkle"
      title="Glass"
      description={s.backdrop === 'liquid'
        ? 'Liquid: refracted, blurred view of what is behind the bar'
        : 'System: Windows acrylic blur (lighter, no refraction)'}
    >
      <Segmented
        label="Glass style"
        value={s.backdrop}
        options={[
          { value: 'liquid', label: 'Liquid' },
          { value: 'system', label: 'System' },
        ]}
        onchange={(v: BackdropMode) => app.saveSettings({ backdrop: v })}
      />
    </Row>
    <Row icon="window" title="Bar position" description="Where the bar opens on the monitor under the cursor">
      <PositionPicker value={s.barPosition} onchange={(v) => app.saveSettings({ barPosition: v })} />
    </Row>
    {#if s.backdrop === 'liquid' && !s.solidSurfaces}
      <Row
        icon="refresh"
        title="Live backdrop"
        description={app.liveBackdrop
          ? 'Refresh the glass about once a second while the bar is open'
          : 'Unavailable while screen-capture exclusion is disabled'}
      >
        <Toggle
          checked={s.liveBackdrop && app.liveBackdrop}
          disabled={!app.liveBackdrop}
          label="Live backdrop"
          onchange={(v) => app.saveSettings({ liveBackdrop: v })}
        />
      </Row>
    {/if}
  </div>
</section>
