import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { ErrorBody, MeBody, Player, TestServer } from './helpers.ts';

describe('deleting an account', () => {
  let s: TestServer;
  let leaver: Player;
  let friend: Player;
  let requestedBy: Player;
  let requestedTo: Player;
  let blockedBy: Player;
  let blocked: Player;
  let leaverUuid: string;
  let leaverName: string;

  before(async () => {
    s = await startServer();
    leaver = await s.newPlayer();
    leaverUuid = leaver.uuid;
    leaverName = leaver.name;
    friend = await s.newPlayer();
    requestedBy = await s.newPlayer();
    requestedTo = await s.newPlayer();
    blockedBy = await s.newPlayer();
    blocked = await s.newPlayer();

    await s.makeFriends(leaver, friend);
    await s.api('POST', '/v1/friends/requests', { token: requestedBy.token, body: { name: leaver.name } });
    await s.api('POST', '/v1/friends/requests', { token: leaver.token, body: { name: requestedTo.name } });
    await s.api('PUT', `/v1/blocks/${leaver.uuid}`, { token: blockedBy.token });
    await s.api('PUT', `/v1/blocks/${blocked.uuid}`, { token: leaver.token });
    await s.api('PATCH', '/v1/me/settings', {
      token: leaver.token,
      body: { visibility: 'everyone', hideServer: true, shareIpServers: true },
    });
    await s.api('PUT', '/v1/me/status', { token: leaver.token, body: { state: 'online' } });
  });

  after(async () => {
    await s.close();
  });

  test('is 204', async () => {
    const res = await s.api('DELETE', '/v1/me', { token: leaver.token });
    assert.equal(res.status, 204);
  });

  test('the token stops working', async () => {
    const res = await s.api<ErrorBody>('GET', '/v1/me', { token: leaver.token });
    assert.equal(res.status, 401);
    assert.equal(res.body.error, 'unauthorized');
  });

  test('the friend no longer has them', async () => {
    assert.deepEqual((await s.friendsOf(friend)).friends, []);
  });

  test('requests from and to them are gone', async () => {
    assert.deepEqual((await s.friendsOf(requestedBy)).requests.outgoing, []);
    assert.deepEqual((await s.friendsOf(requestedTo)).requests.incoming, []);
  });

  test('they can no longer be found by name', async () => {
    const lookup = await s.api<ErrorBody>('GET', `/v1/players/${leaverName}/status`, { token: friend.token });
    assert.equal(lookup.status, 404);
    const request = await s.api<ErrorBody>('POST', '/v1/friends/requests', { token: friend.token, body: { name: leaverName } });
    assert.equal(request.status, 404);
    assert.equal(request.body.error, 'not_found');
  });

  test('signing in again gives an empty account with default settings', async () => {
    const token = await s.signIn(leaverName, leaverUuid);
    const me = await s.api<MeBody>('GET', '/v1/me', { token });
    assert.deepEqual(me.body, {
      uuid: leaverUuid,
      name: leaverName,
      settings: { visibility: 'friends', hideServer: false, shareIpServers: false },
    });
    const again: Player = { uuid: leaverUuid, name: leaverName, token };
    assert.deepEqual(await s.friendsOf(again), { friends: [], requests: { incoming: [], outgoing: [] } });
    const blocks = await s.api('GET', '/v1/blocks', { token });
    assert.deepEqual(blocks.body, { blocked: [] });
  });

  test('blocks in both directions were deleted too', async () => {
    const againToken = await s.signIn(leaverName, leaverUuid);
    const toBlocker = await s.api('POST', '/v1/friends/requests', { token: againToken, body: { name: blockedBy.name } });
    assert.equal(toBlocker.status, 201);
    const toBlocked = await s.api('POST', '/v1/friends/requests', { token: againToken, body: { name: blocked.name } });
    assert.equal(toBlocked.status, 201);
  });

  test('the old status is gone', async () => {
    const token = await s.signIn(leaverName, leaverUuid);
    const me: Player = { uuid: leaverUuid, name: leaverName, token };
    await s.makeFriends(me, friend);
    const seen = (await s.friendsOf(friend)).friends.find((f) => f.uuid === leaverUuid);
    assert.deepEqual(seen?.status, { state: 'offline', lastSeenAt: null });
  });

  test('needs a token', async () => {
    const res = await s.api<ErrorBody>('DELETE', '/v1/me');
    assert.equal(res.status, 401);
  });
});
