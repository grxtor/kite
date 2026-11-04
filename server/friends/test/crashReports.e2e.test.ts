import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { crashSignature } from '../src/crashReports.ts';
import { maxSamplesPerSignature } from '../src/crashStore.ts';
import { crashBody, crashLines, otherLines } from './crashFixtures.ts';
import { startServer, type TestServer } from './helpers.ts';

interface SignatureRow {
  signature: string;
  report_count: string;
  lines: string[];
}

const daySeconds = 24 * 60 * 60;
const signature = crashSignature(crashLines);

describe('POST /v1/crash-reports', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  const send = (body: unknown, headers: Record<string, string> = {}) =>
    s.api('POST', '/v1/crash-reports', { body, headers });
  const signatures = () => s.query<SignatureRow>('select signature, report_count, lines from crash_signatures order by signature');
  const samples = (sig: string) =>
    s.query<{ payload: Record<string, unknown> }>('select payload from crash_reports where signature = $1 order by id', [sig]);

  test('takes a report without signing in and answers 202 with nothing in it', async () => {
    const res = await send(crashBody());
    assert.equal(res.status, 202);
    assert.equal(res.body, undefined);
    assert.equal(res.headers.get('content-length') ?? '0', '0');
  });

  test('the same crash from another computer counts under one signature', async () => {
    const res = await send(crashBody(crashLines, { os: 'windows', arch: 'x86_64', java: 21, kite: '1.2.1' }));
    assert.equal(res.status, 202);
    const rows = await signatures();
    assert.equal(rows.length, 1);
    assert.equal(rows[0]?.signature, signature);
    assert.equal(rows[0]?.report_count, '2');
    assert.deepEqual(rows[0]?.lines, crashLines);
    const kept = await samples(signature);
    assert.equal(kept.length, 2);
    // The lines are kept once, on the signature.
    assert.equal('lines' in (kept[0]?.payload ?? {}), false);
    assert.equal(kept[1]?.payload.os, 'windows');
  });

  test('another crash gets its own signature', async () => {
    const res = await send(crashBody(otherLines(1)));
    assert.equal(res.status, 202);
    assert.equal((await signatures()).length, 2);
  });

  test('a refused report stores nothing', async () => {
    const before = await signatures();
    const res = await send(crashBody(crashLines, { instance: 'My World' }));
    assert.equal(res.status, 400);
    assert.equal((await send(crashBody(['java.lang.Error: at /Users/steve/x']))).status, 400);
    assert.deepEqual(await signatures(), before);
  });

  test('keeps the newest samples of a signature and counts the rest', async () => {
    const lines = otherLines(2);
    const total = maxSamplesPerSignature + 3;
    for (let i = 0; i < total; i += 1) {
      assert.equal((await send(crashBody(lines, { kite: `1.0.${i}` }))).status, 202);
    }
    const sig = crashSignature(lines);
    const row = (await signatures()).find((one) => one.signature === sig);
    assert.equal(row?.report_count, String(total));
    const kept = await samples(sig);
    assert.equal(kept.length, maxSamplesPerSignature);
    assert.equal(kept[0]?.payload.kite, `1.0.${total - maxSamplesPerSignature}`);
    assert.equal(kept.at(-1)?.payload.kite, `1.0.${total - 1}`);
  });

  test('refuses a body over 64 KB', async () => {
    const mods = Array.from({ length: 500 }, (_, i) => ({ id: `mod${i}${'x'.repeat(60)}`.slice(0, 64), version: 'v'.repeat(64) }));
    const res = await send(crashBody(crashLines, { mods }));
    assert.equal(res.status, 413);
  });

  test('ignores a session token and needs none', async () => {
    const res = await s.api('POST', '/v1/crash-reports', { body: crashBody(), token: 'kfs_not-a-real-token' });
    assert.equal(res.status, 202);
  });

  test('keeps the day a report came, not the time', async () => {
    const rows = await s.query<{ received_on: Date }>('select received_on from crash_reports limit 1');
    const day = rows[0]?.received_on;
    assert.ok(day instanceof Date);
    assert.equal(day.getHours() + day.getMinutes() + day.getSeconds(), 0);
  });
});

