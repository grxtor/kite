import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { after, before, describe, test } from 'node:test';
import { promisify } from 'node:util';
import pg from 'pg';
import { createPool } from '../src/db.ts';
import { parseDuration, runMod } from '../src/modCli.ts';
import { ModError } from '../src/modStore.ts';
import { closeCodes } from '../src/routes/socket.ts';
import { startServer } from './helpers.ts';
import type { ErrorBody, Player, TestServer } from './helpers.ts';

const run = promisify(execFile);
const script = new URL('../scripts/mod.ts', import.meta.url).pathname;
const HOUR_MS = 60 * 60 * 1000;

interface LogRow {
  actor: string;
  action: string;
  target: string;
  note: string | null;
}

describe('pnpm mod', () => {
  let s: TestServer;
  let pool: pg.Pool;
  let griefer: Player;
  let reporter: Player;
  let leaver: Player;
  const ids: string[] = [];

  const mod = (...args: string[]) => runMod(pool, args, new Date(s.clock.now()));
  const refused = (args: string[], pattern: RegExp) =>
    assert.rejects(mod(...args), (error: Error) => error instanceof ModError && pattern.test(error.message));
  const lastLog = async () =>
    (await s.query<LogRow>('select actor, action, target, note from moderation_log order by id desc limit 1'))[0];
  const send = (from: Player, to: Player, text = 'hi') =>
    s.api<{ id: string } & ErrorBody>('POST', `/v1/chats/${to.uuid}/messages`, { token: from.token, body: { text } });

  async function report(by: Player, text: string, reason: string): Promise<string> {
    const message = await send(griefer, by, text);
    const res = await s.api('POST', `/v1/chats/${griefer.uuid}/messages/${message.body.id}/report`, {
      token: by.token,
      body: { reason },
    });
    assert.equal(res.status, 204);
    const rows = await s.query<{ id: string }>('select id from chat_reports order by id desc limit 1');
    return rows[0]!.id;
  }

  before(async () => {
    s = await startServer({ heartbeatMs: 50 });
    pool = createPool(s.databaseUrl);
    griefer = await s.newPlayer('Griefer');
    reporter = await s.newPlayer('Reporter');
    leaver = await s.newPlayer('Leaver');
    await s.makeFriends(griefer, reporter);
    await s.makeFriends(griefer, leaver);
    await send(reporter, griefer, 'hello');
    ids.push(await report(reporter, 'you are bad', 'harassment'));
    ids.push(await report(leaver, 'buy coins', 'spam'));
    // The second reporter leaves: the report stays without them.
    assert.equal((await s.api('DELETE', '/v1/me', { token: leaver.token })).status, 204);
  });

  after(async () => {
    await pool.end();
    await s.close();
  });

  test('list shows every report newest first, with a deleted reporter as silinmiş hesap', async () => {
    const lines = (await mod('list')).split('\n');
    assert.equal(lines.length, 2);
    assert.match(lines[0]!, new RegExp(`^#${ids[1]}  2026-10-24 18:00  spam  şikâyet eden silinmiş hesap  şikâyet edilen Griefer  açık$`));
    assert.match(lines[1]!, new RegExp(`^#${ids[0]}  2026-10-24 18:00  taciz  şikâyet eden Reporter  şikâyet edilen Griefer  açık$`));
    assert.equal((await mod('list', '--limit', '1')).split('\n').length, 1);
  });

  test('show prints the copy with each side marked and the reported message flagged', async () => {
    const text = await mod('show', `#${ids[0]}`);
    assert.match(text, new RegExp(`^#${ids[0]}  açık\n`));
    assert.match(text, /Sebep: taciz/);
    assert.match(text, new RegExp(`Şikâyet eden: Reporter \\(${reporter.uuid}\\)`));
    assert.match(text, new RegExp(`Şikâyet edilen: Griefer \\(${griefer.uuid}\\)`));
    assert.match(text, /\n {3}2026-10-24 18:00 {2}\[şikâyet eden {2}\] {2}hello\n/);
    assert.match(text, /\n>> 2026-10-24 18:00 {2}\[şikâyet edilen\] {2}you are bad$/);
    assert.match(await mod('show', ids[1]!), /Şikâyet eden: silinmiş hesap/);
    await refused(['show', '999999'], /#999999 diye bir şikâyet yok/);
  });

  test('mute stops sending until it runs out, and unmute lifts it', async () => {
    assert.match(await mod('mute', griefer.uuid.toUpperCase(), '2h'), /2026-10-24 20:00 UTC'ye kadar susturuldu/);
    assert.deepEqual(await lastLog(), { actor: 'cli', action: 'mute', target: griefer.uuid, note: 'until 2026-10-24T20:00:00.000Z' });
    assert.equal((await send(griefer, reporter)).body.error, 'muted');
    assert.equal((await send(reporter, griefer)).status, 201);

    await mod('unmute', griefer.uuid);
    assert.deepEqual(await lastLog(), { actor: 'cli', action: 'unmute', target: griefer.uuid, note: null });
    assert.equal((await send(griefer, reporter)).status, 201);
    assert.equal((await s.query('select 1 from sanctions where uuid = $1', [griefer.uuid])).length, 0);
    await refused(['unmute', griefer.uuid], /susturulmuş değil/);
  });

  test('ban closes the sockets with 4403 and stops chat, invites and friend requests; unban lifts it', async () => {
    const socket = await s.connect(griefer);
    await mod('ban', griefer.uuid, '--reason', 'kept harassing');
    assert.deepEqual(await lastLog(), { actor: 'cli', action: 'ban', target: griefer.uuid, note: 'kept harassing' });
    assert.equal((await socket.closed).code, closeCodes.banned);
    assert.deepEqual(
      (await send(griefer, reporter)).body,
      { error: 'banned', message: 'This account may not chat, invite or send friend requests', reason: 'kept harassing' },
    );
    const invite = await s.api<ErrorBody>('PUT', `/v1/invites/${reporter.uuid}`, {
      token: griefer.token,
      body: { host: 'play.example.com' },
    });
    assert.equal(invite.body.error, 'banned');
    const stranger = await s.newPlayer();
    const request = await s.api<ErrorBody>('POST', '/v1/friends/requests', { token: griefer.token, body: { name: stranger.name } });
    assert.equal(request.body.error, 'banned');
    await refused(['ban', griefer.uuid], /zaten yasaklı/);

    await mod('unban', griefer.uuid);
    assert.deepEqual(await lastLog(), { actor: 'cli', action: 'unban', target: griefer.uuid, note: null });
    assert.equal((await send(griefer, reporter)).status, 201);
    await refused(['unban', griefer.uuid], /yasaklı değil/);
  });

  test('a ban without a reason, and on a player who never signed in', async () => {
    const unknown = '11111111-2222-4333-8444-555555555555';
    await mod('ban', unknown);
    assert.deepEqual(await lastLog(), { actor: 'cli', action: 'ban', target: unknown, note: null });
    await mod('unban', unknown);
  });

  test('resolve and dismiss close a report once, with an optional note', async () => {
    assert.equal(await mod('resolve', ids[0]!, '--note', 'muted for a day'), `#${ids[0]} çözüldü`);
    assert.deepEqual(await lastLog(), { actor: 'cli', action: 'resolve', target: ids[0], note: 'muted for a day' });
    assert.equal(await mod('dismiss', ids[1]!), `#${ids[1]} reddedildi`);
    assert.deepEqual(await lastLog(), { actor: 'cli', action: 'dismiss', target: ids[1], note: null });
    const rows = await s.query('select id, status, closed_at, resolution_note from chat_reports order by id');
    assert.deepEqual(rows, [
      { id: ids[0], status: 'resolved', closed_at: new Date(s.clock.now()), resolution_note: 'muted for a day' },
      { id: ids[1], status: 'dismissed', closed_at: new Date(s.clock.now()), resolution_note: null },
    ]);
    assert.match(await mod('show', ids[0]!), /^#\d+ {2}çözüldü 2026-10-24 18:00\n[\s\S]*\nNot: muted for a day\n/);
    await refused(['resolve', ids[0]!], /zaten kapalı \(resolved\)/);
    await refused(['dismiss', ids[0]!], /zaten kapalı/);
    await refused(['resolve', '999999'], /diye bir şikâyet yok/);
    assert.equal(await mod('list', '--open'), 'Açık şikâyet yok.');
    assert.match(await mod('list'), /çözüldü[\s\S]*reddedildi|reddedildi[\s\S]*çözüldü/);
  });

  test('refuses what it does not understand, and logs nothing for it', async () => {
    const before = (await s.query('select count(*)::int as n from moderation_log'))[0];
    await refused(['mute', 'not-a-uuid', '1h'], /UUID/);
    await refused(['mute', griefer.uuid], /süre gerekiyor/);
    await refused(['mute', griefer.uuid, '0h'], /süre 0h/);
    await refused(['mute', griefer.uuid, '400d'], /en çok 365d/);
    await refused(['mute', griefer.uuid, '1y'], /süre 1y/);
    await refused(['show', 'abc'], /şikâyet numarası/);
    await refused(['ban', griefer.uuid, '--note', 'x'], /--note ban ile kullanılmaz/);
    await refused(['ban', griefer.uuid, '--reason', ' '], /--reason 1 ile 500/);
    await refused(['list', '--limit', '0'], /--limit/);
    await refused(['list', 'extra'], /fazla argüman/);
    await refused(['unban', griefer.uuid, 'extra'], /fazla argüman/);
    await refused(['--wat'], /Unknown option/);
    await refused(['frobnicate'], /^pnpm mod list/);
    await refused([], /^pnpm mod list/);
    assert.deepEqual((await s.query('select count(*)::int as n from moderation_log'))[0], before);
  });

  test('durations', () => {
    assert.equal(parseDuration('30m'), 30 * 60 * 1000);
    assert.equal(parseDuration('12h'), 12 * HOUR_MS);
    assert.equal(parseDuration('7d'), 7 * 24 * HOUR_MS);
    assert.equal(parseDuration('2w'), 14 * 24 * HOUR_MS);
    assert.equal(parseDuration('365d'), 365 * 24 * HOUR_MS);
  });

  test('the script prints the answer, and a refusal on stderr with exit code 1', async () => {
    const env = { ...process.env, DATABASE_URL: s.databaseUrl };
    const { stdout } = await run(process.execPath, [script, 'list', '--open'], { env });
    assert.equal(stdout, 'Açık şikâyet yok.\n');
    await assert.rejects(run(process.execPath, [script, 'unban', griefer.uuid], { env }), (error: { code: number; stderr: string }) => {
      assert.equal(error.code, 1);
      assert.match(error.stderr, /yasaklı değil/);
      return true;
    });
  });
});
