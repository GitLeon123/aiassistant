import http from 'node:http';
import { spawn } from 'node:child_process';
import assert from 'node:assert/strict';
import path from 'node:path';

const executable = path.resolve(process.argv[2] || 'build/phase2/Release/aiassistant_tests.exe');
let answers = 0, redirectsFollowed = 0;
const server = http.createServer(async (request, response) => {
  if (request.url === '/v1/redirect/models') {
    response.writeHead(302, { Location: '/trap' }); response.end(); return;
  }
  if (request.url === '/trap') { redirectsFollowed++; response.end('{}'); return; }
  if (request.url === '/v1/empty/models') { response.end('{"data":[]}'); return; }
  if (request.url === '/v1/malformed/models') { response.end('{"other":"not a model list"}'); return; }
  if (request.url === '/v1/models') { response.end('{"data":[{"id":"synthetic-model"}]}'); return; }
  if (request.url === '/v1/chat/completions') {
    try {
      const chunks = [];
      for await (const chunk of request) chunks.push(chunk);
      const body = JSON.parse(Buffer.concat(chunks).toString());
      assert.equal(body.model, 'synthetic-model');
      assert.equal(body.stream, true);
      assert.equal(body.messages.length, 2);
      assert.equal(body.messages[0].role, 'system');
      assert.deepEqual(body.messages[1], { role: 'user', content: 'Say hello in one short sentence.' });
      answers++;
      response.writeHead(200, { 'Content-Type': 'text/event-stream' });
      response.end('data: {"choices":[{"delta":{"content":"Hello from the synthetic server!"},"finish_reason":null}]}\n\ndata: {"choices":[{"delta":{},"finish_reason":"stop"}]}\n\ndata: [DONE]\n\n');
    } catch (error) { response.writeHead(400); response.end(String(error)); }
    return;
  }
  response.writeHead(404); response.end();
});
await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
try {
  const endpoint = `http://127.0.0.1:${server.address().port}/v1`;
  const child = spawn(executable, ['--test-case=*setup*,*discovery*'], {
    env: { ...process.env, AIASSISTANT_SETUP_TEST_ENDPOINT: endpoint }, stdio: 'inherit', windowsHide: true,
  });
  const code = await new Promise((resolve, reject) => { child.on('error', reject); child.on('exit', resolve); });
  assert.equal(code, 0);
  assert.equal(answers, 1);
  assert.equal(redirectsFollowed, 0);
  console.log('PASS native loopback discovery, malformed/empty listings, redirect refusal and isolated HTTP test answer');
} finally { await new Promise(resolve => server.close(resolve)); }
