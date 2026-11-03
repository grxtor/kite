import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { maxMessageLength, messageRetentionDays, reportRetentionDays } from '../src/chat.ts';
import { makeUuid, startServer } from './helpers.ts';
import type { ErrorBody, Player, TestServer } from './helpers.ts';

const DAY = 24 * 60 * 60;

interface MessageBody {
  id: string;
  from: string;
  to: string;
  text: string | null;
  createdAt: string;
  deleted: boolean;
}

interface HistoryBody {
  messages: MessageBody[];
  more: boolean;
}

interface ReportRow {
  reporter_uuid: string;
  reported_uuid: string;
  message_id: string;
  reason: string;
  snapshot: {
    reported: { uuid: string; name: string | null };
    messages: { id: string; from: string; text: string | null; createdAt: string }[];
  };
}

function chatApi(s: TestServer) {
  return {
    send: (from: Player, to: Player | string, text: unknown) =>
      s.api<MessageBody & ErrorBody>('POST', `/v1/chats/${typeof to === 'string' ? to : to.uuid}/messages`, {
        token: from.token,
        body: { text },
      }),
    history: (reader: Player, other: Player | string, query = '') =>
      s.api<HistoryBody & ErrorBody>('GET', `/v1/chats/${typeof other === 'string' ? other : other.uuid}/messages${query}`, {
        token: reader.token,
      }),
    read: (reader: Player, other: Player, upTo: string) =>
      s.api<{ upTo: string } & ErrorBody>('POST', `/v1/chats/${other.uuid}/read`, { token: reader.token, body: { upTo } }),
    report: (reporter: Player, other: Player, id: string, reason: unknown = 'harassment') =>
      s.api<ErrorBody>('POST', `/v1/chats/${other.uuid}/messages/${id}/report`, { token: reporter.token, body: { reason } }),
  };
}

async function sent(s: TestServer, from: Player, to: Player, text: string): Promise<MessageBody> {
  const res = await chatApi(s).send(from, to, text);
  assert.equal(res.status, 201, `sending ${text}`);
  return res.body;
}

async function unreadFrom(s: TestServer, reader: Player, other: Player): Promise<number | undefined> {
  return (await s.friendsOf(reader)).friends.find((friend) => friend.uuid === other.uuid)?.unread;
}

