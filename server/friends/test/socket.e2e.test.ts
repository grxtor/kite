import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { closeCodes, maxConnectionsPerUser, maxFrameBytes } from '../src/routes/socket.ts';
import { makeUuid, startServer } from './helpers.ts';
import type { Player, TestServer } from './helpers.ts';
import { refusedUpgradeCloses, silentSocket, TestSocket, upgradeStatus } from './socketClient.ts';

const DAY = 24 * 60 * 60;

function wsUrl(s: TestServer): string {
  return `${s.baseUrl.replace('http', 'ws')}/v1/ws`;
}

describe('socket', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  async function friends(): Promise<{ a: Player; b: Player }> {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    await s.makeFriends(a, b);
    return { a, b };
  }

  describe('signing in on the upgrade', () => {
    test('a valid token gets a ready frame with the player', async () => {
      const a = await s.newPlayer();
      await s.connect(a);
    });

    test('no token is 401', async () => {
      assert.equal(await upgradeStatus(s.baseUrl, {}), 401);
    });

    test('a malformed token is 401', async () => {
      assert.equal(await upgradeStatus(s.baseUrl, { authorization: 'Bearer nope' }), 401);
    });

    test('an unknown token is 401', async () => {
      const token = `kfs_${'A'.repeat(43)}`;
      assert.equal(await upgradeStatus(s.baseUrl, { authorization: `Bearer ${token}` }), 401);
    });

    test('a logged-out token is 401', async () => {
      const a = await s.newPlayer();
      await s.api('POST', '/v1/auth/logout', { token: a.token });
      assert.equal(await upgradeStatus(s.baseUrl, { authorization: `Bearer ${a.token}` }), 401);
    });

    test('a token in the query string is not accepted', async () => {
      const a = await s.newPlayer();
      const socket = new TestSocket(`${wsUrl(s)}?token=${a.token}`, undefined);
      const closed = await socket.closed;
      assert.equal(closed.code, 1006);
    });

    test('a plain GET is 426', async () => {
      const a = await s.newPlayer();
      const res = await s.api<{ error: string }>('GET', '/v1/ws', { token: a.token });
      assert.equal(res.status, 426);
      assert.equal(res.body.error, 'bad_request');
    });
  });

  describe('the session ending', () => {
    test('logging out closes the sockets of that token only', async () => {
      const a = await s.newPlayer();
      const other: Player = { ...a, token: await s.signIn(a.name, a.uuid) };
      const first = await s.connect(a);
      const second = await s.connect(other);
      await s.api('POST', '/v1/auth/logout', { token: a.token });
      assert.deepEqual(await first.closed, { code: closeCodes.sessionEnded, reason: 'Session ended' });
      second.send({ type: 'nope' });
      assert.equal((await second.next('error')).error, 'bad_request');
    });

    test('deleting the account closes every socket', async () => {
      const a = await s.newPlayer();
      const other: Player = { ...a, token: await s.signIn(a.name, a.uuid) };
      const sockets = [await s.connect(a), await s.connect(other)];
      await s.api('DELETE', '/v1/me', { token: a.token });
      for (const socket of sockets) assert.equal((await socket.closed).code, closeCodes.sessionEnded);
    });

    test('a token dropped by the eleventh sign-in closes its socket', async () => {
      const a = await s.newPlayer();
      const socket = await s.connect(a);
      for (let i = 0; i < 10; i += 1) await s.signIn(a.name, a.uuid);
      assert.equal((await socket.closed).code, closeCodes.sessionEnded);
    });
  });

  describe('limits', () => {
    test(`a ${maxConnectionsPerUser + 1}th socket closes the oldest`, async () => {
      const a = await s.newPlayer();
      const sockets: TestSocket[] = [];
      for (let i = 0; i < maxConnectionsPerUser; i += 1) sockets.push(await s.connect(a));
      const newest = await s.connect(a);
      assert.deepEqual(await sockets[0]!.closed, { code: closeCodes.tooManyConnections, reason: 'Too many connections' });
      for (const socket of [...sockets.slice(1), newest]) {
        socket.send({ type: 'nope' });
        assert.equal((await socket.next('error')).error, 'bad_request');
      }
    });

    test('sockets of other players do not count', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      const theirs = await s.connect(b);
      for (let i = 0; i < maxConnectionsPerUser + 1; i += 1) await s.connect(a);
      theirs.send({ type: 'nope' });
      assert.equal((await theirs.next('error')).error, 'bad_request');
    });

    test(`a frame over ${maxFrameBytes} bytes closes the socket with 1009`, async () => {
      const a = await s.newPlayer();
      const socket = await s.connect(a);
      socket.sendRaw('x'.repeat(maxFrameBytes + 1));
      assert.equal((await socket.closed).code, 1009);
    });

    test('a binary frame closes the socket with 1003', async () => {
      const a = await s.newPlayer();
      const socket = await s.connect(a);
      socket.sendRaw(new Uint8Array(4));
      assert.equal((await socket.closed).code, closeCodes.textOnly);
    });
  });

  describe('bad frames', () => {
    const cases: [string, string][] = [
      ['not JSON', '{'],
      ['an array', '[]'],
      ['an unknown type', JSON.stringify({ type: 'friend.request', name: 'x' })],
      ['an extra field', JSON.stringify({ type: 'chat.send', to: makeUuid(1), text: 'hi', extra: 1 })],
      ['a bad uuid', JSON.stringify({ type: 'chat.send', to: 'nope', text: 'hi' })],
      ['text that is not a string', JSON.stringify({ type: 'chat.send', to: makeUuid(1), text: 5 })],
      ['a bad message id', JSON.stringify({ type: 'chat.read', with: makeUuid(1), upTo: '0' })],
      ['a bad ref', JSON.stringify({ type: 'chat.read', ref: 'a b', with: makeUuid(1), upTo: '1' })],
    ];
    for (const [what, raw] of cases) {
      test(`${what} gets a bad_request error and the socket stays open`, async () => {
        const a = await s.newPlayer();
        const socket = await s.connect(a);
        socket.sendRaw(raw);
        assert.equal((await socket.next('error')).error, 'bad_request');
        socket.send({ type: 'nope', ref: 'r2' });
        const again = await socket.next('error');
        assert.equal(again.ref, 'r2');
      });
    }

    test('the error carries the ref of the frame', async () => {
      const a = await s.newPlayer();
      const socket = await s.connect(a);
      socket.send({ type: 'chat.send', ref: 'abc-1', to: makeUuid(999_999), text: 'hi' });
      const error = await socket.next('error');
      assert.deepEqual(error, { type: 'error', ref: 'abc-1', error: 'not_found', message: 'Not friends with this player' });
    });
  });

  describe('events', () => {
    test('a friend request is pushed to the recipient', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      const socket = await s.connect(b);
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      assert.deepEqual(await socket.next('friend.request'), {
        type: 'friend.request',
        user: { uuid: a.uuid, name: a.name },
        createdAt: s.clock.iso(),
      });
    });

    test('sending the same request again pushes nothing', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      const socket = await s.connect(b);
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      await socket.none('friend.request');
    });

    test('accepting is pushed to the one who asked', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      const socket = await s.connect(a);
      await s.makeFriends(a, b);
      assert.deepEqual(await socket.next('friend.accepted'), { type: 'friend.accepted', user: { uuid: b.uuid, name: b.name } });
    });

    test('asking back accepts and is pushed to the first one who asked', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      const socket = await s.connect(a);
      await s.api('POST', '/v1/friends/requests', { token: b.token, body: { name: a.name } });
      assert.deepEqual(await socket.next('friend.accepted'), { type: 'friend.accepted', user: { uuid: b.uuid, name: b.name } });
    });

    test('unfriending is pushed to both', async () => {
      const { a, b } = await friends();
      const [sa, sb] = [await s.connect(a), await s.connect(b)];
      await s.api('DELETE', `/v1/friends/${b.uuid}`, { token: a.token });
      assert.deepEqual(await sa.next('friend.removed'), { type: 'friend.removed', uuid: b.uuid });
      assert.deepEqual(await sb.next('friend.removed'), { type: 'friend.removed', uuid: a.uuid });
    });

    test('a block looks like unfriending to both', async () => {
      const { a, b } = await friends();
      const [sa, sb] = [await s.connect(a), await s.connect(b)];
      await s.api('PUT', `/v1/blocks/${b.uuid}`, { token: a.token });
      assert.deepEqual(await sa.next('friend.removed'), { type: 'friend.removed', uuid: b.uuid });
      assert.deepEqual(await sb.next('friend.removed'), { type: 'friend.removed', uuid: a.uuid });
    });

    test('blocking a stranger pushes nothing', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      const sb = await s.connect(b);
      await s.api('PUT', `/v1/blocks/${b.uuid}`, { token: a.token });
      await sb.none('friend.removed');
    });

    test('an invite is pushed to the friend, and cancelling it too', async () => {
      const { a, b } = await friends();
      const socket = await s.connect(b);
      await s.api('PUT', `/v1/invites/${b.uuid}`, { token: a.token, body: { host: 'mc.hypixel.net' } });
      assert.deepEqual(await socket.next('invite'), {
        type: 'invite',
        invite: {
          uuid: a.uuid,
          name: a.name,
          host: 'mc.hypixel.net',
          port: 25565,
          createdAt: s.clock.iso(),
          expiresAt: s.clock.iso(600),
        },
      });
      await s.api('DELETE', `/v1/invites/${b.uuid}`, { token: a.token });
      assert.deepEqual(await socket.next('invite.cancelled'), { type: 'invite.cancelled', uuid: a.uuid });
    });

    test('a refused invite pushes nothing', async () => {
      const { a, b } = await friends();
      await s.api('PATCH', '/v1/me/settings', { token: b.token, body: { allowInvites: false } });
      const socket = await s.connect(b);
      const res = await s.api('PUT', `/v1/invites/${b.uuid}`, { token: a.token, body: { host: 'mc.hypixel.net' } });
      assert.equal(res.status, 409);
      await socket.none('invite');
    });

    describe('status', () => {
      test('a change is pushed to friends', async () => {
        const { a, b } = await friends();
        const socket = await s.connect(b);
        await s.api('PUT', '/v1/me/status', { token: a.token, body: { state: 'online' } });
        assert.deepEqual(await socket.next('friend.status'), { type: 'friend.status', uuid: a.uuid, status: { state: 'online' } });
        await s.api('PUT', '/v1/me/status', {
          token: a.token,
          body: { state: 'playing', instance: 'Survival', version: '1.21.4', world: { type: 'server', host: 'mc.hypixel.net' } },
        });
        assert.deepEqual(await socket.next('friend.status'), {
          type: 'friend.status',
          uuid: a.uuid,
          status: {
            state: 'playing',
            instance: 'Survival',
            version: '1.21.4',
            world: { type: 'server', host: 'mc.hypixel.net', port: 25565 },
            joinable: true,
          },
        });
      });

      test('the same report again pushes nothing', async () => {
        const { a, b } = await friends();
        await s.api('PUT', '/v1/me/status', { token: a.token, body: { state: 'online' } });
        const socket = await s.connect(b);
        s.clock.advance(60);
        await s.api('PUT', '/v1/me/status', { token: a.token, body: { state: 'online' } });
        await socket.none('friend.status');
      });

      test('going offline is pushed with the last-seen time', async () => {
        const { a, b } = await friends();
        await s.api('PUT', '/v1/me/status', { token: a.token, body: { state: 'online' } });
        const socket = await s.connect(b);
        await s.api('DELETE', '/v1/me/status', { token: a.token });
        assert.deepEqual(await socket.next('friend.status'), {
          type: 'friend.status',
          uuid: a.uuid,
          status: { state: 'offline', lastSeenAt: s.clock.iso() },
        });
      });

      test('a status running out is pushed by the purge', async () => {
        const { a, b } = await friends();
        await s.api('PUT', '/v1/me/status', { token: a.token, body: { state: 'online' } });
        const seen = s.clock.iso();
        const socket = await s.connect(b);
        s.clock.advance(181);
        await s.purge();
        assert.deepEqual(await socket.next('friend.status'), {
          type: 'friend.status',
          uuid: a.uuid,
          status: { state: 'offline', lastSeenAt: seen },
        });
      });

      test('settings that change what friends see are pushed', async () => {
        const { a, b } = await friends();
        await s.api('PUT', '/v1/me/status', {
          token: a.token,
          body: { state: 'playing', instance: 'Survival', version: '1.21.4', world: { type: 'server', host: 'mc.hypixel.net' } },
        });
        const socket = await s.connect(b);
        await s.api('PATCH', '/v1/me/settings', { token: a.token, body: { hideServer: true } });
        const hidden = await socket.next('friend.status');
        assert.deepEqual(hidden.status, { state: 'playing', instance: 'Survival', version: '1.21.4', world: null, joinable: false });
        await s.api('PATCH', '/v1/me/settings', { token: a.token, body: { visibility: 'invisible' } });
        const invisible = await socket.next('friend.status');
        assert.deepEqual(invisible.status, { state: 'offline', lastSeenAt: null });
      });

      test('is not pushed to players who are not friends', async () => {
        const a = await s.newPlayer();
        const stranger = await s.newPlayer();
        await s.api('PATCH', '/v1/me/settings', { token: a.token, body: { visibility: 'everyone' } });
        const socket = await s.connect(stranger);
        await s.api('PUT', '/v1/me/status', { token: a.token, body: { state: 'online' } });
        await socket.none('friend.status');
      });
    });

    test('deleting an account tells friends', async () => {
      const { a, b } = await friends();
      const socket = await s.connect(b);
      await s.api('DELETE', '/v1/me', { token: a.token });
      assert.deepEqual(await socket.next('friend.removed'), { type: 'friend.removed', uuid: a.uuid });
    });
  });
});

