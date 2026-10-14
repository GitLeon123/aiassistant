import type { TokenizerAndRendererExtension, Tokens } from 'marked';

export interface MathToken extends Tokens.Generic {
  name?: string;
  latex: string;
  display: boolean;
  pending: boolean;
}

export function escapeHtml(text: string): string {
  return text.replace(/[&<>"']/g, (ch) => `&#${ch.charCodeAt(0)};`);
}

/** Dollar math stays one token; an unfinished formula never exposes streaming LaTeX. */
export function mathExtension(streaming: boolean): TokenizerAndRendererExtension {
  return {
    name: 'aiaMath', level: 'inline',
    start: (src) => src.indexOf('$'),
    tokenizer(src) {
      if (src[0] !== '$') return;
      const display = src.startsWith('$$');
      const delimiter = display ? '$$' : '$';
      const start = delimiter.length;
      if (!display && (/\s/.test(src[start] ?? '') || !src[start] && !streaming)) return;
      for (let end = start; end < src.length; end++) {
        if (src[end] === '\\') { end++; continue; }
        if (!src.startsWith(delimiter, end)) continue;
        if (!display && (/\s/.test(src[end - 1]) || /\d/.test(src[end + 1] ?? '') || src[end + 1] === '$')) continue;
        const raw = src.slice(0, end + delimiter.length);
        return { type: 'aiaMath', raw, latex: src.slice(start, end).trim(), display, pending: false };
      }
      // Inline math cannot run through a paragraph break. Literal currency is left alone.
      if (!display && src.includes('\n\n')) return;
      if (!display && /\d/.test(src[start] ?? '') && !/[\\^_={}]/.test(src.slice(start).split(/\s/)[0]) && (!streaming || /\s/.test(src.slice(start)))) return;
      return { type: 'aiaMath', raw: src, latex: src.slice(start).trim(), display, pending: streaming };
    },
    renderer(token) {
      const math = token as MathToken;
      return `<span class="math-slot${math.display ? ' math-display' : ''}" data-latex="${escapeHtml(math.latex)}" data-display="${math.display}" data-pending="${math.pending}"></span>`;
    },
  };
}
