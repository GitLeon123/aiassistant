import { formattedAnswer, patchMarkdown, tableToTsv, toSafeFragment } from '../src/lib/markdown';

export function verifyMarkdown() {
  const passed: string[] = [];
  function check(name: string, condition: boolean) {
    if (!condition) throw new Error(name);
    passed.push(name);
  }
  const host = document.createElement('div');
  patchMarkdown(host, 'Before $\\frac{1}{', false, true);
  check('Unfinished formula is held while streaming', host.textContent === 'Before Formula…\n');
  patchMarkdown(host, 'Before $\\frac{1}{2}$ after', false, true);
  check('Completed inline formula renders HTML and accessible MathML', !!host.querySelector('.katex-html') && !!host.querySelector('math semantics annotation'));
  check('Formula source is available for LaTeX copying', host.querySelector<HTMLElement>('.math-slot')?.dataset.latex === '\\frac{1}{2}');
  const stable = host.querySelector('.katex');
  patchMarkdown(host, 'Before $\\frac{1}{2}$ after more text', false, true);
  check('Streaming prose preserves the existing formula DOM', stable === host.querySelector('.katex'));
  host.replaceChildren(toSafeFragment('$$x^2$$\n\n`$x$`\n\n```tex\n$x$\n```\n\nCosts $5 and $10.', false));
  check('Display math renders while code and currency remain literal', host.querySelectorAll('.math-slot').length === 1 && host.textContent!.includes('$5 and $10'));
  host.replaceChildren(toSafeFragment('<img src=x onerror=alert(1)><script>alert(1)</script><a href="javascript:alert(1)">unsafe</a>\n\n$\\href{javascript:alert(1)}{x}$', false));
  check('Model HTML and math cannot create executable markup', !host.querySelector('img,script,iframe,[onerror],a[href^="javascript:"]'));
  const tableMarkdown = '| Label | Value |\n|---|---|\n| Name | **A** |\n| Formula | $x^2$ |\n| Formula injection | =HYPERLINK("https://example.com") |\n| Negative number | -42 |';
  host.replaceChildren(toSafeFragment(tableMarkdown, false));
  const tsv = tableToTsv(host.querySelector('table')!);
  check('TSV preserves cells and LaTeX and escapes Excel formulas', tsv.includes('Name\tA\r\nFormula\tx^2') && tsv.includes("\t'=HYPERLINK") && tsv.endsWith('\t-42'));
  const rich = formattedAnswer('# Title\n\nHello **world**.\n\n$E=mc^2$\n\n' + tableMarkdown);
  check('Formatted copy preserves heading, emphasis, table and LaTeX', rich.html.includes('<h1>Title</h1>') && rich.html.includes('<strong>world</strong>') && rich.html.includes('<table') && rich.text.includes('$E=mc^2$'));
  check('Formatted copy excludes UI and rendering artifacts', !/data-copy|data-latex|katex|<button/.test(rich.html));
  return passed;
}
