import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { ErrorBody, StatusBody, TestServer, UserRef } from './helpers.ts';

interface LookupBody extends UserRef {
  status: StatusBody;
}

describe('player lookup', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  const lookup = (token: string, name: string) =>
    s.api<LookupBody & ErrorBody>('GET', `/v1/players/${name}/status`, { token });

  test('a non-friend with default visibility is 404 not_found', async () => {
    const target = await s.newPlayer();
    const asker = await s.newPlayer();
    await s.api('PUT', '/v1/me/status', { token: target.token, body: { state: 'online' } });
    const res = await lookup(asker.token, target.name);
    assert.equal(res.status, 404);
    assert.equal(res.body.error, 'not_found');
  });

  test('visibility everyone makes the status public to signed-in users', async () => {
    const target = await s.newPlayer();
    const asker = await s.newPlayer();
    await s.api('PATCH', '/v1/me/settings', { token: target.token, body: { visibility: 'everyone' } });
    await s.api('PUT', '/v1/me/status', { token: target.token, body: { state: 'online' } });
    const res = await lookup(asker.token, target.name);
    assert.equal(res.status, 200);
    assert.deepEqual(res.body, { uuid: target.uuid, name: target.name, status: { state: 'online' } });
  });

  test('a friend can look up a player with default visibility', async () => {
    const target = await s.newPlayer();
    const friend = await s.newPlayer();
    await s.makeFriends(target, friend);
    await s.api('PUT', '/v1/me/status', { token: target.token, body: { state: 'online' } });
    const res = await lookup(friend.token, target.name);
    assert.equal(res.status, 200);
    assert.deepEqual(res.body.status, { state: 'online' });
  });

  test('you can look yourself up', async () => {
    const me = await s.newPlayer();
    await s.api('PUT', '/v1/me/status', { token: me.token, body: { state: 'online' } });
    const res = await lookup(me.token, me.name);
    assert.equal(res.status, 200);
    assert.deepEqual(res.body, { uuid: me.uuid, name: me.name, status: { state: 'online' } });
  });

  test('an unknown name is 404 not_found', async () => {
    const me = await s.newPlayer();
    const res = await lookup(me.token, 'NobodyHere');
    assert.equal(res.status, 404);
    assert.equal(res.body.error, 'not_found');
  });

  test('an invisible non-friend is 404', async () => {
    const target = await s.newPlayer();
    const asker = await s.newPlayer();
    await s.api('PATCH', '/v1/me/settings', { token: target.token, body: { visibility: 'invisible' } });
    const res = await lookup(asker.token, target.name);
    assert.equal(res.status, 404);
    assert.equal(res.body.error, 'not_found');
  });

  test('an invisible friend looks offline with no last seen time', async () => {
    const target = await s.newPlayer();
    const friend = await s.newPlayer();
    await s.makeFriends(target, friend);
    await s.api('PATCH', '/v1/me/settings', { token: target.token, body: { visibility: 'invisible' } });
    const res = await lookup(friend.token, target.name);
    assert.equal(res.status, 200);
    assert.deepEqual(res.body.status, { state: 'offline', lastSeenAt: null });
  });

  test('going back to friends only hides the status again', async () => {
    const target = await s.newPlayer();
    const asker = await s.newPlayer();
    await s.api('PATCH', '/v1/me/settings', { token: target.token, body: { visibility: 'everyone' } });
    assert.equal((await lookup(asker.token, target.name)).status, 200);
    await s.api('PATCH', '/v1/me/settings', { token: target.token, body: { visibility: 'friends' } });
    assert.equal((await lookup(asker.token, target.name)).status, 404);
  });

  test('settings are applied to what a lookup shows', async () => {
    const target = await s.newPlayer();
    const asker = await s.newPlayer();
    await s.api('PATCH', '/v1/me/settings', { token: target.token, body: { visibility: 'everyone' } });
    await s.api('PUT', '/v1/me/status', {
      token: target.token,
      body: { state: 'playing', instance: 'i', version: '1.21.4', world: { type: 'server', host: '192.168.1.20' } },
    });
    const res = await lookup(asker.token, target.name);
    assert.deepEqual(res.body.status.world, { type: 'server' });
  });

  test('needs a token', async () => {
    const res = await s.api<ErrorBody>('GET', '/v1/players/Notch/status');
    assert.equal(res.status, 401);
    assert.equal(res.body.error, 'unauthorized');
  });
});
