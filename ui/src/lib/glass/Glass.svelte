<!--
  Liquid glass surface. Layers, back to front:
    backdrop  - the frozen screen capture behind this element, refracted at the rim, blurred, saturated
    tint      - theme-dependent wash that keeps text legible
    noise     - faint grain so the blur doesn't band
    content
    rim       - specular edge highlight + inner glow
  `mode`:
    liquid - draws the capture itself (default)
    system - DWM acrylic is behind the window; only a light tint is drawn
    solid  - no capture available; an opaque-ish fallback
-->
<script lang="ts">
  import type { Snippet } from 'svelte';
  import { onDestroy, onMount } from 'svelte';
  import { displacementMap } from './refraction';

  interface Props {
    backdrop?: string;
    mode?: 'liquid' | 'system' | 'solid';
    radius?: number;
    /** Displacement strength in px (0 disables refraction). */
    refraction?: number;
    blur?: number;
    tone?: 'panel' | 'popover';
    class?: string;
    style?: string;
    element?: HTMLDivElement;
    children?: Snippet;
    onclick?: (e: MouseEvent) => void;
    onpointerdown?: (e: PointerEvent) => void;
  }

  let {
    backdrop = '',
    mode = 'liquid',
    radius = 24,
    refraction = 42,
    blur = 16,
    tone = 'panel',
    class: klass = '',
    style = '',
    element = $bindable(),
    children,
    onclick,
    onpointerdown,
  }: Props = $props();

  const PAD = 48; // backdrop overscan so the blur never samples transparent edges
  const uid = `glass-${Math.random().toString(36).slice(2, 9)}`;

  let width = $state(0);
  let height = $state(0);
  let offsetX = $state(0);
  let offsetY = $state(0);
  let canvasW = $state(window.innerWidth);
  let canvasH = $state(window.innerHeight);
  let mapUrl = $state('');

  interface Layer {
    url: string;
    key: number;
  }
  let layers = $state<Layer[]>([]);
  let layerKey = 0;

  // Crossfade to a new capture once it has decoded, so refreshes never flash.
  $effect(() => {
    const url = backdrop;
    if (!url) {
      layers = [];
      return;
    }
    if (layers.at(-1)?.url === url) return;
    const img = new Image();
    let cancelled = false;
    img.onload = img.onerror = () => {
      if (cancelled) return;
      layers = [...layers.slice(-1), { url, key: ++layerKey }];
      setTimeout(() => {
        if (layers.length > 1 && layers.at(-1)?.url === url) layers = layers.slice(-1);
      }, 450);
    };
    img.src = url;
    return () => (cancelled = true);
  });

  function measure() {
    if (!element) return;
    width = element.offsetWidth;
    height = element.offsetHeight;
    // Layout offset relative to the page (ignores transforms, which only animate).
    let x = 0;
    let y = 0;
    for (let el: HTMLElement | null = element; el; el = el.offsetParent as HTMLElement | null) {
      x += el.offsetLeft;
      y += el.offsetTop;
    }
    offsetX = x;
    offsetY = y;
    canvasW = window.innerWidth;
    canvasH = window.innerHeight;
  }

  let mapTimer = 0;
  let lastMap = 0;
  const useRefraction = $derived(mode === 'liquid' && refraction > 0 && layers.length > 0);

  // Regenerating the map is a few ms; throttle it while the panel is morphing.
  $effect(() => {
    const w = width;
    const h = height;
    const r = radius;
    if (!useRefraction || w < 4 || h < 4) return;
    const run = () => {
      lastMap = performance.now();
      mapUrl = displacementMap({ width: w, height: h, inset: PAD, radius: r, band: Math.min(28, r + 8) });
    };
    clearTimeout(mapTimer);
    const since = performance.now() - lastMap;
    if (since > 80 || !mapUrl) run();
    else mapTimer = window.setTimeout(run, 80 - since);
  });

  let observer: ResizeObserver | undefined;
  onMount(() => {
    measure();
    observer = new ResizeObserver(() => measure());
    observer.observe(element!);
    window.addEventListener('resize', measure);
  });
  onDestroy(() => {
    observer?.disconnect();
    window.removeEventListener('resize', measure);
    clearTimeout(mapTimer);
  });

  export function remeasure() {
    measure();
  }

  const backdropStyle = $derived(
    `left:${-PAD}px;top:${-PAD}px;width:${width + PAD * 2}px;height:${height + PAD * 2}px;` +
      `background-size:${canvasW}px ${canvasH}px;` +
      `background-position:${-(offsetX - PAD)}px ${-(offsetY - PAD)}px;` +
      `--glass-blur:${blur}px;` +
      (useRefraction && mapUrl ? `--glass-refract:url(#${uid});` : ''),
  );