describe('socket rate limits', () => {
  test('frames past the limit get one rate_limited error, then are dropped until the window ends', async () => {
    const s = await startServer({ rateLimits: { socketFramesPerMinute: 3 } });
    try {
      const a = await s.newPlayer();
      const socket = await s.connect(a);
      for (let i = 0; i < 3; i += 1) socket.send({ type: 'nope', ref: `r${i}` });
      for (let i = 0; i < 3; i += 1) assert.equal((await socket.next('error')).ref, `r${i}`);
      socket.send({ type: 'nope', ref: 'r3' });
      socket.send({ type: 'nope', ref: 'r4' });
      const limited = await socket.next('error');
      assert.equal(limited.error, 'rate_limited');
      assert.equal(limited.retryAfter, 60);
      await socket.none('error');
      s.clock.advance(60);
      socket.send({ type: 'nope', ref: 'r5' });
      const after = await socket.next('error');
      assert.deepEqual([after.error, after.ref], ['bad_request', 'r5']);
    } finally {
      await s.close();
    }
  });

  test('the limit is per socket', async () => {
    const s = await startServer({ rateLimits: { socketFramesPerMinute: 1 } });
    try {
      const a = await s.newPlayer();
      const [first, second] = [await s.connect(a), await s.connect(a)];
      first.send({ type: 'nope' });
      second.send({ type: 'nope' });
      assert.equal((await first.next('error')).error, 'bad_request');
      assert.equal((await second.next('error')).error, 'bad_request');
    } finally {
      await s.close();
    }
  });

  test('upgrades count against the per-user limit', async () => {
    const s = await startServer({ rateLimits: { userPerMinute: 2 } });
    try {
      const a = await s.newPlayer();
      const headers = { authorization: `Bearer ${a.token}` };
      assert.equal(await upgradeStatus(s.baseUrl, headers), 101);
      assert.equal(await upgradeStatus(s.baseUrl, headers), 101);
      assert.equal(await upgradeStatus(s.baseUrl, headers), 429);
    } finally {
      await s.close();
    }
  });

  test('upgrades count against the per-IP limit', async () => {
    const s = await startServer({ rateLimits: { ipPerMinute: 1 } });
    try {
      assert.equal(await upgradeStatus(s.baseUrl, {}), 401);
      assert.equal(await upgradeStatus(s.baseUrl, {}), 429);
    } finally {
      await s.close();
    }
  });

  test('the server closes the connection of an upgrade it refused', async () => {
    const s = await startServer({ rateLimits: { ipPerMinute: 1 } });
    try {
      assert.equal(await refusedUpgradeCloses(s.baseUrl, {}), 401);
      assert.equal(await refusedUpgradeCloses(s.baseUrl, {}), 429);
    } finally {
      await s.close();
    }
  });
});

describe('heartbeat', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer({ heartbeatMs: 50 });
  });

  after(async () => {
    await s.close();
  });

  test('a socket that does not answer pings is dropped', async () => {
    const a = await s.newPlayer();
    const silent = await silentSocket(s.baseUrl, a.token);
    await silent.closed;
  });

  test('a socket that answers pings stays open', async () => {
    const a = await s.newPlayer();
    const socket = await s.connect(a);
    await new Promise((resolve) => setTimeout(resolve, 300));
    socket.send({ type: 'nope' });
    assert.equal((await socket.next('error')).error, 'bad_request');
  });

  test('a socket whose session ran out is closed', async () => {
    const a = await s.newPlayer();
    const socket = await s.connect(a);
    s.clock.advance(30 * DAY);
    assert.deepEqual(await socket.closed, { code: closeCodes.sessionEnded, reason: 'Session ended' });
  });
});
