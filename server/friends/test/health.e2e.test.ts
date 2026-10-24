import assert from 'node:assert/strict';
import { after, before, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { TestServer } from './helpers.ts';

let s: TestServer;

before(async () => {
  s = await startServer();
});

after(async () => {
  await s.close();
});

test('GET /healthz is 200 ok without a token', async () => {
  const res = await s.api('GET', '/healthz');
  assert.equal(res.status, 200);
  assert.deepEqual(res.body, { ok: true });
});
