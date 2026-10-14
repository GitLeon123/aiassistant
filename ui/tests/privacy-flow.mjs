import { chromium } from 'playwright';
import assert from 'node:assert/strict';
import { mkdir, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

// Run with the Vite dev server active. Only the synthetic browser mock is used.
const project = fileURLToPath(new URL('../../', import.meta.url));
const base = (process.env.AIASSISTANT_UI_TEST_URL || 'http://127.0.0.1:5747').replace(/\/$/, '');
const output = process.env.AIASSISTANT_UI_TEST_ARTIFACTS || path.join(project, 'build/ui-browser-tests');
await mkdir(output, { recursive: true });
const browser = await chromium.launch({ channel: 'msedge', headless: true });
const page = await browser.newPage({ viewport: { width: 760, height: 680 }, deviceScaleFactor: 1 });
const errors = [];
page.on('pageerror', error => errors.push(error.message));
const results = [];
async function record(name, run) { await run(); results.push(name); console.log('PASS', name); }
async function open(query = '') {
  await page.goto(`${base}/bar.html?reduced&${query}`);
  await page.waitForFunction(() => getComputedStyle(document.querySelector('.stage')).opacity === '1');
}
async function ready() {
  await page.locator('.preview-toggle img').waitFor();
  await page.waitForFunction(() => document.querySelector('.preview-toggle img')?.naturalWidth > 0);
}
try {
  await open('theme=dark');
  await ready();
  await record('Cloud destination, monitor metadata, loaded preview and send gating', async () => {
    assert.match(await page.locator('.destination').innerText(), /Screen → Anthropic.*\(Cloud\).*api\.anthropic\.com/);
    assert.match(await page.locator('.area').innerText(), /Monitor 2, 760×680/);
    await page.getByRole('textbox', { name: 'Ask', exact: true }).fill('Synthetic screenshot question');
    assert.equal(await page.getByRole('button', { name: 'Send', exact: true }).isEnabled(), true);
    await page.screenshot({ path: path.join(output, 'bar-dark.png') });
  });
  await record('Expand/collapse preview and remove switches Screen off', async () => {
    await page.getByRole('button', { name: 'Enlarge screenshot preview' }).click();
    assert.equal(await page.locator('.preview-toggle').getAttribute('aria-expanded'), 'true');
    assert.equal(await page.locator('.full-preview img').getAttribute('src'), await page.locator('.preview-toggle img').getAttribute('src'));
    await page.keyboard.press('Escape');
    assert.equal(await page.locator('.full-preview').count(), 0);
    await page.getByRole('button', { name: 'Remove screenshot' }).click();
    assert.equal(await page.locator('.chip.screen').getAttribute('aria-pressed'), 'false');
    assert.equal(await page.locator('.attachment').count(), 0);
    await page.keyboard.press('Control+s');
    await ready();
  });
  await record('Window mode changes the frozen crop and keeps it sendable', async () => {
    await page.getByRole('button', { name: 'Use only Code.exe' }).click();
    await page.waitForFunction(() => document.querySelector('.area')?.textContent.includes('Code.exe'));
    await ready();
    const dims = await page.locator('.preview-toggle img').evaluate(img => [img.naturalWidth, img.naturalHeight]);
    assert.deepEqual(dims, [638, 530]);
    assert.equal(await page.getByRole('button', { name: 'Send', exact: true }).isEnabled(), true);
    await page.getByRole('button', { name: 'Send', exact: true }).click();
    await page.getByText('Earlier screenshots are included in this conversation. New chat clears them.', { exact: true }).waitFor({ timeout: 20000 });
    assert.equal(await page.locator('[role=alert]').count(), 0);
    await page.keyboard.press('Control+n');
    assert.equal(await page.locator('.turn').count(), 0);
  });
  await record('Closing drops image elements; reopening produces a fresh preview', async () => {
    await page.keyboard.press('Escape');
    await page.waitForFunction(() => getComputedStyle(document.querySelector('.stage')).opacity === '0');
    assert.equal(await page.locator('.attachment').count(), 0);
    await page.keyboard.press('Alt+Space');
    await ready();
  });
  for (const [fixture, reason] of [['password','Password manager'],['private','Private browser window'],['banking','Banking window'],['remote','Remote desktop']]) {
    await record(`Block ${fixture} fixture before preview/backdrop`, async () => {
      await open(`theme=light&blocked=${fixture}`);
      assert.match(await page.locator('.capture-warning').innerText(), new RegExp(reason));
      assert.equal(await page.locator('.chip.screen').isDisabled(), true);
      assert.equal(await page.locator('.attachment').count(), 0);
    });
  }
  await page.screenshot({ path: path.join(output, 'bar-blocked.png') });
  await record('Failed preview blocks sending; removal restores text-only sending', async () => {
    await open('theme=dark&preview-error');
    await page.getByText('Screenshot preview failed. Remove it to send text only, or reopen the bar.', { exact: true }).waitFor();
    await page.getByRole('textbox', { name: 'Ask', exact: true }).fill('Synthetic text question');
    assert.equal(await page.getByRole('button', { name: 'Send', exact: true }).isDisabled(), true);
    assert.equal(await page.getByRole('button', { name: 'Screenshot preview unavailable' }).isDisabled(), true);
    assert.equal(await page.locator('.attachment img, .chip.screen img').count(), 0);
    await page.screenshot({ path: path.join(output, 'bar-failed-preview.png') });
    await page.getByRole('button', { name: 'Remove screenshot' }).click();
    assert.equal(await page.getByRole('button', { name: 'Send', exact: true }).isEnabled(), true);
    assert.equal(await page.locator('.capture-warning').count(), 0);
  });
  await record('Loopback Ollama is Local; remote Ollama is Cloud', async () => {
    await open('theme=light&provider=ollama');
    await ready();
    assert.match(await page.locator('.destination').innerText(), /Screen → Ollama.*\(Local\).*localhost:11434/);
    await page.getByRole('button', { name: 'Enlarge screenshot preview' }).click();
    await page.waitForFunction(() => document.querySelector('.full-preview img')?.naturalWidth > 0);
    await page.screenshot({ path: path.join(output, 'bar-light-expanded.png') });
    await open('theme=light&provider=ollama&endpoint=http://remote.example:11434/v1');
    await ready();
    assert.match(await page.locator('.destination').innerText(), /\(Cloud\).*remote\.example:11434/);
  });
  await record('A text-only model disables screenshot sharing', async () => {
    await page.evaluate(async () => {
      const { app } = await import('/src/lib/state.svelte.ts');
      app.saveSettings({ defaultModel: { provider: 'ollama', model: 'qwen3:14b', reasoning: '' } });
    });
    await page.waitForFunction(() => document.querySelector('.chip.screen').disabled);
    assert.equal(await page.locator('.attachment').count(), 0);
    assert.equal(await page.locator('.chip.screen').getAttribute('aria-pressed'), 'false');
  });
  await page.setViewportSize({ width: 980, height: 700 });
  await record('Privacy rule editor adds/removes rules, restores defaults and toggles protection', async () => {
    await page.goto(`${base}/settings.html?theme=dark&reduced`);
    await page.getByRole('button', { name: 'Privacy', exact: true }).click();
    await page.getByText('Screen capture blocklist', { exact: true }).waitFor();
    const initial = await page.locator('.rules li').count();
    await page.screenshot({ path: path.join(output, 'settings-dark.png') });
    await page.getByRole('textbox', { name: 'Blocked process name' }).fill('vault*.exe');
    await page.getByRole('textbox', { name: 'Blocked window title' }).fill('Account');
    await page.getByRole('textbox', { name: 'Block reason', exact: true }).fill('Synthetic private account');
    await page.getByRole('button', { name: 'Add rule', exact: true }).click();
    await page.waitForFunction(n => document.querySelectorAll('.rules li').length === n, initial + 1);
    await page.waitForFunction(() => document.querySelector('[aria-label="Blocked process name"]').value === '', undefined, { timeout: 5000 });
    await page.getByRole('button', { name: new RegExp(`Remove rule ${initial + 1}: Synthetic`) }).click();
    assert.equal(await page.locator('.rules li').count(), initial);
    await page.getByRole('button', { name: 'Restore defaults' }).click();
    assert.equal(await page.locator('.rules li').count(), initial);
    await page.getByRole('switch', { name: 'Block sensitive windows', exact: true }).click();
    await page.getByText('Protection is off. These rules will not prevent capture.', { exact: true }).waitFor();
    await page.getByRole('switch', { name: 'Block sensitive windows', exact: true }).click();
    assert.equal(await page.locator('.protection-off').count(), 0);
    await page.getByRole('button', { name: 'Add rule', exact: true }).click();
    assert.match(await page.locator('[role=alert]').innerText(), /Enter a process name/);
  });
  await page.goto(`${base}/settings.html?theme=light&reduced`);
  await page.getByRole('button', { name: 'Privacy', exact: true }).click();
  await page.getByText('Screen capture blocklist', { exact: true }).waitFor();
  await page.screenshot({ path: path.join(output, 'settings-light.png') });
  await page.getByRole('button', { name: 'Add rule', exact: true }).scrollIntoViewIfNeeded();
  await page.screenshot({ path: path.join(output, 'settings-light-editor.png') });
  assert.deepEqual(errors, []);
  console.log('PASS no uncaught browser errors');
  await writeFile(path.join(output, 'verification.json'), JSON.stringify({ results, errors }, null, 2));
} finally { await browser.close(); }
