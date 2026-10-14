<script lang="ts">
  import { onMount, tick } from 'svelte';
  import { on, send } from '../lib/bridge';
  import { copyToHost } from '../lib/clipboard';
  import { layoutRect, uid, union, type Rect } from '../lib/dom';
  import Glass from '../lib/glass/Glass.svelte';
  import Icon from '../lib/Icon.svelte';
  import { clamp, rafThrottle, reducedMotion, setReducedMotion, Spring, springs } from '../lib/motion';
  import { app, applyTheme, levelFor, levelLabel } from '../lib/state.svelte';
  import type { ModelChoice, ScreenInfo, Usage } from '../lib/types';
  import ModelPicker from './ModelPicker.svelte';
  import type { Turn } from './turn';
  import TurnView from './TurnView.svelte';

  // Geometry (CSS px). The native window is a fixed 760x680 DIP canvas; the panel's top edge
  // sits at kBarPanelTopDip (36) so the host can place it at 20% of the work area.
  const PANEL_TOP = 36;
  const PANEL_BOTTOM = 36; // Gap below the panel for the bottom row, mirroring PANEL_TOP
  const PANEL_WIDTH = 680;
  const RADIUS = 26;
  const SHADOW_PAD = 26;

  // Which vertical edge the panel is attached to. The panel grows away from it.
  let barVertical = $state<'top' | 'middle' | 'bottom'>('top');

  let turns = $state<Turn[]>([]);
  let draft = $state('');
  let open = $state(false);
  let backdrop = $state('');
  let screen = $state<ScreenInfo>({ available: false, title: '', app: '', hasWindow: false, mode: 'monitor', thumb: '', id: 0, monitor: 0, width: 0, height: 0, status: 'blocked', reason: '' });
  let screenOn = $state(false);
  let previewReady = $state(false);
  let pickerOpen = $state(false);
  let pickerPos = $state({ x: 0, y: 0 });
  let announcement = $state('');
  let canvasHeight = $state(window.innerHeight);
  let canvasWidth = $state(window.innerWidth);

  let stage = $state<HTMLDivElement>();
  let panel = $state<HTMLDivElement>();
  let inner = $state<HTMLDivElement>();
  let input = $state<HTMLTextAreaElement>();
  let scroller = $state<HTMLDivElement>();
  let modelChip = $state<HTMLButtonElement>();
  let pickerEl = $state<HTMLDivElement>();

  const streaming = $derived(turns.at(-1)?.state === 'streaming');
  const choice = $derived(app.defaultChoice);
  const model = $derived(choice ? app.findModel(choice.provider, choice.model) : undefined);
  const level = $derived(levelFor(model, choice?.reasoning ?? ''));
  const providerName = $derived(choice ? (app.provider(choice.provider)?.name ?? choice.provider) : '');
  const budgetNearLimit = $derived(!!app.settings?.apiDailyBudgetUsd && (app.payload?.budget.reservedUsd ?? 0) >= app.settings!.apiDailyBudgetUsd * 0.8);
  const modelLabel = $derived(choice ? (model?.name ?? choice.model) : 'Choose a model');
  const canShare = $derived(screen.available && model?.vision !== false);
  const canSend = $derived(!!draft.trim() && (!screenOn || (previewReady && canShare)));
  const glassMode = $derived<'liquid' | 'system' | 'solid'>(
    app.settings?.solidSurfaces ? 'solid' : app.settings?.backdrop === 'system' ? 'system' : backdrop ? 'liquid' : 'solid',
  );
  const screenLabel = $derived(
    screen.mode === 'window' && screen.hasWindow ? screen.app.replace(/\.exe$/i, '') || 'Window' : 'Screen',
  );

  $effect(() => {
    applyTheme(app.dark, app.theme);
    setReducedMotion(app.reducedMotion);
  });

  // Make sure the selected provider's model list (capabilities, reasoning levels) is loaded.
  $effect(() => {
    if (app.ready && choice) app.requestModels(choice.provider);
  });
  $effect(() => {
    if (model?.vision === false) screenOn = false;
  });
  // Turning Screen on is the review: confirm the frozen capture to the host without showing it.
  $effect(() => {
    if (screenOn && screen.available && !previewReady) {
      previewReady = true;
      send({ type: 'screenPreviewReady', screenId: screen.id });
    }
  });

  // Let the host warm up whatever the next question will use (start the CLI, open the connection,
  // encode the screenshot) while the user is still typing.
  $effect(() => {
    if (!open || !app.ready || !choice) return;
    send({
      type: 'prepare',
      provider: choice.provider,
      model: choice.model,
      reasoning: level,
      screen: screenOn && screen.available,
    });
  });

  // ---- Panel height morph ---------------------------------------------------------------
  let panelHeight = 0;
  let targetHeight = 0;
  const heightSpring = new Spring(64, springs.layout, (v) => {
    panelHeight = v;
    if (panel) panel.style.height = `${v}px`;
    placeStage();
    syncRegion();
  });

  // Top of the panel inside the canvas for a given panel height. Top keeps the panel hanging from
  // PANEL_TOP; middle centres it; bottom keeps its lower edge at PANEL_BOTTOM from the canvas bottom.
  function stageTop(h: number) {
    if (barVertical === 'bottom') return Math.max(8, canvasHeight - PANEL_BOTTOM - h);
    if (barVertical === 'middle') return Math.max(8, (canvasHeight - h) / 2);
    return Math.min(PANEL_TOP, Math.max(8, canvasHeight - h - 8));
  }
  function placeStage() {
    if (!stage) return;
    stage.style.top = `${stageTop(panelHeight)}px`;
    stage.style.transformOrigin = barVertical === 'bottom' ? '50% 100%' : '50% 0';
  }

  function measureInner() {
    if (!inner) return;
    const h = inner.offsetHeight;
    if (Math.abs(h - targetHeight) < 0.5) return;
    targetHeight = h;
    if (!open || heightSpring.value === 0) heightSpring.jump(h);
    else heightSpring.set(h, springs.layout);
    syncRegion();
  }

  // ---- Open / close ----------------------------------------------------------------------
  const openSpring = new Spring(0, springs.open, (p, v) => {
    if (!stage) return;
    stage.style.opacity = String(clamp(p * 1.8, 0, 1));
    if (p === 1 && v === 0) {
      // At rest: drop the transform and the layer hint so text is rasterized at full
      // resolution (a layer promoted mid-scale keeps its scaled raster and looks soft).
      stage.style.transform = '';
      stage.style.filter = '';
      stage.style.willChange = '';
      return;
    }
    const q = clamp(p, 0, 1.2);
    stage.style.willChange = 'transform, opacity';
    stage.style.transform = `translateY(${(1 - q) * 10}px) scale(${0.94 + 0.06 * q})`;
    const blur = Math.max(0, (1 - p) * 10);
    stage.style.filter = blur > 0.2 ? `blur(${blur}px)` : '';
  });
  let closeAnim: Animation | null = null;

  function animateOpen() {
    closeAnim?.cancel();
    closeAnim = null;
    if (!stage) return;
    if (reducedMotion()) {
      stage.style.transform = '';
      stage.style.filter = '';
      stage.animate([{ opacity: 0 }, { opacity: 1 }], { duration: 120, easing: 'linear' });
      stage.style.opacity = '1';
      return;
    }
    openSpring.jump(0);
    openSpring.set(1, springs.open);
  }

  function close(restoreFocus: boolean) {
    if (!open) return;
    open = false;
    announcement = '';
    pickerOpen = false;
    screenOn = false;
    previewReady = false;
    screen = { ...screen, available: false, thumb: '', reason: '' };
    backdrop = '';
    openSpring.stop();
    if (!stage) {
      send({ type: 'hidden', restoreFocus });
      return;
    }
    const from = { opacity: stage.style.opacity || '1', transform: stage.style.transform || 'none' };
    closeAnim = stage.animate(
      [
        { ...from, filter: 'blur(0px)' },
        reducedMotion()
          ? { opacity: 0, transform: from.transform, filter: 'blur(0px)' }
          : { opacity: 0, transform: 'translateY(4px) scale(0.965)', filter: 'blur(6px)' },
      ],
      { duration: reducedMotion() ? 90 : 150, easing: 'cubic-bezier(0.55, 0, 0.75, 0.2)', fill: 'forwards' },
    );
    closeAnim.onfinish = () => {
      if (!open && stage) {
        stage.style.opacity = '0';
        send({ type: 'hidden', restoreFocus });
      }
    };
  }

  on('show', async (m) => {
    backdrop = m.backdrop;
    setScreen(m.screen);
    screenOn = m.screenDefault && canShare;
    if (m.conversationReset) turns = [];
    pickerOpen = false;
    barVertical = m.position.split('-')[0] as typeof barVertical;
    open = true;
    await tick();
    measureInner();
    heightSpring.jump(targetHeight);
    placeStage();
    animateOpen();
    focusInput(true);
  });
  on('dismiss', () => close(true));
  on('backdrop', (m) => {
    if (open) backdrop = m.url;
  });
  on('screen', (m) => {
    setScreen(m.screen);
    if (!m.screen.available) screenOn = false;
  });

  function setScreen(next: ScreenInfo) {
    screen = next;
    previewReady = false;
  }

  // ---- Streaming -------------------------------------------------------------------------
  function turnById(id: string) {
    return turns.find((t) => t.id === id);
  }
  on('delta', (m) => {
    const t = turnById(m.id);
    if (!t || t.state !== 'streaming') return;
    if ((m.text || m.reasoning) && !t.firstTokenAt) t.firstTokenAt = performance.now();
    if (m.reasoning) t.reasoning += m.reasoning;
    if (m.status) t.status = m.status;
    if (m.limitWarning !== undefined) {
      t.limitWarning = m.limitWarning;
      if (m.limitWarning) announcement = m.limitWarning;
    }
    if (m.text) {
      if (!t.answeredAt) t.answeredAt = performance.now();
      t.answer += m.text;
    }
  });
  on('done', (m) => {
    const t = turnById(m.id);
    if (!t) return;
    t.finishedAt = performance.now();
    t.state = m.ok ? 'done' : m.cancelled ? 'cancelled' : 'error';
    t.error = m.error ?? '';
    if (m.limitWarning) t.limitWarning = m.limitWarning;
    t.outputTokens = outputTokens(m.usage);
    t.streamed = m.streamed !== false;
    announcement = m.ok ? 'Answer ready.' : m.cancelled ? 'Answer stopped.' : `Answer failed. ${t.error || 'Try again.'}`;
  });
  on('copyResult', (m) => { announcement = m.ok ? 'Copied to clipboard.' : 'Copy failed. Try again.'; });

  // Providers name the generated-token count differently (Anthropic/Codex, OpenAI-compatible, Gemini).
  function outputTokens(usage: Usage | null | undefined) {
    if (!usage) return 0;
    const n = Number(usage.output_tokens ?? usage.completion_tokens ?? 0) ||
      Number(usage.candidatesTokenCount ?? 0) + Number(usage.thoughtsTokenCount ?? 0);
    return Number.isFinite(n) ? n : 0;
  }

  function submit(text = draft.trim(), withScreen = screenOn && screen.available) {
    if (!text) return;
    if (withScreen && (!previewReady || !canShare)) {
      screenOn = canShare;
      return;
    }
    const current = turns.at(-1);
    // A new question supersedes a running one (the host cancels it).
    if (current?.state === 'streaming') {
      current.state = 'cancelled';
      current.finishedAt = performance.now();
    }
    const id = uid();
    const c = choice;
    turns.push({
      id,
      question: text,
      screen: withScreen,
      screenLabel: screen.title || screenLabel,
      modelLabel: c ? `${modelLabel}${level ? ` · ${levelLabel(level)}` : ''}` : '',
      providerLabel: providerName,
      providerId: c?.provider ?? '',
      limitWarning: '',
      answer: '',
      reasoning: '',
      status: '',
      state: 'streaming',
      error: '',
      startedAt: performance.now(),
      firstTokenAt: 0,
      answeredAt: 0,
      finishedAt: 0,
      outputTokens: 0,
      streamed: true,
    });
    send({
      type: 'ask',
      id,
      text,
      provider: c?.provider ?? '',
      model: c?.model ?? '',
      reasoning: level,
      screen: withScreen,
      screenId: withScreen ? screen.id : undefined,
    });
    draft = '';
    announcement = 'Creating an answer.';
    // Follow-ups retain conversation context, but don't add the same attachment twice.
    if (withScreen) screenOn = false;
    tick().then(() => {
      autosize();
      scrollToBottom(true);
    });
  }

  function retry(turn: Turn) {
    // A retry must use the currently reviewed attachment and destination.
    screenOn = turn.screen && canShare;
    draft = turn.question;
    focusInput(false);
  }

  function cancel() {
    const t = turns.at(-1);
    if (t?.state === 'streaming') send({ type: 'cancel', id: t.id });
  }

  function newChat() {
    cancel();
    send({ type: 'newChat' });
    turns = [];
    announcement = 'New chat.';
    screenOn = (app.settings?.screenContextDefault ?? true) && canShare;
    focusInput(false);
  }

  // ---- Screen context ------------------------------------------------------------------
  function toggleScreen() {
    if (!canShare) return;
    screenOn = !screenOn;
    previewReady = false;
  }
  function toggleScreenMode() {
    const mode = screen.mode === 'window' ? 'monitor' : 'window';
    previewReady = false;
    send({ type: 'setScreenMode', mode });
    screenOn = true;
  }

  // ---- Model picker ----------------------------------------------------------------------
  async function togglePicker() {
    if (pickerOpen) {
      pickerOpen = false;
      focusInput(false);
      return;
    }
    if (!modelChip || !stage) return;
    const chip = layoutRect(modelChip);
    const st = layoutRect(stage);
    pickerPos = { x: chip.x - st.x - 4, y: chip.y - st.y + chip.height + 8 };
    pickerOpen = true;
    // Open upward when the list would run past the bottom of the canvas.
    await tick();
    const h = pickerEl?.offsetHeight ?? 0;
    if (st.y + pickerPos.y + h > canvasHeight - 8 && chip.y - 8 - h >= 8) {
      pickerPos = { ...pickerPos, y: chip.y - st.y - 8 - h };
    }
    pickerPos = { x: Math.max(8 - st.x, Math.min(pickerPos.x, canvasWidth - st.x - (pickerEl?.offsetWidth ?? 372) - 8)),
      y: Math.max(8 - st.y, Math.min(pickerPos.y, canvasHeight - st.y - h - 8)) };
  }
  function restoreModelFocus() {
    void tick().then(() => modelChip?.focus());
  }
  function pickModel(c: ModelChoice) {
    if (choice && c.provider !== choice.provider) newChat();
    app.saveSettings({ defaultModel: c });
    if (app.findModel(c.provider, c.model)?.vision === false) screenOn = false;
    pickerOpen = false;
    focusInput(false);
  }
  function pickLevel(reasoning: string) {
    if (!choice) return;
    app.saveSettings({ defaultModel: { ...choice, reasoning } });
  }

  // ---- Input -----------------------------------------------------------------------------
  function focusInput(selectAll: boolean) {
    requestAnimationFrame(() => {
      input?.focus();
      if (selectAll && input && draft) input.select();
    });
  }

  function autosize() {
    if (!input) return;
    input.style.height = '0px';
    input.style.height = `${Math.min(input.scrollHeight, 132)}px`;
    // Sub-pixel line-height rounding leaves a 1px overflow; only scroll once text exceeds the cap.
    input.style.overflowY = input.scrollHeight > 132 ? 'auto' : 'hidden';
  }

  function onInputKey(e: KeyboardEvent) {
    if (e.key === 'Enter' && !e.shiftKey && !e.isComposing) {
      e.preventDefault();
      submit();
    }
  }

  function onWindowKey(e: KeyboardEvent) {
    if (!open) return;
    const ctrl = e.ctrlKey && !e.altKey && !e.metaKey;
    if (e.key === 'Tab') {
      const root = pickerOpen ? pickerEl : stage;
      const controls = [...(root?.querySelectorAll<HTMLElement>('button:not(:disabled), input:not(:disabled), textarea:not(:disabled), select:not(:disabled), a[href], [tabindex="0"]') ?? [])]
        .filter((el) => el.tabIndex >= 0 && el.getClientRects().length && !el.closest('[inert]'));
      const index = controls.indexOf(document.activeElement as HTMLElement);
      if (controls.length && (index === -1 || e.shiftKey && index === 0 || !e.shiftKey && index === controls.length - 1)) {
        e.preventDefault();
        controls[e.shiftKey ? controls.length - 1 : 0].focus();
      }
    } else if (e.key === 'Escape') {
      e.preventDefault();
      if (pickerOpen) {
        pickerOpen = false;
        restoreModelFocus();
      } else if (streaming) cancel();
      else close(true);
    } else if (ctrl && e.key.toLowerCase() === 'n') {
      e.preventDefault();
      newChat();
    } else if (ctrl && e.key.toLowerCase() === 's') {
      e.preventDefault();
      toggleScreen();
    } else if (ctrl && e.key.toLowerCase() === 'm') {
      e.preventDefault();
      togglePicker();
    } else if (ctrl && e.key === ',') {
      e.preventDefault();
      send({ type: 'openSettings' });
    } else if (ctrl && e.shiftKey && e.key.toLowerCase() === 'c') {
      const last = turns.findLast((t) => t.answer);
      if (last) {
        e.preventDefault();
        void copyToHost(last.answer);
      }
    }
  }

  // ---- Scrolling -------------------------------------------------------------------------
  let stick = true;
  function onScroll() {
    if (!scroller) return;
    stick = scroller.scrollHeight - scroller.scrollTop - scroller.clientHeight < 48;
  }
  function scrollToBottom(force = false) {
    if (!scroller || (!stick && !force) || !force && scroller.contains(document.activeElement)) return;
    scroller.scrollTop = scroller.scrollHeight;
    stick = true;
  }
  $effect(() => {
    const last = turns.at(-1);
    void last?.answer;
    void last?.reasoning;
    void last?.state;
    requestAnimationFrame(() => scrollToBottom());
  });

  // ---- Native hit region -----------------------------------------------------------------
  // Only the panel (and an open picker) is clickable; everything else passes through to the
  // desktop. While the panel grows, the region uses the target height so nothing is clipped.
  let lastRegion = '';
  const syncRegion = rafThrottle(() => {
    if (!stage || !panel) return;
    const h = Math.max(panelHeight, targetHeight);
    let rect: Rect = layoutRect(panel, h);
    // The panel's top moves with its height, so the region must follow the target's top edge.
    rect.y += stageTop(h) - stageTop(panelHeight);
    if (pickerOpen && pickerEl) rect = union(rect, layoutRect(pickerEl));
    const region = {
      x: Math.floor(rect.x - SHADOW_PAD),
      y: Math.floor(rect.y - SHADOW_PAD / 2),
      width: Math.ceil(rect.width + SHADOW_PAD * 2),
      height: Math.ceil(rect.height + SHADOW_PAD * 2),
      radius: RADIUS + SHADOW_PAD,
    };
    const key = JSON.stringify(region);
    if (key === lastRegion) return;
    lastRegion = key;
    send({ type: 'setHitRegion', ...region });
  });

  $effect(() => {
    void pickerOpen;
    void pickerEl;
    syncRegion();
  });

  function onCanvasPointer(e: PointerEvent) {
    // Clicks on the transparent margin around the panel dismiss the bar.
    if (e.target === e.currentTarget && open) close(true);
  }

  onMount(() => {
    const ro = new ResizeObserver(() => measureInner());
    ro.observe(inner!);
    window.addEventListener('keydown', onWindowKey);
    // The panel is centred in the window, so a window resize (e.g. on first show) moves it.
    const resize = () => { canvasHeight = window.innerHeight; canvasWidth = window.innerWidth; tick().then(() => { measureInner(); placeStage(); syncRegion(); }); };
    window.addEventListener('resize', resize);
    if (stage) stage.style.opacity = '0';
    return () => {
      ro.disconnect();
      window.removeEventListener('keydown', onWindowKey);
      window.removeEventListener('resize', resize);
    };
  });
