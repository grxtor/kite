import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { reportRetentionDays } from '../src/chat.ts';
import { closeCodes } from '../src/routes/socket.ts';
import { makeUuid, startServer } from './helpers.ts';
import type { ErrorBody, MeBody, Player, TestServer } from './helpers.ts';
import { TestSocket } from './socketClient.ts';

const HOUR = 60 * 60;
const DAY = 24 * HOUR;

interface MessageBody {
  id: string;
}

type SanctionError = ErrorBody & { until?: string; reason?: string | null };

function api(s: TestServer) {
  return {
    send: (from: Player, to: Player, text = 'hi') =>
      s.api<MessageBody & SanctionError>('POST', `/v1/chats/${to.uuid}/messages`, { token: from.token, body: { text } }),
    history: (reader: Player, other: Player) => s.api('GET', `/v1/chats/${other.uuid}/messages`, { token: reader.token }),
    invite: (from: Player, to: Player) =>
      s.api<SanctionError>('PUT', `/v1/invites/${to.uuid}`, { token: from.token, body: { host: 'play.example.com' } }),
    request: (from: Player, to: Player) =>
      s.api<SanctionError>('POST', '/v1/friends/requests', { token: from.token, body: { name: to.name } }),
    report: (reporter: Player, other: Player, id: string, reason = 'harassment') =>
      s.api('POST', `/v1/chats/${other.uuid}/messages/${id}/report`, { token: reporter.token, body: { reason } }),
    me: (player: Player) => s.api<MeBody>('GET', '/v1/me', { token: player.token }),
  };
}

async function sanction(s: TestServer, uuid: string, columns: { mutedUntil?: string; bannedAt?: string; reason?: string }) {
  await s.query(
    `insert into sanctions (uuid, muted_until, banned_at, ban_reason) values ($1, $2, $3, $4)
     on conflict (uuid) do update set muted_until = excluded.muted_until, banned_at = excluded.banned_at,
       ban_reason = excluded.ban_reason`,
    [uuid, columns.mutedUntil ?? null, columns.bannedAt ?? null, columns.reason ?? null],
  );
}

const lift = (s: TestServer, uuid: string) => s.query('delete from sanctions where uuid = $1', [uuid]);

