// Small spring-physics animator (damped harmonic oscillator, fixed-step integration).
// Values are pushed to `onUpdate` every frame; nothing here touches Svelte reactivity so
// per-frame style writes stay cheap.

export interface SpringOptions {
  /** Angular stiffness (higher = snappier). */
  stiffness?: number;
  /** Damping ratio: 1 = critically damped, < 1 overshoots. */
  damping?: number;
  /** Settling tolerance. */
  precision?: number;
}

export const springs = {
  /** Panel entrance: quick with a hint of overshoot. */
  open: { stiffness: 420, damping: 0.72 },
  /** Height morphs: smooth, no visible bounce. */
  layout: { stiffness: 340, damping: 0.92 },
  /** Small UI bits (chips, menus). */
  snappy: { stiffness: 700, damping: 0.8 },
} satisfies Record<string, SpringOptions>;

let reduced = false;
export function setReducedMotion(value: boolean) {
  reduced = value;
}
export function reducedMotion() {
  return reduced;
}

export class Spring {
  value: number;
  target: number;
  velocity = 0;
  private opts: Required<SpringOptions>;
  private frame = 0;
  private last = 0;
  private resolve: (() => void) | null = null;

  constructor(
    initial: number,
    opts: SpringOptions,
    private onUpdate: (value: number, velocity: number) => void,
  ) {
    this.value = this.target = initial;
    this.opts = { stiffness: 300, damping: 0.85, precision: 0.001, ...opts };
  }

  get animating() {
    return this.frame !== 0;
  }

  configure(opts: SpringOptions) {
    this.opts = { ...this.opts, ...opts };
  }

  /** Animates towards `target`. Resolves when settled (or immediately with reduced motion). */
  set(target: number, opts?: SpringOptions & { velocity?: number }): Promise<void> {
    if (opts) this.configure(opts);
    if (opts?.velocity !== undefined) this.velocity = opts.velocity;
    this.target = target;
    if (reduced) {
      this.jump(target);
      return Promise.resolve();
    }
    this.resolve?.();
    const done = new Promise<void>((r) => (this.resolve = r));
    if (!this.frame) {
      this.last = performance.now();
      this.frame = requestAnimationFrame(this.tick);
    }
    return done;
  }

  /** Moves to `value` instantly and stops. */
  jump(value: number) {
    this.stop();
    this.value = this.target = value;
    this.velocity = 0;
    this.onUpdate(value, 0);
  }

  stop() {
    if (this.frame) cancelAnimationFrame(this.frame);
    this.frame = 0;
    this.resolve?.();
    this.resolve = null;
  }

  private tick = (now: number) => {
    // Cap dt so a hidden/suspended page doesn't explode the integrator.
    let dt = Math.min((now - this.last) / 1000, 1 / 20);
    this.last = now;
    const { stiffness, damping, precision } = this.opts;
    const omega = Math.sqrt(stiffness);
    const step = 1 / 240;
    while (dt > 0) {
      const h = Math.min(step, dt);
      const force = -stiffness * (this.value - this.target) - 2 * damping * omega * this.velocity;
      this.velocity += force * h;
      this.value += this.velocity * h;
      dt -= h;
    }
    const scale = Math.max(1, Math.abs(this.target));
    if (Math.abs(this.velocity) < precision * scale * 10 && Math.abs(this.value - this.target) < precision * scale) {
      this.value = this.target;
      this.velocity = 0;
      this.onUpdate(this.value, 0);
      this.frame = 0;
      this.resolve?.();
      this.resolve = null;
      return;
    }
    this.onUpdate(this.value, this.velocity);
    this.frame = requestAnimationFrame(this.tick);
  };
}

/** Runs `fn` at most once per animation frame with the latest arguments. */
export function rafThrottle<A extends unknown[]>(fn: (...args: A) => void): ((...args: A) => void) & { cancel(): void } {
  let frame = 0;
  let args: A;
  const wrapped = (...a: A) => {
    args = a;
    if (!frame) {
      frame = requestAnimationFrame(() => {
        frame = 0;
        fn(...args);
      });
    }
  };
  wrapped.cancel = () => {
    if (frame) cancelAnimationFrame(frame);
    frame = 0;
  };
  return wrapped;
}

export const clamp = (v: number, lo: number, hi: number) => Math.min(hi, Math.max(lo, v));
export const lerp = (a: number, b: number, t: number) => a + (b - a) * t;