</script>

<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="canvas" onpointerdown={onCanvasPointer}>
  <span class="sr-only" role="status" aria-live="polite" aria-atomic="true">{announcement}</span>
  <div class="stage" bind:this={stage} inert={!open} style="width:min({PANEL_WIDTH}px, calc(100vw - 16px));--panel-max-height:{Math.max(80, canvasHeight - 72)}px">
    <Glass bind:element={panel} {backdrop} mode={glassMode} radius={RADIUS} class="panel">
      <div class="inner" bind:this={inner} inert={pickerOpen}>
        <div class="input-row">
          <span class="glyph" class:busy={streaming}><Icon name="sparkle" size={18} fill /></span>
          <textarea
            class="quiet-input"
            bind:this={input}
            bind:value={draft}
            oninput={autosize}
            onkeydown={onInputKey}
            rows="1"
            placeholder={turns.length ? 'Ask a follow-up…' : 'Ask anything…'}
            spellcheck="false"
            aria-label="Ask"
          ></textarea>
          {#if streaming}
            <button type="button" class="send stop" title="Stop (Esc)" onclick={cancel}>
              <Icon name="stop" size={14} fill /><span>Stop</span>
            </button>
          {:else}
            <button
              type="button"
              class="send"
              class:ready={canSend}
              title={screenOn && !previewReady ? 'Wait for the screenshot preview or remove it' : 'Send (Enter)'}
              aria-label="Send"
              disabled={!canSend}
              onclick={() => submit()}
            >
              <Icon name="arrowUp" size={16} stroke={2.2} />
            </button>
          {/if}
        </div>

        <div class="chips">
          <div class="chip-group" class:on={screenOn} class:disabled={!canShare}>
            <button
              type="button"
              class="chip screen"
              disabled={!canShare}
              aria-pressed={screenOn}
              title={canShare ? `Include ${screenLabel.toLowerCase()} context (Ctrl+S)` : screen.reason || 'This model cannot view screenshots'}
              onclick={toggleScreen}
            >
              <Icon name={screen.mode === 'window' ? 'window' : 'screen'} size={14} />
              <span>{screenLabel} {screenOn ? 'on' : 'off'}</span>
            </button>
            {#if screen.hasWindow && canShare}
              <button
                type="button"
                class="chip mode"
                title={screen.mode === 'window' ? 'Use the whole screen' : `Use only ${screen.app || 'the active window'}`}
                onclick={toggleScreenMode}
              >
                <Icon name={screen.mode === 'window' ? 'screen' : 'window'} size={13} />
              </button>
            {/if}
          </div>

          <button
            type="button"
            class="chip model"
            class:open={pickerOpen}
            bind:this={modelChip}
            title="Choose model (Ctrl+M)"
            aria-expanded={pickerOpen}
            aria-haspopup="dialog"
            onclick={togglePicker}
          >
            {#if providerName}<span class="provider">{providerName}</span>{/if}
            <span class="model-name">{modelLabel}</span>
            {#if level}<span class="level">{levelLabel(level)}</span>{/if}
            <Icon name="chevronDown" size={12} stroke={2} />
          </button>

          <span class="spacer"></span>
          <button type="button" class="chip" aria-pressed={app.settings?.barPinned ?? false}
            title="Keep open when another app gets focus" onclick={() => app.saveSettings({ barPinned: !app.settings?.barPinned })}>
            {app.settings?.barPinned ? 'Pinned' : 'Pin'}
          </button>

          {#if turns.length}
            <button type="button" class="icon-btn" title="New chat (Ctrl+N)" onclick={newChat}>
              <Icon name="newChat" size={15} />
            </button>
          {/if}
          <button type="button" class="icon-btn" title="Settings (Ctrl+,)" onclick={() => send({ type: 'openSettings' })}>
            <Icon name="gear" size={15} />
          </button>
        </div>

        {#if budgetNearLimit && app.provider(choice?.provider ?? '')?.kind === 'api'}
          <div class="capture-notice" role="status">Daily API allowance nearly used · ${(app.payload?.budget.reservedUsd ?? 0).toFixed(2)} / ${app.settings!.apiDailyBudgetUsd.toFixed(2)}</div>
        {/if}

        {#if turns.length}
          <div class="scroller" bind:this={scroller} onscroll={onScroll}>
            {#each turns as turn (turn.id)}
              <TurnView {turn} onretry={retry} />
            {/each}
          </div>
        {/if}
      </div>
    </Glass>

    {#if pickerOpen}
      <ModelPicker
        bind:element={pickerEl}
        {backdrop}
        mode={glassMode}
        x={pickerPos.x}
        y={pickerPos.y}
        current={choice}
        onpick={pickModel}
        onlevel={pickLevel}
        onclose={() => {
          pickerOpen = false;
          restoreModelFocus();
        }}
      />
    {/if}
  </div>
</div>

<style>
  .canvas {
    position: fixed;
    inset: 0;
    overflow: hidden;
  }
  .stage {
    position: absolute;
    left: max(8px, calc(50% - 340px));
    transform-origin: 50% 0;
  }
  .stage :global(.panel) {
    height: 64px;
    overflow: hidden;
  }
  .inner {
    position: absolute;
    top: 0;
    left: 0;
    right: 0;
    max-height: var(--panel-max-height);
    overflow-y: auto;
    scrollbar-gutter: stable;
    display: flex;
    flex-direction: column;
  }

  .input-row {
    display: flex;
    align-items: flex-start;
    gap: 10px;
    padding: 14px 14px 6px 18px;
  }
  .glyph {
    display: flex;
    margin-top: 6px;
    color: var(--accent);
    filter: drop-shadow(0 0 6px color-mix(in srgb, var(--accent) 45%, transparent));
  }
  .glyph.busy {
    animation: glyph-spin 2.4s var(--ease-out) infinite;
  }
  @keyframes glyph-spin {
    0% {
      transform: rotate(0) scale(1);
    }
    50% {
      transform: rotate(180deg) scale(0.86);
    }
    100% {
      transform: rotate(360deg) scale(1);
    }
  }
  textarea {
    flex: 1;
    min-width: 0;
    min-height: 30px;
    max-height: 132px;
    padding: 3px 0;
    border: 0;
    resize: none;
    overflow-y: hidden;
    background: transparent;
    font-family: var(--font-display);
    font-size: 1.3571rem;
    line-height: 1.3;
    letter-spacing: -0.01em;
    color: var(--fg);
    caret-color: var(--accent);
    user-select: text;
    -webkit-user-select: text;
    cursor: text;
  }
  textarea::placeholder {
    color: var(--fg-3);
  }
  .send {
    flex: none;
    display: grid;
    place-items: center;
    width: 30px;
    height: 30px;
    border: 0;
    border-radius: 50%;
    background: var(--fill);
    color: var(--fg-3);
    transition:
      background 180ms,
      color 180ms,
      transform 220ms var(--ease-spring),
      box-shadow 180ms;
  }
  .send.ready {
    background: var(--accent);
    color: var(--accent-fg);
    box-shadow: 0 4px 14px -4px color-mix(in srgb, var(--accent) 70%, transparent);
  }
  .send.stop {
    display: flex;
    gap: 6px;
    width: auto;
    padding: 0 10px;
    border-radius: 10px;
    background: var(--fill-active);
    color: var(--fg);
  }
  .send:not(:disabled):active {
    transform: scale(0.9);
  }

  .chips {
    display: flex;
    flex-wrap: wrap;
    align-items: center;
    gap: 6px;
    padding: 4px 12px 8px 14px;
  }
  .capture-notice {
    display: flex;
    align-items: flex-start;
    gap: 7px;
    padding: 0 18px 10px;
    color: var(--fg-2);
    font-size: 0.85714rem;
    overflow-wrap: anywhere;
  }
  .chip {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    min-height: 28px;
    height: auto;
    padding-block: 4px;
    padding: 0 10px;
    border: 0;
    border-radius: 14px;
    background: var(--fill);
    color: var(--fg-2);
    font-size: 0.89286rem;
    font-weight: 500;
    white-space: nowrap;
    transition:
      background 160ms,
      color 160ms,
      transform 220ms var(--ease-spring),
      box-shadow 160ms;
  }
  .chip:not(:disabled):hover {
    background: var(--fill-hover);
    color: var(--fg);
  }
  .chip:not(:disabled):active {
    transform: scale(0.96);
  }
  .chip:disabled {
    opacity: 0.45;
  }
  .chip-group {
    display: inline-flex;
    border-radius: 14px;
  }
  .chip-group .chip.screen:has(+ .mode) {
    border-radius: 14px 0 0 14px;
    padding-right: 8px;
  }
  .chip.mode {
    border-radius: 0 14px 14px 0;
    padding: 0 9px 0 7px;
    margin-left: 1px;
  }
  .chip-group.on .chip {
    background: color-mix(in srgb, var(--accent) 22%, transparent);
    color: var(--fg);
  }
  .chip-group.on .chip:hover {
    background: color-mix(in srgb, var(--accent) 30%, transparent);
  }
  .chip.screen {
    padding-left: 8px;
  }
  .chip.model {
    max-width: 360px;
  }
  .chip.model.open {
    background: var(--fill-active);
    color: var(--fg);
  }
  .provider {
    color: var(--fg-3);
  }
  .model-name {
    overflow: hidden;
    text-overflow: ellipsis;
    color: var(--fg);
  }
  .level {
    padding: 1px 6px;
    border-radius: 8px;
    background: var(--fill);
    color: var(--fg-2);
    font-size: 0.78571rem;
  }
  .spacer {
    flex: 1;
  }
  .icon-btn {
    display: grid;
    place-items: center;
    width: 28px;
    height: 28px;
    border: 0;
    border-radius: 9px;
    background: transparent;
    color: var(--fg-2);
    transition:
      background 140ms,
      transform 200ms var(--ease-spring);
  }
  .icon-btn:hover {
    background: var(--fill-hover);
    color: var(--fg);
  }
  .icon-btn:active {
    transform: scale(0.92);
  }

  .scroller {
    flex: 1 1 auto;
    min-height: min(144px, calc(var(--panel-max-height) / 2));
    overflow-y: auto;
    padding: 0 20px 14px 22px;
    border-top: 1px solid var(--line);
    scroll-padding: 10px;
  }
</style>
