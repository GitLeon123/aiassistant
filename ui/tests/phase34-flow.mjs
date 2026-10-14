import { chromium } from 'playwright';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const project = fileURLToPath(new URL('../../', import.meta.url));
const base = process.env.AIASSISTANT_UI_TEST_URL || 'http://127.0.0.1:5747';
const output = path.join(project, 'build/ui-phase34-tests');
await mkdir(output, { recursive: true });
const browser = await chromium.launch({ channel: 'msedge', headless: true });
const page = await browser.newPage({ viewport: { width: 760, height: 680 } });
const results = [], errors = [], audits = [], layout = [];
page.on('pageerror', (error) => errors.push(error.message));
await page.addInitScript(() => {
  window.testClipboard = [];
  Object.defineProperty(navigator, 'clipboard', { configurable: true, value: {
    writeText: async text => { window.testClipboard.push({ text }); },
    write: async items => { const item = items[0]; window.testClipboard.push({ text: await (await item.getType('text/plain')).text(), html: await (await item.getType('text/html')).text() }); },
  } });
});
async function record(name, run) { await run(); results.push(name); console.log('PASS', name); }
async function open(query = '') {
  await page.goto(`${base}/bar.html?reduced&${query}`);
  await page.waitForFunction(() => document.querySelector('[aria-label="Ask"]') === document.activeElement);
  await page.waitForFunction(() => document.querySelector('.preview-toggle img')?.naturalWidth > 0);
}
async function ask(text = 'Synthetic question') {
  await page.getByRole('textbox', { name: 'Ask', exact: true }).fill(text);
  await page.keyboard.press('Enter');
}
async function audit(name) {
  await page.addScriptTag({ url: `${base}/node_modules/axe-core/axe.min.js` });
  const violations = await page.evaluate(async () => (await axe.run(document, { runOnly: { type: 'tag', values: ['wcag2a','wcag2aa','wcag21aa','wcag22aa'] } })).violations.map(v => ({ id: v.id, impact: v.impact, nodes: v.nodes.map(n=>({ target:n.target, summary:n.failureSummary })) })));
  audits.push({ name, violations });
}
async function capture(name) {
  await page.waitForTimeout(200); // allow the reduced-motion entrance to settle
  await page.screenshot({ path: path.join(output, `${name}.png`) });
  layout.push({ name, ...await page.evaluate(() => ({ width: innerWidth, height: innerHeight, overflow: document.documentElement.scrollWidth > innerWidth, smallControls: [...document.querySelectorAll('button,input,textarea,select')].filter(e=>e.getClientRects().length && !e.closest('[inert]') && !e.disabled).map(e=>({label:e.getAttribute('aria-label') || e.textContent.trim() || e.title,width:e.getBoundingClientRect().width,height:e.getBoundingClientRect().height})).filter(e=>e.width<24||e.height<24) })) });
}
try {
  await record('Streaming math, sanitization, DOM stability and clipboard conversions', async () => {
    await open('solid&theme=dark&fast&math');
    results.push(...await page.evaluate(async () => (await import('/tests/markdown-browser.ts')).verifyMarkdown()));
    await ask();
    await page.getByRole('button', { name: 'Formatted', exact: true }).waitFor();
    await page.waitForFunction(()=>document.querySelectorAll('.katex').length===3);
    assert.match(await page.locator('.meta').innerText(), /Anthropic · Claude Opus/);
    assert.equal(await page.locator('.katex').count(), 3);
    assert.equal(await page.locator('.answer').getAttribute('aria-live'), 'off');
    assert.equal(await page.locator('[aria-live="polite"]').innerText(), 'Answer ready.');
    await page.getByRole('button', { name: 'Copy', exact: true }).click();
    await page.waitForFunction(() => window.testClipboard.length === 1);
    assert.match((await page.evaluate(() => window.testClipboard[0])).text, /\$x\^2/);
    await page.getByRole('button', { name: 'Formatted', exact: true }).click();
    await page.waitForFunction(() => window.testClipboard.length === 2);
    assert.match((await page.evaluate(() => window.testClipboard[1])).html, /<h1>Formula examples<\/h1>/);
    await page.getByRole('button', { name: 'Copy table for Excel' }).click();
    await page.waitForFunction(() => window.testClipboard.length === 3);
    assert.match((await page.evaluate(() => window.testClipboard[2])).text, /Item\tValue\r\nTotal\t42/);
    await page.getByRole('button', { name: 'Copy formula as LaTeX' }).first().click();
    await page.waitForFunction(() => window.testClipboard.length === 4);
    assert.equal((await page.evaluate(() => window.testClipboard[3])).text, 'x^2 + y^2 = z^2');
    await capture('bar-solid-dark-math'); await audit('bar-solid-dark-math');
  });
  await record('Clipboard failures report failure instead of success', async () => {
    await page.evaluate(() => { navigator.clipboard.writeText = async () => { throw new Error('Synthetic locked clipboard'); }; });
    await page.getByRole('button', { name: 'Copy', exact: true }).click();
    await page.locator('.meta').getByText('Copy failed. Try again.', { exact: true }).waitFor();
    assert.equal(await page.locator('[aria-live="polite"]').innerText(), 'Copy failed. Try again.');
  });
  await record('Keyboard focus cycles within bar and model picker, then returns to trigger', async () => {
    await page.locator('.chip.model').click();
    await page.getByRole('combobox', { name:'Search models' }).waitFor();
    await page.getByRole('combobox', { name: 'Search models' }).waitFor();
    assert.equal(await page.locator('.search input').evaluate(e => e === document.activeElement), true);
    await page.keyboard.press('ArrowDown');
    assert.match(await page.locator('.search input').getAttribute('aria-activedescendant'), /^model-option-\d+$/);
    await audit('model-picker');
    await page.keyboard.press('Escape');
    assert.equal(await page.locator('.chip.model').evaluate(e=>e===document.activeElement), true);
    await page.locator('.stage button:not(:disabled)').last().focus();
    await page.keyboard.press('Tab');
    assert.equal(await page.getByRole('textbox', { name:'Ask', exact:true }).evaluate(e=>e===document.activeElement), true);
  });
  await record('Pin persists across close/reopen and prevents focus-loss dismissal', async () => {
    await page.getByRole('button', { name: 'Pin', exact: true }).click();
    await page.getByRole('button', { name: 'Pinned', exact: true }).waitFor();
    await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
    assert.equal(await page.locator('.stage').getAttribute('inert'), null);
    await page.getByRole('button', { name: 'Close bar' }).click();
    await page.waitForFunction(()=>document.querySelector('.stage').inert && getComputedStyle(document.querySelector('.stage')).opacity==='0');
    await page.keyboard.press('Alt+Space');
    await page.waitForFunction(()=>!document.querySelector('.stage').inert && getComputedStyle(document.querySelector('.stage')).opacity==='1');
    await page.getByRole('button', { name: 'Pinned', exact: true }).waitFor();
    await page.getByRole('button', { name: 'Pinned', exact: true }).click();
    await page.evaluate(()=>window.dispatchEvent(new Event('blur')));
    await page.waitForFunction(()=>document.querySelector('.stage').inert);
  });
  await record('Only start/end status is announced during streaming and Stop has a visible label', async () => {
    await open('solid');
    await page.evaluate(()=>{ window.liveChanges=[]; const region=document.querySelector('[aria-live="polite"]'); window.liveObserver=new MutationObserver(()=>window.liveChanges.push(region.textContent)); window.liveObserver.observe(region,{subtree:true,childList:true,characterData:true}); });
    await ask();
    await page.getByRole('button', { name: 'Stop', exact: true }).waitFor();
    assert.equal(await page.getByRole('button', { name: 'Stop', exact: true }).innerText(), 'Stop');
    await page.waitForTimeout(1200);
    assert.deepEqual(await page.evaluate(()=>window.liveChanges), ['Creating an answer.']);
    await page.getByRole('button', { name: 'Stop', exact: true }).click();
    await page.waitForFunction(()=>document.querySelector('[aria-live="polite"]').textContent==='Answer stopped.');
  });
  await record('Daily API allowance stops a second request; local loopback remains exempt', async () => {
    await open('solid&fast&budget=0.25');
    await ask('First request'); await page.getByRole('button', {name:'Formatted',exact:true}).waitFor();
    await ask('Second request'); await page.getByText('Daily API allowance reached. Wait until tomorrow or change Settings → Usage.',{exact:true}).waitFor();
    assert.match(await page.locator('[aria-live="polite"]').innerText(), /Answer failed.*Daily API allowance reached/);
    await open('solid&fast&budget=0.1&provider=ollama');
    await ask('Local request'); await page.getByRole('button', {name:'Formatted',exact:true}).waitFor();
  });
  await record('Subscription quota is unknown; provider warnings become visible', async () => {
    await open('solid&fast&limit-warning');
    await page.evaluate(async()=>{ const {app}=await import('/src/lib/state.svelte.ts'); app.saveSettings({defaultModel:{provider:'claude-cli',model:'sonnet',reasoning:''}}); });
    await page.getByText(/remaining quota unknown/).waitFor();
    await ask(); await page.getByText(/Claude reports that account usage is nearing a limit/).first().waitFor();
  });
  for (const [theme, width, height, scale] of [['light',760,680,100],['dark',640,440,200]]) {
    await page.setViewportSize({width,height});
    await open(`solid&fast&math&theme=${theme}&text-scale=${scale}`);
    await ask(); await page.getByRole('button',{name:'Formatted',exact:true}).waitFor();
    await capture(`bar-${theme}-${scale}`); await audit(`bar-${theme}-${scale}`);
    await page.locator('.chip.model').click();
    await capture(`picker-${theme}-${scale}`);
    const clipped = await page.locator('.segmented button, .search input').evaluateAll(nodes => nodes.filter(node => {
      const rect=node.getBoundingClientRect();
      const parent=node.parentElement.getBoundingClientRect();
      return rect.height < parseFloat(getComputedStyle(node).fontSize) * 1.2 || rect.left < parent.left - 1 || rect.right > parent.right + 1;
    }).map(node=>node.textContent || node.getAttribute('aria-label')));
    assert.deepEqual(clipped, [], 'Large-text model controls fit their content');
    await page.keyboard.press('Escape');
    await page.goto(`${base}/settings.html?reduced&solid&theme=${theme}&text-scale=${scale}`);
    await page.getByRole('heading',{name:'General',exact:true}).waitFor();
    await capture(`settings-${theme}-${scale}`); await audit(`settings-${theme}-${scale}`);
    await page.getByRole('button',{name:'Usage',exact:true}).click();
    await capture(`usage-${theme}-${scale}`); await audit(`usage-${theme}-${scale}`);
  }
  await record('General preferences update the document and Usage validates inputs',async()=>{
    await page.getByRole('button',{name:'General',exact:true}).click();
    await page.getByRole('combobox',{name:'Text size',exact:true}).selectOption('125');
    assert.equal(await page.evaluate(()=>getComputedStyle(document.documentElement).getPropertyValue('--text-scale')),'1.25');
    await page.getByRole('switch',{name:'Solid surfaces',exact:true}).click();
    assert.equal(await page.evaluate(()=>document.documentElement.dataset.solid),'false');
    await page.getByRole('button',{name:'Usage',exact:true}).click();
    await page.getByRole('spinbutton',{name:'Daily API allowance (USD)'}).fill('2.00');
    await page.keyboard.press('Tab');
    assert.match(await page.locator('.total').innerText(), /of \$2.00/);
  });
  await record('Windows contrast themes use system colors, borders and focus indicators',async()=>{
    await page.emulateMedia({forcedColors:'active'});
    await page.goto(`${base}/bar.html?reduced&fast&math&text-scale=200`);
    await page.getByRole('textbox',{name:'Ask',exact:true}).waitFor();
    await page.waitForFunction(()=>document.querySelector('.preview-toggle img')?.naturalWidth>0);
    await ask(); await page.getByRole('button',{name:'Formatted',exact:true}).waitFor();
    const controls=await page.locator('button').evaluateAll(items=>items.filter(e=>!e.disabled).map(e=>({border:getComputedStyle(e).borderTopStyle,color:getComputedStyle(e).color})));
    assert(controls.every(e=>e.border==='solid'));
    await capture('bar-forced-colors-200'); await audit('bar-forced-colors-200');
    await page.goto(`${base}/settings.html?reduced&text-scale=200`);
    await page.getByRole('heading',{name:'General',exact:true}).waitFor();
    await capture('settings-forced-colors-200'); await audit('settings-forced-colors-200');
  });
  await writeFile(path.join(output,'verification.json'),JSON.stringify({results,errors,audits,layout},null,2));
  assert.deepEqual(errors,[]);
  assert.deepEqual(audits.filter(a=>a.violations.length),[], 'WCAG automated checks');
  assert.deepEqual(layout.filter(l=>l.overflow||l.smallControls.length),[], 'Viewport and 24px target checks');
} finally {
  await writeFile(path.join(output,'verification.json'),JSON.stringify({results,errors,audits,layout},null,2));
  await browser.close();
}
