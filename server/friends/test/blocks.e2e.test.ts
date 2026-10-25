import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { makeUuid, startServer } from './helpers.ts';
import type { ErrorBody, Player, TestServer, UserRef } from './helpers.ts';

interface BlockedBody {
  blocked: Array<UserRef & { createdAt: string }>;
}

describe('blocks', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  async function block(by: Player, target: Player): Promise<number> {
    return (await s.api('PUT', `/v1/blocks/${target.uuid}`, { token: by.token })).status;
  }

  async function everyone(player: Player): Promise<void> {
    await s.api('PATCH', '/v1/me/settings', { token: player.token, body: { visibility: 'everyone' } });
  }

  async function sendRequest(from: Player, to: Player): Promise<{ status: number; body: ErrorBody }> {
    return s.api<ErrorBody>('POST', '/v1/friends/requests', { token: from.token, body: { name: to.name } });
  }

  test('blocking is 204 and ends the friendship', async () => {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    await s.makeFriends(a, b);
    assert.equal(await block(a, b), 204);
    assert.deepEqual((await s.friendsOf(a)).friends, []);
    assert.deepEqual((await s.friendsOf(b)).friends, []);
  });

  test('a block racing an accept never leaves them friends', async () => {
    for (let round = 0; round < 15; round += 1) {
      const blocker = await s.newPlayer();
      const other = await s.newPlayer();
      await sendRequest(blocker, other);
      await Promise.all([
        s.api('POST', `/v1/friends/requests/${blocker.uuid}/accept`, { token: other.token }),
        block(blocker, other),
      ]);
      assert.deepEqual((await s.friendsOf(other)).friends, []);
      assert.deepEqual((await s.friendsOf(blocker)).friends, []);
    }
  });

  test('blocking removes pending requests in both directions', async () => {
    const a = await s.newPlayer();
    const sentTo = await s.newPlayer();
    const sentBy = await s.newPlayer();
    await sendRequest(a, sentTo);
    await sendRequest(sentBy, a);
    assert.equal(await block(a, sentTo), 204);
    assert.equal(await block(a, sentBy), 204);

    assert.deepEqual((await s.friendsOf(a)).requests, { incoming: [], outgoing: [] });
    assert.deepEqual((await s.friendsOf(sentTo)).requests.incoming, []);
    assert.deepEqual((await s.friendsOf(sentBy)).requests.outgoing, []);
  });

  test('a blocked player sending a request to the blocker gets 404 not_found', async () => {
    const blocker = await s.newPlayer();
    const blocked = await s.newPlayer();
    await block(blocker, blocked);
    const res = await sendRequest(blocked, blocker);
    assert.equal(res.status, 404);
    assert.equal(res.body.error, 'not_found');
    assert.deepEqual((await s.friendsOf(blocker)).requests.incoming, []);
  });

  test('the blocker sending a request to the blocked gets 409 blocked_by_you', async () => {
    const blocker = await s.newPlayer();
    const blocked = await s.newPlayer();
    await block(blocker, blocked);
    const res = await sendRequest(blocker, blocked);
    assert.equal(res.status, 409);
    assert.equal(res.body.error, 'blocked_by_you');
  });

  test('status lookup is 404 in both directions even with visibility everyone', async () => {
    const blocker = await s.newPlayer();
    const blocked = await s.newPlayer();
    await everyone(blocker);
    await everyone(blocked);
    const initial = await s.api('GET', `/v1/players/${blocked.name}/status`, { token: blocker.token });
    assert.equal(initial.status, 200);

    await block(blocker, blocked);
    const byBlocker = await s.api<ErrorBody>('GET', `/v1/players/${blocked.name}/status`, { token: blocker.token });
    assert.equal(byBlocker.status, 404);
    assert.equal(byBlocker.body.error, 'not_found');
    const byBlocked = await s.api<ErrorBody>('GET', `/v1/players/${blocker.name}/status`, { token: blocked.token });
    assert.equal(byBlocked.status, 404);
    assert.equal(byBlocked.body.error, 'not_found');
  });

  test('GET /v1/blocks lists who you blocked', async () => {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    const c = await s.newPlayer();
    const empty = await s.api<BlockedBody>('GET', '/v1/blocks', { token: a.token });
    assert.deepEqual(empty.body, { blocked: [] });

    await block(a, b);
    s.clock.advance(10);
    await block(a, c);
    const res = await s.api<BlockedBody>('GET', '/v1/blocks', { token: a.token });
    assert.equal(res.status, 200);
    assert.deepEqual(
      res.body.blocked.map((entry) => entry.uuid).sort(),
      [b.uuid, c.uuid].sort(),
    );
    const entry = res.body.blocked.find((candidate) => candidate.uuid === c.uuid);
    assert.deepEqual(entry, { uuid: c.uuid, name: c.name, createdAt: s.clock.iso() });

    const theirs = await s.api<BlockedBody>('GET', '/v1/blocks', { token: b.token });
    assert.deepEqual(theirs.body, { blocked: [] });
  });

  test('unblock is 204 and a second unblock is 404', async () => {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    await block(a, b);
    const res = await s.api('DELETE', `/v1/blocks/${b.uuid}`, { token: a.token });
    assert.equal(res.status, 204);
    assert.deepEqual((await s.api<BlockedBody>('GET', '/v1/blocks', { token: a.token })).body, { blocked: [] });

    const again = await s.api<ErrorBody>('DELETE', `/v1/blocks/${b.uuid}`, { token: a.token });
    assert.equal(again.status, 404);
    assert.equal(again.body.error, 'not_found');
  });

  test('after unblocking, requests work again in both directions', async () => {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    await block(a, b);
    await s.api('DELETE', `/v1/blocks/${b.uuid}`, { token: a.token });
    assert.equal((await sendRequest(b, a)).status, 201);
  });

  test('unblocking someone you did not block is 404', async () => {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    const res = await s.api<ErrorBody>('DELETE', `/v1/blocks/${b.uuid}`, { token: a.token });
    assert.equal(res.status, 404);
  });

  test('blocking an unknown uuid is 404 not_found', async () => {
    const a = await s.newPlayer();
    const res = await s.api<ErrorBody>('PUT', `/v1/blocks/${makeUuid(31_337)}`, { token: a.token });
    assert.equal(res.status, 404);
    assert.equal(res.body.error, 'not_found');
  });

  test('a malformed uuid is 400', async () => {
    const a = await s.newPlayer();
    assert.equal((await s.api('PUT', '/v1/blocks/nope', { token: a.token })).status, 400);
    assert.equal((await s.api('DELETE', '/v1/blocks/nope', { token: a.token })).status, 400);
  });

  test('a block does not show up in the blocked player’s lists', async () => {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    await block(a, b);
    const theirs = await s.friendsOf(b);
    assert.deepEqual(theirs, { friends: [], requests: { incoming: [], outgoing: [] } });
  });

  test('blocking needs a token', async () => {
    const res = await s.api<ErrorBody>('GET', '/v1/blocks');
    assert.equal(res.status, 401);
    assert.equal(res.body.error, 'unauthorized');
  });
});