describe('chat', () => {
  let s: TestServer;
  let chat: ReturnType<typeof chatApi>;

  before(async () => {
    // Retention tests move the clock by months; the players' sessions must outlast that.
    s = await startServer({ sessionTtlDays: 1000 });
    chat = chatApi(s);
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

  describe('sending', () => {
    test('over HTTP is 201 and reaches both players\' sockets', async () => {
      const { a, b } = await friends();
      const [sa, sb] = [await s.connect(a), await s.connect(b)];
      const res = await chat.send(a, b, 'hi');
      assert.equal(res.status, 201);
      assert.match(res.body.id, /^[1-9][0-9]*$/);
      assert.deepEqual(res.body, { id: res.body.id, from: a.uuid, to: b.uuid, text: 'hi', createdAt: s.clock.iso(), deleted: false });
      assert.deepEqual(await sb.next('chat.message'), { type: 'chat.message', message: res.body });
      assert.deepEqual(await sa.next('chat.message'), { type: 'chat.message', message: res.body });
    });

    test('over the socket answers chat.sent on that socket and pushes chat.message everywhere else', async () => {
      const { a, b } = await friends();
      const [origin, other, sb] = [await s.connect(a), await s.connect(a), await s.connect(b)];
      origin.send({ type: 'chat.send', ref: 'm1', to: b.uuid, text: 'hello' });
      const answer = await origin.next('chat.sent');
      assert.equal(answer.ref, 'm1');
      const message = answer.message as MessageBody;
      assert.deepEqual(message, { id: message.id, from: a.uuid, to: b.uuid, text: 'hello', createdAt: s.clock.iso(), deleted: false });
      assert.deepEqual(await sb.next('chat.message'), { type: 'chat.message', message });
      assert.deepEqual(await other.next('chat.message'), { type: 'chat.message', message });
      await origin.none('chat.message');
    });

    test('a socket keeps the order its messages were sent in', async () => {
      const { a, b } = await friends();
      const socket = await s.connect(a);
      for (let i = 0; i < 10; i += 1) socket.send({ type: 'chat.send', ref: `m${i}`, to: b.uuid, text: `n${i}` });
      for (let i = 0; i < 10; i += 1) assert.equal((await socket.next('chat.sent')).ref, `m${i}`);
      const history = await chat.history(b, a);
      assert.deepEqual(
        history.body.messages.map((m) => m.text),
        Array.from({ length: 10 }, (_, i) => `n${i}`),
      );
    });

    test('control and bidi characters are removed and the text trimmed', async () => {
      const { a, b } = await friends();
      const res = await chat.send(a, b, '  hi‮ the\u0007re\n⁦ ');
      assert.equal(res.status, 201);
      assert.equal(res.body.text, 'hi there');
    });

    test(`${maxMessageLength} characters are allowed`, async () => {
      const { a, b } = await friends();
      assert.equal((await chat.send(a, b, 'x'.repeat(maxMessageLength))).status, 201);
    });

    test(`${maxMessageLength + 1} characters are 400`, async () => {
      const { a, b } = await friends();
      const res = await chat.send(a, b, 'x'.repeat(maxMessageLength + 1));
      assert.equal(res.status, 400);
      assert.equal(res.body.error, 'bad_request');
    });

    test('characters outside the BMP count as two, the way Java and Qt count them', async () => {
      const { a, b } = await friends();
      assert.equal((await chat.send(a, b, '😀'.repeat(maxMessageLength / 2))).status, 201);
      assert.equal((await chat.send(a, b, `${'😀'.repeat(maxMessageLength / 2)}x`)).status, 400);
    });

    for (const [what, text] of [
      ['an empty text', ''],
      ['only spaces and control characters', ' \u0000‮\t '],
    ] as const) {
      test(`${what} is 400`, async () => {
        const { a, b } = await friends();
        assert.equal((await chat.send(a, b, text)).status, 400);
      });
    }

    test('a text that is not a string is 400', async () => {
      const { a, b } = await friends();
      assert.equal((await chat.send(a, b, { text: 'hi' })).status, 400);
    });

    test('to yourself is 400', async () => {
      const a = await s.newPlayer();
      assert.equal((await chat.send(a, a, 'hi')).status, 400);
    });

    test('an upper-case uuid reaches the same friend', async () => {
      const { a, b } = await friends();
      const res = await chat.send(a, b.uuid.toUpperCase(), 'hi');
      assert.equal(res.status, 201);
      assert.equal(res.body.to, b.uuid);
    });

    describe('only between friends', () => {
      test('an unknown player is 404', async () => {
        const a = await s.newPlayer();
        const res = await chat.send(a, makeUuid(999_999), 'hi');
        assert.equal(res.status, 404);
        assert.equal(res.body.error, 'not_found');
      });

      test('a player who is not a friend is 404 and gets nothing', async () => {
        const a = await s.newPlayer();
        const stranger = await s.newPlayer();
        const socket = await s.connect(stranger);
        assert.equal((await chat.send(a, stranger, 'hi')).status, 404);
        await socket.none('chat.message');
        assert.deepEqual((await chat.history(stranger, a)).body.messages, []);
      });

      test('a pending request is not enough', async () => {
        const a = await s.newPlayer();
        const b = await s.newPlayer();
        await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
        assert.equal((await chat.send(a, b, 'hi')).status, 404);
        assert.equal((await chat.send(b, a, 'hi')).status, 404);
      });

      test('over the socket, a stranger gets a not_found error', async () => {
        const a = await s.newPlayer();
        const stranger = await s.newPlayer();
        const socket = await s.connect(a);
        socket.send({ type: 'chat.send', ref: 'x', to: stranger.uuid, text: 'hi' });
        const error = await socket.next('error');
        assert.deepEqual([error.ref, error.error], ['x', 'not_found']);
      });

      test('after unfriending, sending is refused both ways', async () => {
        const { a, b } = await friends();
        await sent(s, a, b, 'hi');
        await s.api('DELETE', `/v1/friends/${b.uuid}`, { token: a.token });
        assert.equal((await chat.send(a, b, 'hi')).status, 404);
        assert.equal((await chat.send(b, a, 'hi')).status, 404);
      });
    });
  });

  describe('blocks', () => {
    for (const blocker of ['sender', 'recipient'] as const) {
      test(`a block by the ${blocker} stops messages both ways and hides history from both`, async () => {
        const { a, b } = await friends();
        await sent(s, a, b, 'one');
        await sent(s, b, a, 'two');
        const [by, on] = blocker === 'sender' ? [a, b] : [b, a];
        await s.api('PUT', `/v1/blocks/${on.uuid}`, { token: by.token });
        const socket = await s.connect(b);
        assert.equal((await chat.send(a, b, 'three')).status, 404);
        assert.equal((await chat.send(b, a, 'three')).status, 404);
        await socket.none('chat.message');
        assert.deepEqual((await chat.history(a, b)).body, { messages: [], more: false });
        assert.deepEqual((await chat.history(b, a)).body, { messages: [], more: false });
      });
    }

    test('a block does not hide the history once lifted', async () => {
      const { a, b } = await friends();
      await sent(s, a, b, 'one');
      await s.api('PUT', `/v1/blocks/${b.uuid}`, { token: a.token });
      await s.api('DELETE', `/v1/blocks/${b.uuid}`, { token: a.token });
      assert.deepEqual(
        (await chat.history(b, a)).body.messages.map((m) => m.text),
        ['one'],
      );
    });

    test('marking read is refused while blocked', async () => {
      const { a, b } = await friends();
      const message = await sent(s, a, b, 'one');
      await s.api('PUT', `/v1/blocks/${a.uuid}`, { token: b.token });
      assert.equal((await chat.read(b, a, message.id)).status, 404);
    });
  });

  describe('history', () => {
    test('is oldest first, newest page by default', async () => {
      const { a, b } = await friends();
      const sentIds: string[] = [];
      for (let i = 0; i < 5; i += 1) {
        sentIds.push((await sent(s, i % 2 ? b : a, i % 2 ? a : b, `m${i}`)).id);
        s.clock.advance(1);
      }
      const res = await chat.history(a, b);
      assert.equal(res.status, 200);
      assert.deepEqual(res.body.messages.map((m) => m.id), sentIds);
      assert.equal(res.body.more, false);
      assert.deepEqual((await chat.history(b, a)).body, res.body);
    });

    test('pages with limit and before, and after for catching up', async () => {
      const { a, b } = await friends();
      const ids: string[] = [];
      for (let i = 0; i < 5; i += 1) ids.push((await sent(s, a, b, `m${i}`)).id);

      const latest = await chat.history(b, a, '?limit=2');
      assert.deepEqual(latest.body.messages.map((m) => m.id), ids.slice(3));
      assert.equal(latest.body.more, true);

      const older = await chat.history(b, a, `?limit=2&before=${ids[3]}`);
      assert.deepEqual(older.body.messages.map((m) => m.id), ids.slice(1, 3));
      assert.equal(older.body.more, true);

      const oldest = await chat.history(b, a, `?limit=2&before=${ids[1]}`);
      assert.deepEqual(oldest.body.messages.map((m) => m.id), ids.slice(0, 1));
      assert.equal(oldest.body.more, false);

      const newer = await chat.history(b, a, `?limit=2&after=${ids[0]}`);
      assert.deepEqual(newer.body.messages.map((m) => m.id), ids.slice(1, 3));
      assert.equal(newer.body.more, true);

      const rest = await chat.history(b, a, `?after=${ids[2]}`);
      assert.deepEqual(rest.body.messages.map((m) => m.id), ids.slice(3));
      assert.equal(rest.body.more, false);
    });

    test('a socket that reconnects catches up with after', async () => {
      const { a, b } = await friends();
      const first = await s.connect(b);
      const seen = await sent(s, a, b, 'before');
      await first.next('chat.message');
      first.close();
      await first.closed;
      const missed = [await sent(s, a, b, 'while away 1'), await sent(s, a, b, 'while away 2')];
      await s.connect(b);
      const res = await chat.history(b, a, `?after=${seen.id}`);
      assert.deepEqual(res.body, { messages: missed, more: false });
    });

    for (const [what, query] of [
      ['before and after together', '?before=5&after=1'],
      ['limit 0', '?limit=0'],
      ['limit 101', '?limit=101'],
      ['a message id of 0', '?before=0'],
      ['a message id that is not a number', '?after=abc'],
      ['a message id with 19 digits', `?before=${'9'.repeat(19)}`],
    ] as const) {
      test(`${what} is 400`, async () => {
        const { a, b } = await friends();
        assert.equal((await chat.history(a, b, query)).status, 400);
      });
    }

    test('a bad uuid is 400', async () => {
      const a = await s.newPlayer();
      assert.equal((await chat.history(a, 'nope')).status, 400);
    });

    test('a third player only ever sees their own conversation', async () => {
      const { a, b } = await friends();
      await sent(s, a, b, 'private');
      const c = await s.newPlayer();
      await s.makeFriends(c, a);
      await s.makeFriends(c, b);
      assert.deepEqual((await chat.history(c, a)).body.messages, []);
      assert.deepEqual((await chat.history(c, b)).body.messages, []);
    });

    test('stays readable after unfriending', async () => {
      const { a, b } = await friends();
      await sent(s, a, b, 'hi');
      await s.api('DELETE', `/v1/friends/${a.uuid}`, { token: b.token });
      assert.deepEqual((await chat.history(b, a)).body.messages.map((m) => m.text), ['hi']);
    });

    test(`messages older than ${messageRetentionDays} days are not shown even before the purge`, async () => {
      const { a, b } = await friends();
      await sent(s, a, b, 'old');
      s.clock.advance(messageRetentionDays * DAY - 1);
      const fresh = await sent(s, a, b, 'new');
      assert.equal((await chat.history(b, a)).body.messages.length, 2);
      s.clock.advance(1);
      assert.deepEqual((await chat.history(b, a)).body.messages, [fresh]);
    });
  });

  describe('read markers', () => {
    test('GET /v1/friends counts unread messages per friend', async () => {
      const { a, b } = await friends();
      await sent(s, a, b, 'one');
      await sent(s, a, b, 'two');
      await sent(s, b, a, 'back');
      assert.equal(await unreadFrom(s, b, a), 2);
      assert.equal(await unreadFrom(s, a, b), 1);
    });

    test('marking read lowers the count and never moves back', async () => {
      const { a, b } = await friends();
      const one = await sent(s, a, b, 'one');
      const two = await sent(s, a, b, 'two');
      const res = await chat.read(b, a, one.id);
      assert.deepEqual([res.status, res.body], [200, { upTo: one.id }]);
      assert.equal(await unreadFrom(s, b, a), 1);
      assert.deepEqual((await chat.read(b, a, two.id)).body, { upTo: two.id });
      assert.equal(await unreadFrom(s, b, a), 0);
      assert.deepEqual((await chat.read(b, a, one.id)).body, { upTo: two.id });
      assert.equal(await unreadFrom(s, b, a), 0);
    });

    test('your own message may be the marker', async () => {
      const { a, b } = await friends();
      await sent(s, a, b, 'one');
      const reply = await sent(s, b, a, 'reply');
      assert.equal((await chat.read(b, a, reply.id)).status, 200);
      assert.equal(await unreadFrom(s, b, a), 0);
    });

    test('a message of another conversation is 404', async () => {
      const { a, b } = await friends();
      const c = await s.newPlayer();
      await s.makeFriends(a, c);
      const elsewhere = await sent(s, a, c, 'to c');
      const res = await chat.read(b, a, elsewhere.id);
      assert.equal(res.status, 404);
      assert.equal(res.body.error, 'not_found');
    });

    test('an unknown message is 404', async () => {
      const { a, b } = await friends();
      assert.equal((await chat.read(b, a, '999999999999')).status, 404);
    });

    test('is pushed to your other sockets', async () => {
      const { a, b } = await friends();
      const message = await sent(s, a, b, 'one');
      const [origin, other] = [await s.connect(b), await s.connect(b)];
      origin.send({ type: 'chat.read', ref: 'r', with: a.uuid.toUpperCase(), upTo: message.id });
      assert.deepEqual(await origin.next('chat.read'), { type: 'chat.read', ref: 'r', with: a.uuid, upTo: message.id });
      assert.deepEqual(await other.next('chat.read'), { type: 'chat.read', with: a.uuid, upTo: message.id });
      await origin.none('chat.read');
    });

    test('the other player is not told', async () => {
      const { a, b } = await friends();
      const message = await sent(s, a, b, 'one');
      const socket = await s.connect(a);
      await chat.read(b, a, message.id);
      await socket.none('chat.read');
    });
  });

  describe('reports', () => {
    test('keeps a copy of the message and what came before it', async () => {
      const { a, b } = await friends();
      const first = await sent(s, b, a, 'hey');
      const second = await sent(s, a, b, 'something nasty');
      const res = await chat.report(b, a, second.id);
      assert.equal(res.status, 204);
      const rows = await s.query<ReportRow>('select * from chat_reports where reporter_uuid = $1', [b.uuid]);
      assert.equal(rows.length, 1);
      assert.deepEqual(
        { ...rows[0], id: undefined, created_at: undefined },
        {
          id: undefined,
          created_at: undefined,
          reporter_uuid: b.uuid,
          reported_uuid: a.uuid,
          message_id: second.id,
          reason: 'harassment',
          snapshot: {
            reported: { uuid: a.uuid, name: a.name },
            messages: [first, second].map(({ id, from, text, createdAt }) => ({ id, from, text, createdAt })),
          },
        },
      );
    });

    test('keeps at most 20 earlier messages', async () => {
      const { a, b } = await friends();
      let last: MessageBody | undefined;
      for (let i = 0; i < 25; i += 1) last = await sent(s, a, b, `m${i}`);
      await chat.report(b, a, last!.id, 'spam');
      const [row] = await s.query<ReportRow>('select snapshot from chat_reports where reporter_uuid = $1', [b.uuid]);
      assert.equal(row!.snapshot.messages.length, 21);
      assert.equal(row!.snapshot.messages[0]!.text, 'm4');
    });

    test('reporting the same message twice keeps one report', async () => {
      const { a, b } = await friends();
      const message = await sent(s, a, b, 'spam');
      assert.equal((await chat.report(b, a, message.id, 'spam')).status, 204);
      assert.equal((await chat.report(b, a, message.id, 'other')).status, 204);
      assert.equal((await s.query('select 1 from chat_reports where reporter_uuid = $1', [b.uuid])).length, 1);
    });

    test('works after blocking', async () => {
      const { a, b } = await friends();
      const message = await sent(s, a, b, 'nasty');
      await s.api('PUT', `/v1/blocks/${a.uuid}`, { token: b.token });
      assert.equal((await chat.report(b, a, message.id)).status, 204);
    });

    test('your own message is 404', async () => {
      const { a, b } = await friends();
      const mine = await sent(s, b, a, 'mine');
      assert.equal((await chat.report(b, a, mine.id)).status, 404);
    });

    test('a message between other players is 404', async () => {
      const { a, b } = await friends();
      const c = await s.newPlayer();
      await s.makeFriends(a, c);
      const elsewhere = await sent(s, a, c, 'to c');
      assert.equal((await chat.report(b, a, elsewhere.id)).status, 404);
      assert.equal((await s.query('select 1 from chat_reports where reporter_uuid = $1', [b.uuid])).length, 0);
    });

    test('an unknown reason is 400', async () => {
      const { a, b } = await friends();
      const message = await sent(s, a, b, 'hi');
      assert.equal((await chat.report(b, a, message.id, 'boring')).status, 400);
    });
  });

  describe('retention', () => {
    test(`the purge removes messages after ${messageRetentionDays} days, and the read markers with them`, async () => {
      const { a, b } = await friends();
      const old = await sent(s, a, b, 'old');
      await chat.read(b, a, old.id);
      s.clock.advance(DAY);
      const kept = await sent(s, a, b, 'kept');
      s.clock.advance((messageRetentionDays - 1) * DAY);
      await s.purge();
      const rows = await s.query<{ id: string }>(
        'select id::text from messages where sender_uuid = $1 or recipient_uuid = $1 order by id',
        [a.uuid],
      );
      assert.deepEqual(rows.map((row) => row.id), [kept.id]);
      assert.equal((await s.query('select 1 from chat_reads where reader_uuid = $1', [b.uuid])).length, 0);
      assert.equal(await unreadFrom(s, b, a), 1);

      s.clock.advance(DAY);
      await s.purge();
      assert.equal((await s.query('select 1 from messages where sender_uuid = $1', [a.uuid])).length, 0);
      assert.equal(await unreadFrom(s, b, a), 0);
    });

    test(`a report outlives the message and the reported account, until ${reportRetentionDays} days`, async () => {
      const { a, b } = await friends();
      const message = await sent(s, a, b, 'nasty');
      await chat.report(b, a, message.id);
      await s.api('DELETE', '/v1/me', { token: a.token });
      s.clock.advance(messageRetentionDays * DAY);
      await s.purge();
      const kept = await s.query<ReportRow>('select snapshot from chat_reports where reporter_uuid = $1', [b.uuid]);
      assert.equal(kept[0]?.snapshot.messages[0]?.text, 'nasty');
      s.clock.advance((reportRetentionDays - messageRetentionDays) * DAY);
      await s.purge();
      assert.equal((await s.query('select 1 from chat_reports where reporter_uuid = $1', [b.uuid])).length, 0);
    });
  });

  describe('deleting an account', () => {
    test('empties what they sent, which the other side sees as deleted', async () => {
      const { a, b } = await friends();
      const fromA = await sent(s, a, b, 'from a');
      const fromB = await sent(s, b, a, 'from b');
      const socket = await s.connect(b);
      assert.equal((await s.api('DELETE', '/v1/me', { token: a.token })).status, 204);
      assert.deepEqual(await socket.next('chat.deleted'), { type: 'chat.deleted', uuid: a.uuid });
      assert.deepEqual((await chat.history(b, a)).body.messages, [{ ...fromA, text: null, deleted: true }, fromB]);
      assert.equal((await s.query('select 1 from messages where sender_uuid = $1 and text is not null', [a.uuid])).length, 0);
    });

    test('signing in again starts with no history', async () => {
      const { a, b } = await friends();
      await sent(s, a, b, 'from a');
      await sent(s, b, a, 'from b');
      await s.api('DELETE', '/v1/me', { token: a.token });
      const again: Player = { ...a, token: await s.signIn(a.name, a.uuid) };
      assert.deepEqual((await chat.history(again, b)).body.messages, []);
      await s.makeFriends(again, b);
      assert.equal(await unreadFrom(s, again, b), 0);
      assert.equal(await unreadFrom(s, b, again), 0);
    });

    test('removes their read markers and the ones on their conversations', async () => {
      const { a, b } = await friends();
      const fromA = await sent(s, a, b, 'from a');
      const fromB = await sent(s, b, a, 'from b');
      await chat.read(a, b, fromB.id);
      await chat.read(b, a, fromA.id);
      await s.api('DELETE', '/v1/me', { token: a.token });
      assert.equal((await s.query('select 1 from chat_reads where reader_uuid = $1 or other_uuid = $1', [a.uuid])).length, 0);
    });

    test('once both players deleted their accounts nothing of the conversation is left', async () => {
      const { a, b } = await friends();
      await sent(s, a, b, 'from a');
      await sent(s, b, a, 'from b');
      await s.api('DELETE', '/v1/me', { token: a.token });
      await s.api('DELETE', '/v1/me', { token: b.token });
      assert.equal(
        (await s.query('select 1 from messages where sender_uuid in ($1, $2) or recipient_uuid in ($1, $2)', [a.uuid, b.uuid])).length,
        0,
      );
    });

    test('a blocked player is not told', async () => {
      const { a, b } = await friends();
      await sent(s, a, b, 'from a');
      await s.api('PUT', `/v1/blocks/${a.uuid}`, { token: b.token });
      const socket = await s.connect(b);
      await s.api('DELETE', '/v1/me', { token: a.token });
      await socket.none('chat.deleted');
    });

    test('reports they filed go with them', async () => {
      const { a, b } = await friends();
      const message = await sent(s, a, b, 'nasty');
      await chat.report(b, a, message.id);
      await s.api('DELETE', '/v1/me', { token: b.token });
      assert.equal((await s.query('select 1 from chat_reports where reporter_uuid = $1', [b.uuid])).length, 0);
    });
  });

  describe('signing in', () => {
    for (const [method, path] of [
      ['GET', `/v1/chats/${makeUuid(1)}/messages`],
      ['POST', `/v1/chats/${makeUuid(1)}/messages`],
      ['POST', `/v1/chats/${makeUuid(1)}/read`],
      ['POST', `/v1/chats/${makeUuid(1)}/messages/1/report`],
    ] as const) {
      test(`${method} ${path.replace(makeUuid(1), '{uuid}')} needs a token`, async () => {
        const res = await s.api<ErrorBody>(method, path, { body: method === 'POST' ? {} : undefined });
        assert.equal(res.status, 401);
        assert.equal(res.body.error, 'unauthorized');
      });
    }
  });
});

describe('chat rate limits', () => {
  test('messages past the per-minute limit are 429 over HTTP and an error over the socket', async () => {
    const s = await startServer({ rateLimits: { chatMessagesPerMinute: 2 } });
    try {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.makeFriends(a, b);
      const chat = chatApi(s);
      await sent(s, a, b, 'one');
      await sent(s, a, b, 'two');
      const limited = await chat.send(a, b, 'three');
      assert.equal(limited.status, 429);
      assert.equal(limited.body.error, 'rate_limited');
      assert.equal(limited.headers.get('retry-after'), '60');
      const socket = await s.connect(a);
      socket.send({ type: 'chat.send', ref: 'x', to: b.uuid, text: 'three' });
      const error = await socket.next('error');
      assert.deepEqual([error.ref, error.error, error.retryAfter], ['x', 'rate_limited', 60]);
      s.clock.advance(60);
      await sent(s, a, b, 'three');
    } finally {
      await s.close();
    }
  });

  test('messages past the daily limit are 429', async () => {
    const s = await startServer({ rateLimits: { chatMessagesPerDay: 2 } });
    try {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.makeFriends(a, b);
      await sent(s, a, b, 'one');
      s.clock.advance(3600);
      await sent(s, a, b, 'two');
      assert.equal((await chatApi(s).send(a, b, 'three')).status, 429);
      s.clock.advance(DAY);
      await sent(s, a, b, 'three');
    } finally {
      await s.close();
    }
  });

  test('a refused message counts too', async () => {
    const s = await startServer({ rateLimits: { chatMessagesPerMinute: 1 } });
    try {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.makeFriends(a, b);
      const chat = chatApi(s);
      assert.equal((await chat.send(a, makeUuid(999_999), 'probe')).status, 404);
      assert.equal((await chat.send(a, b, 'hi')).status, 429);
    } finally {
      await s.close();
    }
  });

  test('reports past the daily limit are 429', async () => {
    const s = await startServer({ rateLimits: { chatReportsPerDay: 1 } });
    try {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.makeFriends(a, b);
      const chat = chatApi(s);
      const one = await sent(s, a, b, 'one');
      const two = await sent(s, a, b, 'two');
      assert.equal((await chat.report(b, a, one.id)).status, 204);
      assert.equal((await chat.report(b, a, two.id)).status, 429);
    } finally {
      await s.close();
    }
  });
});