</script>

<div
  bind:this={element}
  class="glass glass-{mode} glass-{tone} {klass}"
  style="border-radius:{radius}px;{style}"
  role="presentation"
  {onclick}
  {onpointerdown}
>
  {#if useRefraction && mapUrl}
    <svg class="glass-defs" width="0" height="0" aria-hidden="true">
      <filter
        id={uid}
        x="0"
        y="0"
        width={width + PAD * 2}
        height={height + PAD * 2}
        filterUnits="userSpaceOnUse"
        primitiveUnits="userSpaceOnUse"
        color-interpolation-filters="sRGB"
      >
        <feImage
          href={mapUrl}
          x="0"
          y="0"
          width={width + PAD * 2}
          height={height + PAD * 2}
          preserveAspectRatio="none"
          result="map"
        />
        <feDisplacementMap in="SourceGraphic" in2="map" scale={refraction} xChannelSelector="R" yChannelSelector="G" />
      </filter>
    </svg>
  {/if}
  <div class="glass-bg" aria-hidden="true">
    {#if mode === 'liquid'}
      {#each layers as layer (layer.key)}
        <div class="glass-backdrop" style="{backdropStyle}background-image:url('{layer.url}')"></div>
      {/each}
    {/if}
    <div class="glass-tint"></div>
    <div class="glass-noise"></div>
  </div>
  <div class="glass-content">
    {@render children?.()}
  </div>
  <div class="glass-rim" aria-hidden="true"></div>
</div>

<style>
  .glass {
    position: relative;
    isolation: isolate;
    box-shadow: var(--glass-shadow);
  }
  .glass-defs {
    position: absolute;
    width: 0;
    height: 0;
    overflow: hidden;
  }
  .glass-bg {
    position: absolute;
    inset: 0;
    border-radius: inherit;
    overflow: hidden;
    z-index: 0;
    pointer-events: none;
  }
  .glass-backdrop {
    position: absolute;
    background-repeat: no-repeat;
    filter: var(--glass-refract, ) blur(var(--glass-blur)) saturate(var(--glass-saturate))
      brightness(var(--glass-brightness));
    animation: glass-in 420ms ease both;
    will-change: filter;
  }
  @keyframes glass-in {
    from {
      opacity: 0;
    }
  }
  .glass-tint {
    position: absolute;
    inset: 0;
    background: var(--glass-tint);
  }
  .glass-solid .glass-tint {
    background: var(--glass-tint-solid);
  }
  .glass-system .glass-tint {
    background: var(--glass-tint-system);
  }
  .glass-popover .glass-tint {
    background: var(--glass-tint-popover);
  }
  .glass-noise {
    position: absolute;
    inset: 0;
    opacity: var(--glass-noise-opacity);
    mix-blend-mode: overlay;
    background-image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='160' height='160'><filter id='n'><feTurbulence type='fractalNoise' baseFrequency='0.9' numOctaves='2' stitchTiles='stitch'/><feColorMatrix values='0 0 0 0 0.5  0 0 0 0 0.5  0 0 0 0 0.5  0 0 0 1.4 -0.2'/></filter><rect width='100%' height='100%' filter='url(%23n)'/></svg>");
  }
  .glass-content {
    position: relative;
    z-index: 1;
    border-radius: inherit;
  }
  .glass-rim {
    position: absolute;
    inset: 0;
    z-index: 2;
    border-radius: inherit;
    pointer-events: none;
    box-shadow: var(--glass-inner);
  }
  .glass-rim::before {
    content: '';
    position: absolute;
    inset: 0;
    border-radius: inherit;
    padding: 1px;
    background: var(--glass-rim);
    -webkit-mask:
      linear-gradient(#000 0 0) content-box,
      linear-gradient(#000 0 0);
    mask:
      linear-gradient(#000 0 0) content-box,
      linear-gradient(#000 0 0);
    -webkit-mask-composite: xor;
    mask-composite: exclude;
  }
</style>