describe('crash report privacy', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer({ clientIpHeader: 'cf-connecting-ip' });
  });

  after(async () => {
    await s.close();
  });

  test('no address or header value is stored anywhere', async () => {
    const markers = ['203.0.113.77', '198.51.100.23', 'Kite/9.9.9-marker'];
    const res = await s.api('POST', '/v1/crash-reports', {
      body: crashBody(),
      headers: {
        'cf-connecting-ip': markers[0] ?? '',
        'x-forwarded-for': markers[1] ?? '',
        'user-agent': markers[2] ?? '',
      },
    });
    assert.equal(res.status, 202);
    const tables = await s.query<{ table_name: string }>(
      `select table_name from information_schema.tables where table_schema = 'public' and table_type = 'BASE TABLE'`,
    );
    let rows = 0;
    for (const { table_name } of tables) {
      const dump = await s.query<{ row: string }>(`select t::text as row from "${table_name}" t`);
      rows += dump.length;
      for (const { row } of dump) {
        for (const marker of [...markers, '127.0.0.1']) {
          assert.equal(row.includes(marker), false, `${table_name} holds ${marker}`);
        }
      }
    }
    assert.ok(rows >= 2);
  });
});

describe('crash report retention', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  const count = async (table: string) =>
    Number((await s.query<{ n: string }>(`select count(*) as n from ${table}`))[0]?.n);

  test('reports go after 90 days, the signature after a year without one', async () => {
    assert.equal((await s.api('POST', '/v1/crash-reports', { body: crashBody() })).status, 202);
    s.clock.advance(89 * daySeconds);
    await s.purge();
    assert.equal(await count('crash_reports'), 1);

    s.clock.advance(2 * daySeconds);
    await s.purge();
    assert.equal(await count('crash_reports'), 0);
    assert.equal(await count('crash_signatures'), 1);

    s.clock.advance(300 * daySeconds);
    await s.purge();
    assert.equal(await count('crash_signatures'), 0);
  });

  test('a signature sent again stays and keeps its first sighting', async () => {
    assert.equal((await s.api('POST', '/v1/crash-reports', { body: crashBody() })).status, 202);
    const first = s.clock.iso();
    s.clock.advance(200 * daySeconds);
    assert.equal((await s.api('POST', '/v1/crash-reports', { body: crashBody() })).status, 202);
    s.clock.advance(200 * daySeconds);
    await s.purge();
    const rows = await s.query<{ first_seen: Date; report_count: string }>('select first_seen, report_count from crash_signatures');
    assert.equal(rows.length, 1);
    assert.equal(rows[0]?.first_seen.toISOString(), first);
    assert.equal(rows[0]?.report_count, '2');
    assert.equal(await count('crash_reports'), 0);
  });
});

describe('crash report rate limit', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer({ rateLimits: { crashReportsPerHour: 2 } });
  });

  after(async () => {
    await s.close();
  });

  test('allows a few an hour per address, kept in memory only', async () => {
    assert.equal((await s.api('POST', '/v1/crash-reports', { body: crashBody() })).status, 202);
    assert.equal((await s.api('POST', '/v1/crash-reports', { body: crashBody(otherLines(3)) })).status, 202);
    const refused = await s.api('POST', '/v1/crash-reports', { body: crashBody(otherLines(4)) });
    assert.equal(refused.status, 429);
    assert.ok(Number(refused.headers.get('retry-after')) > 0);
    // A bad report counts too, so guessing what passes is slow.
    s.clock.advance(60 * 60);
    assert.equal((await s.api('POST', '/v1/crash-reports', { body: { nope: true } })).status, 400);
    assert.equal((await s.api('POST', '/v1/crash-reports', { body: crashBody(otherLines(5)) })).status, 202);
    assert.equal((await s.api('POST', '/v1/crash-reports', { body: crashBody(otherLines(6)) })).status, 429);
  });
});