describe('moderation', () => {
  let s: TestServer;
  let call: ReturnType<typeof api>;

  before(async () => {
    // A short heartbeat, so a ban made in another process closes sockets quickly.
    s = await startServer({ heartbeatMs: 50 });
    call = api(s);
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

  describe('a muted player', () => {
    test('cannot send over HTTP: 403 muted with until', async () => {
      const { a, b } = await friends();
      await sanction(s, a.uuid, { mutedUntil: s.clock.iso(HOUR) });
      const res = await call.send(a, b);
      assert.equal(res.status, 403);
      assert.deepEqual(res.body, {
        error: 'muted',
        message: `You can send messages again at ${s.clock.iso(HOUR)}`,
        until: s.clock.iso(HOUR),
      });
      assert.equal((await s.query('select 1 from messages where sender_uuid = $1', [a.uuid])).length, 0);
    });

    test('cannot send over the socket: a muted error with until, and the socket stays open', async () => {
      const { a, b } = await friends();
      await sanction(s, a.uuid, { mutedUntil: s.clock.iso(HOUR) });
      const [socket, other] = [await s.connect(a), await s.connect(b)];
      socket.send({ type: 'chat.send', ref: 'm1', to: b.uuid, text: 'hi' });
      const error = await socket.next('error');
      assert.deepEqual(error, {
        type: 'error',
        ref: 'm1',
        error: 'muted',
        message: `You can send messages again at ${s.clock.iso(HOUR)}`,
        until: s.clock.iso(HOUR),
      });
      await other.none('chat.message');
      socket.send({ type: 'chat.send', ref: 'm2', to: b.uuid, text: 'again' });
      assert.equal((await socket.next('error')).error, 'muted');
    });

    test('still reads, receives messages, invites and sends friend requests', async () => {
      const { a, b } = await friends();
      const sent = await call.send(b, a, 'from b');
      await sanction(s, a.uuid, { mutedUntil: s.clock.iso(HOUR) });
      assert.equal((await call.history(a, b)).status, 200);
      assert.equal((await call.send(b, a, 'still reaches a')).status, 201);
      const read = await s.api('POST', `/v1/chats/${b.uuid}/read`, { token: a.token, body: { upTo: sent.body.id } });
      assert.equal(read.status, 200);
      assert.equal((await call.invite(a, b)).status, 200);
      assert.equal((await call.request(a, await s.newPlayer())).status, 201);
    });

    test('sees the mute in GET /v1/me', async () => {
      const a = await s.newPlayer();
      await sanction(s, a.uuid, { mutedUntil: s.clock.iso(HOUR) });
      assert.deepEqual((await call.me(a)).body.moderation, { mutedUntil: s.clock.iso(HOUR), banned: null });
    });

    test('can send again once the mute runs out, and the purge drops it', async () => {
      const { a, b } = await friends();
      await sanction(s, a.uuid, { mutedUntil: s.clock.iso(60) });
      assert.equal((await call.send(a, b)).status, 403);
      s.clock.advance(60);
      assert.equal((await call.send(a, b)).status, 201);
      assert.equal((await call.me(a)).body.moderation.mutedUntil, null);
      await s.purge();
      assert.equal((await s.query('select 1 from sanctions where uuid = $1', [a.uuid])).length, 0);
    });
  });

  describe('a banned player', () => {
    const reason = 'harassing younger players';

    test('cannot chat, invite or send friend requests: 403 banned with the reason', async () => {
      const { a, b } = await friends();
      await sanction(s, a.uuid, { bannedAt: s.clock.iso(), reason });
      const stranger = await s.newPlayer();
      for (const res of [await call.send(a, b), await call.invite(a, b), await call.request(a, stranger)]) {
        assert.equal(res.status, 403);
        assert.deepEqual(res.body, {
          error: 'banned',
          message: 'This account may not chat, invite or send friend requests',
          reason,
        });
      }
      assert.equal((await s.query('select 1 from invites where from_uuid = $1', [a.uuid])).length, 0);
      assert.equal((await s.query('select 1 from friend_requests where from_uuid = $1', [a.uuid])).length, 0);
    });

    test('has their open sockets closed with 4403 by the next heartbeat', async () => {
      const a = await s.newPlayer();
      const sockets = [await s.connect(a), await s.connect(a)];
      await sanction(s, a.uuid, { bannedAt: s.clock.iso(), reason });
      for (const socket of sockets) assert.deepEqual(await socket.closed, { code: closeCodes.banned, reason: 'Banned' });
    });

    test('gets a socket that closes with 4403 at once, without ready', async () => {
      const a = await s.newPlayer();
      await sanction(s, a.uuid, { bannedAt: s.clock.iso(), reason });
      const socket = new TestSocket(`${s.baseUrl.replace('http', 'ws')}/v1/ws`, a.token);
      assert.deepEqual(await socket.closed, { code: closeCodes.banned, reason: 'Banned' });
      await socket.none('ready', 0);
    });

    test('still signs in, sees why, reads and deletes their account', async () => {
      const { a, b } = await friends();
      await call.send(b, a, 'hello');
      await sanction(s, a.uuid, { bannedAt: s.clock.iso(), reason });
      const token = await s.signIn(a.name, a.uuid);
      const again = { ...a, token };
      assert.deepEqual((await call.me(again)).body.moderation, { mutedUntil: null, banned: { at: s.clock.iso(), reason } });
      assert.equal((await call.history(again, b)).status, 200);
      assert.equal((await s.api('GET', '/v1/friends', { token })).status, 200);
      assert.equal((await s.api('DELETE', '/v1/me', { token })).status, 204);
    });

    test('stays banned after deleting the account and signing in again', async () => {
      const a = await s.newPlayer();
      await sanction(s, a.uuid, { bannedAt: s.clock.iso(), reason });
      assert.equal((await s.api('DELETE', '/v1/me', { token: a.token })).status, 204);
      const again = { ...a, token: await s.signIn(a.name, a.uuid) };
      assert.equal((await call.request(again, await s.newPlayer())).body.error, 'banned');
      await s.purge();
      assert.equal((await s.query('select 1 from sanctions where uuid = $1', [a.uuid])).length, 1);
    });

    test('can do everything again once unbanned', async () => {
      const { a, b } = await friends();
      await sanction(s, a.uuid, { bannedAt: s.clock.iso(), reason });
      assert.equal((await call.send(a, b)).status, 403);
      await lift(s, a.uuid);
      assert.equal((await call.send(a, b)).status, 201);
      assert.equal((await call.invite(a, b)).status, 200);
      await s.connect(a);
    });

    test('others can still send them friend requests and messages', async () => {
      const { a, b } = await friends();
      await sanction(s, a.uuid, { bannedAt: s.clock.iso(), reason });
      assert.equal((await call.send(b, a)).status, 201);
      assert.equal((await call.request(await s.newPlayer(), a)).status, 201);
    });
  });
});

describe('a ban before the heartbeat finds it', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  test('a chat.send on the open socket gets a banned error with the reason', async () => {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    await s.makeFriends(a, b);
    const socket = await s.connect(a);
    await sanction(s, a.uuid, { bannedAt: s.clock.iso(), reason: 'spam' });
    socket.send({ type: 'chat.send', ref: 'm1', to: b.uuid, text: 'hi' });
    assert.deepEqual(await socket.next('error'), {
      type: 'error',
      ref: 'm1',
      error: 'banned',
      message: 'This account may not chat, invite or send friend requests',
      reason: 'spam',
    });
  });
});

describe('the purge', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  test(`drops log entries after ${reportRetentionDays} days, except those about a player still banned`, async () => {
    const banned = makeUuid(1);
    const unbanned = makeUuid(2);
    await sanction(s, banned, { bannedAt: s.clock.iso() });
    await s.query(
      `insert into moderation_log (actor, at, action, target, note) values
         ('cli', $1, 'ban', $2, null), ('cli', $1, 'unban', $3, null), ('cli', $1, 'resolve', '7', 'done')`,
      [s.clock.iso(), banned, unbanned],
    );
    s.clock.advance((reportRetentionDays - 1) * DAY);
    await s.purge();
    assert.equal((await s.query('select 1 from moderation_log')).length, 3);
    s.clock.advance(DAY);
    await s.purge();
    assert.deepEqual(await s.query('select action, target from moderation_log'), [{ action: 'ban', target: banned }]);
  });
});
