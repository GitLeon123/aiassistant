// Generates feDisplacementMap images for a rounded rectangle: near the edge, pixels sample
// the backdrop further outward along the edge normal, bending what's behind the glass like a
// thick lens rim. Centre stays neutral (no distortion) so the panel reads cleanly.

const cache = new Map<string, string>();

export interface MapSpec {
  /** Panel size in CSS px. */
  width: number;
  height: number;
  /** How far the backdrop layer extends beyond the panel on each side. */
  inset: number;
  radius: number;
  /** Depth of the refracting rim, in CSS px. */
  band: number;
}

/** Returns a data URL; the map covers (width + 2*inset) x (height + 2*inset) CSS px. */
export function displacementMap(spec: MapSpec): string {
  const width = Math.max(1, Math.round(spec.width));
  const height = Math.max(1, Math.round(spec.height));
  const key = `${width}x${height}:${spec.inset}:${spec.radius}:${spec.band}`;
  const hit = cache.get(key);
  if (hit) return hit;

  // Half resolution is indistinguishable after the blur and 4x cheaper.
  const res = 0.5;
  const fullW = width + spec.inset * 2;
  const fullH = height + spec.inset * 2;
  const cw = Math.max(1, Math.round(fullW * res));
  const ch = Math.max(1, Math.round(fullH * res));
  const canvas = document.createElement('canvas');
  canvas.width = cw;
  canvas.height = ch;
  const ctx = canvas.getContext('2d')!;
  const img = ctx.createImageData(cw, ch);
  const data = img.data;

  const bx = width / 2;
  const by = height / 2;
  const r = Math.min(spec.radius, bx, by);
  const cx = spec.inset + bx;
  const cy = spec.inset + by;
  const band = Math.max(1, spec.band);

  let i = 0;
  for (let py = 0; py < ch; py++) {
    const y = (py + 0.5) / res - cy;
    const ay = Math.abs(y);
    const sy = Math.sign(y);
    const qy = ay - by + r;
    for (let px = 0; px < cw; px++, i += 4) {
      const x = (px + 0.5) / res - cx;
      const ax = Math.abs(x);
      const qx = ax - bx + r;
      // Signed distance to the rounded box (negative inside).
      const ox = Math.max(qx, 0);
      const oy = Math.max(qy, 0);
      const outside = Math.hypot(ox, oy);
      const d = outside + Math.min(Math.max(qx, qy), 0) - r;

      let dx = 0;
      let dy = 0;
      if (d < 0 && -d < band) {
        let nx: number;
        let ny: number;
        if (qx > 0 && qy > 0) {
          const len = outside || 1;
          nx = (Math.sign(x) * qx) / len;
          ny = (sy * qy) / len;
        } else if (qx > qy) {
          nx = Math.sign(x);
          ny = 0;
        } else {
          nx = 0;
          ny = sy;
        }
        // Smooth ramp: strongest at the very edge, easing to zero at the band's inner side.
        const t = 1 + d / band;
        const m = t * t * (3 - 2 * t);
        dx = nx * m;
        dy = ny * m;
      }
      data[i] = 128 + Math.round(dx * 127);
      data[i + 1] = 128 + Math.round(dy * 127);
      data[i + 2] = 128;
      data[i + 3] = 255;
    }
  }
  ctx.putImageData(img, 0, 0);
  const url = canvas.toDataURL('image/png');
  if (cache.size > 24) cache.clear();
  cache.set(key, url);
  return url;
}
