// Streaming-friendly Markdown: marked -> DOMPurify -> word spans -> morphdom patch.
// Re-rendering the whole answer each frame is cheap at chat sizes, and morphdom keeps
// existing nodes in place so only genuinely new words get the fade-in animation.

import DOMPurify from 'dompurify';
import hljs from 'highlight.js/lib/core';
import bash from 'highlight.js/lib/languages/bash';
import c from 'highlight.js/lib/languages/c';
import cpp from 'highlight.js/lib/languages/cpp';
import csharp from 'highlight.js/lib/languages/csharp';
import css from 'highlight.js/lib/languages/css';
import diff from 'highlight.js/lib/languages/diff';
import go from 'highlight.js/lib/languages/go';
import java from 'highlight.js/lib/languages/java';
import javascript from 'highlight.js/lib/languages/javascript';
import json from 'highlight.js/lib/languages/json';
import kotlin from 'highlight.js/lib/languages/kotlin';
import markdown from 'highlight.js/lib/languages/markdown';
import php from 'highlight.js/lib/languages/php';
import plaintext from 'highlight.js/lib/languages/plaintext';
import powershell from 'highlight.js/lib/languages/powershell';
import python from 'highlight.js/lib/languages/python';
import ruby from 'highlight.js/lib/languages/ruby';
import rust from 'highlight.js/lib/languages/rust';
import sql from 'highlight.js/lib/languages/sql';
import swift from 'highlight.js/lib/languages/swift';
import typescript from 'highlight.js/lib/languages/typescript';
import xml from 'highlight.js/lib/languages/xml';
import yaml from 'highlight.js/lib/languages/yaml';
import { Marked, type Tokens } from 'marked';
import morphdom from 'morphdom';
import katex from 'katex';
import 'katex/dist/katex.min.css';
import { mathExtension } from './math';

const languages = {
  bash, c, cpp, csharp, css, diff, go, java, javascript, json, kotlin, markdown, php, plaintext,
  powershell, python, ruby, rust, sql, swift, typescript, xml, yaml,
};
for (const [name, lang] of Object.entries(languages)) hljs.registerLanguage(name, lang);
hljs.registerAliases(['sh', 'shell', 'zsh', 'console'], { languageName: 'bash' });
hljs.registerAliases(['ps', 'ps1', 'pwsh'], { languageName: 'powershell' });
hljs.registerAliases(['html', 'svg', 'vue', 'svelte'], { languageName: 'xml' });
hljs.registerAliases(['js', 'jsx', 'mjs'], { languageName: 'javascript' });
hljs.registerAliases(['ts', 'tsx'], { languageName: 'typescript' });
hljs.registerAliases(['py'], { languageName: 'python' });
hljs.registerAliases(['c++', 'hpp', 'cc', 'h'], { languageName: 'cpp' });
hljs.registerAliases(['cs', 'c#'], { languageName: 'csharp' });
hljs.registerAliases(['rs'], { languageName: 'rust' });
hljs.registerAliases(['yml'], { languageName: 'yaml' });
hljs.registerAliases(['md'], { languageName: 'markdown' });
hljs.registerAliases(['text', 'txt'], { languageName: 'plaintext' });

const highlightCache = new Map<string, string>();

