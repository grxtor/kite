import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { ErrorBody, MeBody, Settings, TestServer } from './helpers.ts';

describe('me and settings', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  const defaults: Settings = { visibility: 'friends', hideServer: false, shareIpServers: false };

  test('GET /v1/me returns uuid, name and default settings', async () => {
    const me = await s.newPlayer();
    const res = await s.api<MeBody>('GET', '/v1/me', { token: me.token });
    assert.equal(res.status, 200);
    assert.deepEqual(res.body, { uuid: me.uuid, name: me.name, settings: defaults });
  });

  test('PATCH returns the full settings object', async () => {
    const me = await s.newPlayer();
    const res = await s.api<Settings>('PATCH', '/v1/me/settings', { token: me.token, body: { visibility: 'everyone' } });
    assert.equal(res.status, 200);
    assert.deepEqual(res.body, { ...defaults, visibility: 'everyone' });
  });

  test('PATCH accepts any subset and keeps the rest', async () => {
    const me = await s.newPlayer();
    await s.api('PATCH', '/v1/me/settings', { token: me.token, body: { visibility: 'invisible', hideServer: true } });
    const res = await s.api<Settings>('PATCH', '/v1/me/settings', { token: me.token, body: { shareIpServers: true } });
    assert.deepEqual(res.body, { visibility: 'invisible', hideServer: true, shareIpServers: true });
    const mine = await s.api<MeBody>('GET', '/v1/me', { token: me.token });
    assert.deepEqual(mine.body.settings, res.body);
  });

  test('PATCH with an empty body returns the current settings', async () => {
    const me = await s.newPlayer();
    const res = await s.api<Settings>('PATCH', '/v1/me/settings', { token: me.token, body: {} });
    assert.equal(res.status, 200);
    assert.deepEqual(res.body, defaults);
  });

  for (const visibility of ['friends', 'everyone', 'invisible']) {
    test(`visibility ${visibility} is accepted`, async () => {
      const me = await s.newPlayer();
      const res = await s.api<Settings>('PATCH', '/v1/me/settings', { token: me.token, body: { visibility } });
      assert.equal(res.status, 200);
      assert.equal(res.body.visibility, visibility);
    });
  }

  const rejected: Array<[string, unknown]> = [
    ['an unknown visibility', { visibility: 'public' }],
    ['a visibility that is not a string', { visibility: true }],
    ['hideServer that is not a boolean', { hideServer: 'yes' }],
    ['shareIpServers that is not a boolean', { shareIpServers: 'maybe' }],
  ];
  for (const [title, body] of rejected) {
    test(`PATCH rejects ${title}`, async () => {
      const me = await s.newPlayer();
      const res = await s.api<ErrorBody>('PATCH', '/v1/me/settings', { token: me.token, body });
      assert.equal(res.status, 400);
      assert.equal(res.body.error, 'bad_request');
      const mine = await s.api<MeBody>('GET', '/v1/me', { token: me.token });
      assert.deepEqual(mine.body.settings, defaults);
    });
  }

  test('settings belong to one account', async () => {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    await s.api('PATCH', '/v1/me/settings', { token: a.token, body: { hideServer: true } });
    const res = await s.api<MeBody>('GET', '/v1/me', { token: b.token });
    assert.deepEqual(res.body.settings, defaults);
  });

  test('both endpoints need a token', async () => {
    assert.equal((await s.api('GET', '/v1/me')).status, 401);
    assert.equal((await s.api('PATCH', '/v1/me/settings', { body: {} })).status, 401);
  });
});
