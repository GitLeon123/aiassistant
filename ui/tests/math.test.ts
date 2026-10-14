import assert from 'node:assert/strict';
import test from 'node:test';
import { Marked } from 'marked';
import { mathExtension } from '../src/lib/math.ts';

function parse(source: string, streaming = false) {
  return new Marked({ extensions: [mathExtension(streaming)] }).parse(source) as string;
}

test('math tokens keep complete and unfinished formulas separate from code and currency', () => {
  assert.match(parse('$x^2$ and $$\\frac{1}{2}$$'), /data-latex="x\^2"/);
  assert.match(parse('$2^3$'), /data-latex="2\^3"/);
  assert.match(parse('Before $\\frac{1}{', true), /data-pending="true"/);
  assert.match(parse('$2^', true), /data-pending="true"/);
  assert.match(parse('$2', true), /data-pending="true"/);
  assert.doesNotMatch(parse('Before $\\frac{1}{', true).replace(/data-latex="[^"]*"/g, ''), /\\frac/);
  assert.doesNotMatch(parse('`$x$`\n\n```tex\n$x$\n```'), /math-slot/);
  assert.doesNotMatch(parse('Costs $5 and $10. Escaped \\$x.'), /math-slot/);
  assert.match(parse('$$\nx^2\n+ y^2\n$$'), /math-display/);
  assert.equal((parse('$a$ and $b$').match(/math-slot/g) ?? []).length, 2);
});

test('math attributes cannot break out into model HTML', () => {
  const result = parse('$x"><img src=x onerror=alert(1)>$');
  assert.doesNotMatch(result, /<img/);
  assert.match(result, /&#34;/);
});