function escapeHtml(s: string) {
  return s.replace(/[&<>"']/g, (ch) => `&#${ch.charCodeAt(0)};`);
}

function highlight(code: string, lang: string): string {
  const key = `${lang}\u0000${code}`;
  const cached = highlightCache.get(key);
  if (cached !== undefined) return cached;
  let html: string;
  try {
    html = hljs.getLanguage(lang)
      ? hljs.highlight(code, { language: lang, ignoreIllegals: true }).value
      : escapeHtml(code);
  } catch {
    html = escapeHtml(code);
  }
  if (highlightCache.size > 200) highlightCache.clear();
  highlightCache.set(key, html);
  return html;
}

const marked = new Marked({ gfm: true, breaks: false, async: false });
const streamingMarked = new Marked({ gfm: true, breaks: false, async: false });
const codeRenderer = {
  renderer: {
    code({ text, lang }: Tokens.Code) {
      const language = (lang ?? '').trim().split(/\s+/)[0].toLowerCase();
      const label = language && language !== 'plaintext' ? language : 'text';
      return (
        `<div class="code-block"><div class="code-head"><span class="code-lang">${escapeHtml(label)}</span>` +
        `<button type="button" class="code-copy" data-copy>Copy</button></div>` +
        `<pre><code class="hljs">${highlight(text, language || 'plaintext')}</code></pre></div>`
      );
    },
  },
};
marked.use(codeRenderer, { extensions: [mathExtension(false)] });
streamingMarked.use(codeRenderer, { extensions: [mathExtension(true)] });

DOMPurify.addHook('afterSanitizeAttributes', (node) => {
  if (node.tagName === 'A') {
    node.setAttribute('rel', 'noreferrer noopener');
    node.setAttribute('target', '_blank');
  }
});

const PURIFY = {
  FORBID_TAGS: ['img', 'style', 'form', 'input', 'textarea', 'select', 'iframe', 'video', 'audio', 'source'],
  FORBID_ATTR: ['style'],
  ADD_ATTR: ['target', 'data-copy', 'data-latex', 'data-display', 'data-pending'],
};

const mathCache = new Map<string, string>();
function renderMath(root: DocumentFragment) {
  for (const slot of root.querySelectorAll<HTMLElement>('.math-slot[data-latex]')) {
    const latex = slot.dataset.latex ?? '';
    const display = slot.dataset.display === 'true';
    if (slot.dataset.pending === 'true') {
      slot.textContent = 'Formula…';
      slot.classList.add('math-pending');
      continue;
    }
    const key = `${display}\u0000${latex}`;
    try {
      let html = mathCache.get(key);
      if (html === undefined) {
        if (!latex || latex.length > 10000) throw new Error('Formula too long or empty');
        html = katex.renderToString(latex, { displayMode: display, trust: false, strict: 'error',
          throwOnError: true, maxExpand: 300, maxSize: 10, output: 'htmlAndMathml' });
        if (mathCache.size >= 200) mathCache.clear();
        mathCache.set(key, html);
      }
      // Only library-generated markup can carry styles. Model HTML was sanitized above.
      slot.append(DOMPurify.sanitize(html, { RETURN_DOM_FRAGMENT: true,
        ADD_TAGS: ['semantics', 'annotation'], ADD_ATTR: ['encoding'],
        FORBID_TAGS: ['a', 'img', 'style', 'script', 'iframe'] }) as DocumentFragment);
    } catch {
      const error = document.createElement('span');
      error.className = 'math-error';
      error.textContent = 'Formula could not be rendered.';
      slot.append(error);
    }
    const copy = document.createElement('button');
    copy.type = 'button'; copy.className = 'math-copy'; copy.dataset.mathCopy = '';
    copy.textContent = 'LaTeX'; copy.setAttribute('aria-label', 'Copy formula as LaTeX');
    copy.title = 'Copy formula as LaTeX';
    slot.append(copy);
  }
}

function tableControls(root: DocumentFragment) {
  for (const table of root.querySelectorAll('table')) {
    const wrap = document.createElement('div'); wrap.className = 'table-block';
    table.replaceWith(wrap);
    const copy = document.createElement('button'); copy.type = 'button'; copy.className = 'table-copy';
    copy.dataset.tableCopy = ''; copy.textContent = 'Copy table for Excel';
    wrap.append(copy, table);
  }
}

const SKIP_WORDS = new Set(['PRE', 'CODE', 'BUTTON']);

/** Wraps words of prose text nodes in <span class="w"> so new ones can be animated individually. */
function wrapWords(root: Node) {
  const walker = document.createTreeWalker(root, NodeFilter.SHOW_TEXT, {
    acceptNode(node) {
      for (let p = node.parentElement; p; p = p.parentElement) {
        if (SKIP_WORDS.has(p.tagName) || p.classList.contains('math-slot')) return NodeFilter.FILTER_REJECT;
      }
      return /\S/.test(node.nodeValue ?? '') ? NodeFilter.FILTER_ACCEPT : NodeFilter.FILTER_REJECT;
    },
  });
  const nodes: Text[] = [];
  while (walker.nextNode()) nodes.push(walker.currentNode as Text);
  for (const text of nodes) {
    const parts = (text.nodeValue ?? '').split(/(\s+)/);
    const frag = document.createDocumentFragment();
    for (const part of parts) {
      if (!part) continue;
      if (/^\s+$/.test(part)) {
        frag.appendChild(document.createTextNode(part));
      } else {
        const span = document.createElement('span');
        span.className = 'w';
        span.textContent = part;
        frag.appendChild(span);
      }
    }
    text.replaceWith(frag);
  }
}

export function toSafeFragment(src: string, words: boolean, streaming = false): DocumentFragment {
  const html = (streaming ? streamingMarked : marked).parse(src) as string;
  const frag = DOMPurify.sanitize(html, { ...PURIFY, RETURN_DOM_FRAGMENT: true }) as DocumentFragment;
  renderMath(frag);
  tableControls(frag);
  if (words) wrapWords(frag);
  return frag;
}

/**
 * Patches `container` to show `src`. With `animate`, newly added words fade/de-blur in,
 * staggered slightly within a single update.
 */
export function patchMarkdown(container: HTMLElement, src: string, animate: boolean, streaming = false) {
  const next = document.createElement('div');
  next.appendChild(toSafeFragment(src, animate, streaming));
  let stagger = 0;
  morphdom(container, next, {
    childrenOnly: true,
    onBeforeElUpdated(fromEl, toEl) {
      if (fromEl.isEqualNode(toEl)) return false;
      // Keep the entrance class so in-flight animations aren't cut by attribute syncing.
      if (fromEl.classList.contains('fresh')) toEl.classList.add('fresh');
      const delay = (fromEl as HTMLElement).style?.animationDelay;
      if (delay) (toEl as HTMLElement).style.animationDelay = delay;
      return true;
    },
    onNodeAdded(node) {
      if (!animate || !(node instanceof HTMLElement)) return node;
      if (node.matches('span.w') || node.matches('.code-block, hr, table, li:empty')) {
        node.classList.add('fresh');
        node.style.animationDelay = `${Math.min(stagger, 24) * 14}ms`;
        stagger++;
      }
      return node;
    },
  });
}

/** Static render (no animation), e.g. for reasoning summaries. */
export function renderMarkdownInto(container: HTMLElement, src: string) {
  const next = document.createElement('div');
  next.appendChild(toSafeFragment(src, false));
  morphdom(container, next, { childrenOnly: true });
}

/** Handles copy buttons and links inside rendered markdown. */
export async function handleMarkdownClick(
  event: MouseEvent,
  actions: { copy(text: string): Promise<boolean>; open(url: string): void },
) {
  const target = event.target as HTMLElement | null;
  if (!target) return;
  const copyBtn = target.closest<HTMLElement>('[data-copy], [data-math-copy], [data-table-copy]');
  if (copyBtn) {
    event.preventDefault();
    const text = copyBtn.hasAttribute('data-math-copy') ? copyBtn.closest<HTMLElement>('.math-slot')?.dataset.latex ?? ''
      : copyBtn.hasAttribute('data-table-copy') ? tableToTsv(copyBtn.closest('.table-block')!.querySelector('table')!)
      : copyBtn.closest('.code-block')?.querySelector('code')?.textContent ?? '';
    const label = copyBtn.textContent;
    const ok = await actions.copy(text);
    copyBtn.textContent = ok ? 'Copied' : 'Copy failed';
    copyBtn.classList.add('copied');
    setTimeout(() => {
      copyBtn.textContent = label;
      copyBtn.classList.remove('copied');
    }, 1400);
    return;
  }
  const link = target.closest<HTMLAnchorElement>('a[href]');
  if (link) {
    event.preventDefault();
    if (/^https?:\/\//i.test(link.href)) actions.open(link.href);
  }
}

export function tableToTsv(table: HTMLTableElement): string {
  return [...table.rows].map((row) => [...row.cells].map((cell) => {
    const clone = cell.cloneNode(true) as HTMLElement;
    for (const math of clone.querySelectorAll<HTMLElement>('.math-slot')) math.replaceWith(math.dataset.latex ?? '');
    clone.querySelectorAll('button').forEach((node) => node.remove());
    // Excel interprets leading formula characters on paste. Copy these cells as literal text.
    const text = (clone.textContent ?? '').replace(/[\t\r\n]+/g, ' ').trim();
    return /^[=+@-]/.test(text) && !/^[-+]?\d+(?:[.,]\d+)?$/.test(text) ? `'${text}` : text;
  }).join('\t')).join('\r\n');
}

/** Portable HTML + readable text fallback, with math preserved as LaTeX. */
export function formattedAnswer(src: string): { text: string; html: string } {
  const container = document.createElement('div'); container.append(toSafeFragment(src, false));
  container.querySelectorAll('button, .code-head').forEach((node) => node.remove());
  for (const math of container.querySelectorAll<HTMLElement>('.math-slot')) {
    const display = math.dataset.display === 'true';
    math.replaceWith(document.createTextNode(`${display ? '$$' : '$'}${math.dataset.latex ?? ''}${display ? '$$' : '$'}`));
  }
  for (const cell of container.querySelectorAll<HTMLElement>('th,td')) cell.style.cssText = 'border:1px solid #777;padding:6px 10px;text-align:left';
  for (const table of container.querySelectorAll<HTMLElement>('table')) table.style.borderCollapse = 'collapse';
  const clone = container.cloneNode(true) as HTMLElement;
  for (const table of clone.querySelectorAll('table')) table.replaceWith(document.createTextNode(tableToTsv(table)));
  for (const br of clone.querySelectorAll('br')) br.replaceWith('\n');
  for (const block of clone.querySelectorAll('p,h1,h2,h3,h4,h5,h6,li,pre,blockquote,.table-block')) block.append('\n');
  return { text: (clone.textContent ?? '').trim(), html: `<div style="font-family:Segoe UI,sans-serif;font-size:11pt;color:#17171c">${container.innerHTML}</div>` };
}
