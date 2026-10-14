import { chromium } from 'playwright';
import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const project = fileURLToPath(new URL('../../', import.meta.url));
const base = process.env.AIASSISTANT_UI_TEST_URL || 'http://127.0.0.1:5747';
const output = path.join(project, 'build/ui-setup-tests');
await mkdir(output, { recursive: true });
const browser = await chromium.launch({ channel: 'msedge', headless: true });
const page = await browser.newPage({ viewport: { width: 900, height: 700 } });
const errors = [];
page.on('pageerror', error => errors.push(error.message));
const open = async (query = '') => {
  await page.goto(`${base}/settings.html?reduced&first-run&${query}`);
  await page.getByRole('heading', { name: 'Let’s get you connected' }).waitFor();
};
const record = async (name, fn) => { await fn(); console.log('PASS', name); };
try {
  await record('First run discovers a local provider and suggests a model', async () => {
    await open('theme=dark&hotkey-conflict');
    await page.waitForFunction(() => document.querySelector('.choice.selected')?.textContent.includes('Ollama'));
    await page.screenshot({ path: path.join(output, 'setup-dark.png') });
    assert.match(await page.locator('.choice.selected').innerText(), /Suggested.*Connected/);
    await page.getByRole('button', { name: 'Continue', exact: true }).click();
    assert.match(await page.locator('main').innerText(), /Ollama \(Local\).*screen and chat history are not sent/);
    assert.equal(await page.getByRole('button', { name: 'Continue to shortcut' }).isEnabled(), false);
  });
  await record('Only a received answer unlocks completion; shortcut conflict has alternatives', async () => {
    await page.getByRole('button', { name: 'Send test question' }).click();
    await page.getByText('Connected. Ollama answered:').waitFor();
    assert.match(await page.locator('blockquote').innerText(), /Hello/);
    await page.screenshot({ path: path.join(output, 'setup-answer.png') });
    await page.getByRole('button', { name: 'Continue to shortcut' }).click();
    assert.match(await page.locator('main').innerText(), /PowerToys Run and Flow Launcher/);
    assert.equal(await page.getByRole('button', { name: 'Finish setup' }).isEnabled(), false);
    assert.equal(await page.getByRole('button', { name: 'Alt + Space (in use)', exact: true }).isEnabled(), false);
    await page.getByRole('button', { name: 'Ctrl + Alt + Space', exact: true }).click();
    await page.waitForFunction(() => document.querySelector('.recorder')?.textContent.includes('Ctrl'));
    await page.getByRole('button', { name: 'Finish setup' }).click();
    await page.getByRole('heading', { name: 'General', exact: true }).waitFor();
    await page.getByRole('button', { name: 'Model', exact: true }).click();
    assert.match(await page.locator('main').innerText(), /Ollama/);
  });
  await record('Failure keeps the wizard on the test and explains recovery', async () => {
    await open('setup-error');
    await page.getByRole('button', { name: 'Continue', exact: true }).click();
    await page.getByRole('button', { name: 'Send test question' }).click();
    await page.getByRole('alert').waitFor();
    assert.match(await page.getByRole('alert').innerText(), /Check your sign-in/);
    assert.equal(await page.getByRole('button', { name: 'Continue to shortcut' }).isEnabled(), false);
  });
  await record('Cancellation does not count as a successful answer', async () => {
    await open('slow-test');
    await page.getByRole('button', { name: 'Continue', exact: true }).click();
    await page.getByRole('button', { name: 'Send test question' }).click();
    await page.getByRole('button', { name: 'Stop test' }).click();
    await page.getByRole('alert').waitFor();
    assert.match(await page.getByRole('alert').innerText(), /Test stopped/);
    assert.equal(await page.getByRole('button', { name: 'Continue to shortcut' }).isEnabled(), false);
  });
  await record('Missing runtime and providers have a download link and no false ready state', async () => {
    await open('theme=light&no-node&no-providers');
    await page.getByRole('button', { name: /ChatGPT Existing subscription/ }).click();
    await page.getByRole('button', { name: 'Download Node.js' }).waitFor();
    assert.equal(await page.getByRole('button', { name: 'Continue', exact: true }).isEnabled(), false);
    await page.setViewportSize({ width: 720, height: 520 });
    await page.screenshot({ path: path.join(output, 'setup-missing-light-minimum.png') });
    assert.equal(await page.evaluate(() => document.documentElement.scrollWidth > innerWidth), false);
  });
  await record('API-key route provides a model and a Cloud test destination', async () => {
    await page.setViewportSize({ width: 900, height: 700 });
    await open('theme=light');
    await page.getByLabel('Use an API key or another provider').selectOption('openai');
    await page.getByRole('textbox', { name: 'OpenAI API key' }).fill('synthetic-key');
    await page.getByRole('button', { name: 'Save', exact: true }).click();
    await page.getByRole('button', { name: 'Continue', exact: true }).click();
    assert.match(await page.locator('main').innerText(), /OpenAI \(Cloud\)/);
    await page.getByRole('button', { name: 'Send test question' }).click();
    await page.getByText('Connected. OpenAI answered:').waitFor();
    await page.screenshot({ path: path.join(output, 'setup-api-light.png') });
  });
  await record('Providers overview lists every provider with status and next step', async () => {
    await page.getByRole('button', { name: 'Providers', exact: true }).click();
    await page.getByRole('heading', { name: 'What’s connected' }).waitFor();
    assert.equal(await page.locator('.overview li').count(), 14);
    assert.match(await page.locator('.overview').innerText(), /Not connected/);
    await page.locator('.overview').getByRole('button', { name: /LM Studio/ }).click();
    assert.equal(await page.locator('#provider-lmstudio .head-main').getAttribute('aria-expanded'), 'true');
  });
  await record('Existing installations open normal Settings', async () => {
    await page.goto(`${base}/settings.html?reduced`);
    await page.getByRole('heading', { name: 'General', exact: true }).waitFor();
    assert.equal(await page.getByRole('heading', { name: 'Let’s get you connected' }).count(), 0);
  });
  assert.deepEqual(errors, []);
} finally { await browser.close(); }
