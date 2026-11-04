import assert from 'node:assert/strict';
import { execFile } from 'node:child_process';
import { mkdtemp, readdir, readFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { after, before, describe, test } from 'node:test';
import { promisify } from 'node:util';
import pg from 'pg';
import { crashSignature, parseCrashReport, sampleOf } from '../src/crashReports.ts';
import { findSignature, recordCrashReport, resolveSignature, samplesOf, topSignatures } from '../src/crashStore.ts';
import { crashReportText, summarize } from '../src/crashTop.ts';
import { createPool, migrate } from '../src/db.ts';
import { crashBody, crashLines, otherLines } from './crashFixtures.ts';
import { createTestDatabase, type TestDatabase } from './infra.ts';

const run = promisify(execFile);
const script = new URL('../scripts/crash-top.ts', import.meta.url).pathname;
const now = new Date('2026-11-04T12:00:00Z');

describe('crash summaries', () => {
  const sample = (extra: Record<string, unknown>) => sampleOf(parseCrashReport(crashBody(crashLines, extra)));

  test('list the mods most samples share, and how often each version comes', () => {
    const summary = summarize([
      sample({}),
      sample({ mods: [{ id: 'create', version: '0.5.1.i' }, { id: 'jei', version: '15' }] }),
      sample({
        mods: [{ id: 'create', version: '0.5.1.j' }, { id: 'flywheel', version: '0.6.11' }],
        culprit: 'flywheel',
        loader: { type: 'neoforge' },
      }),
    ]);
    assert.equal(summary.samples, 3);
    assert.deepEqual(summary.mods.map((mod) => [mod.id, Math.round(mod.share * 100)]), [['create', 100], ['flywheel', 67]]);
    assert.deepEqual(summary.mods[0]?.versions, [
      { value: '0.5.1.j', count: 2 },
      { value: '0.5.1.i', count: 1 },
    ]);
    assert.deepEqual(summary.culprits, [{ value: 'create', count: 2 }, { value: 'flywheel', count: 1 }]);
    assert.deepEqual(summary.loaders, [{ value: 'forge 47.3.0', count: 2 }, { value: 'neoforge', count: 1 }]);
  });

  test('a crash report text indents the frames', () => {
    assert.equal(crashReportText(['java.lang.Error: x', 'at a.B.c(B.java)']), 'java.lang.Error: x\n\tat a.B.c(B.java)\n');
  });
});

describe('crash-top', () => {
  let db: TestDatabase;
  let pool: pg.Pool;
  let dumpDir: string;

  before(async () => {
    db = await createTestDatabase();
    pool = createPool(db.url);
    await migrate(pool);
    dumpDir = await mkdtemp(join(tmpdir(), 'crash-top-'));
    for (let i = 0; i < 3; i += 1) await recordCrashReport(pool, parseCrashReport(crashBody()), now);
    await recordCrashReport(pool, parseCrashReport(crashBody(otherLines(1))), now);
  });

  after(async () => {
    await pool.end();
    await db.drop();
    await rm(dumpDir, { recursive: true, force: true });
  });

  const crashTop = (...args: string[]) =>
    run(process.execPath, [script, ...args], { env: { ...process.env, DATABASE_URL: db.url } });

  test('lists the most sent unresolved signatures first', async () => {
    const top = await topSignatures(pool, 10);
    assert.deepEqual(top.map((row) => row.reportCount), [3, 1]);
    const { stdout } = await crashTop('--limit', '1');
    assert.match(stdout, /^#1 {2}[0-9a-f]{12} {2}3 reports {2}first 2026-11-04 {2}last 2026-11-04/);
    assert.match(stdout, /Carriage\.tick/);
    assert.match(stdout, /Mods in most of the 3 samples: create 100%, flywheel 100%/);
    assert.doesNotMatch(stdout, /#2/);
  });

  test('finds a signature by its start', async () => {
    const full = crashSignature(crashLines);
    assert.equal((await findSignature(pool, full.slice(0, 8))).signature, full);
    await assert.rejects(findSignature(pool, full.slice(0, 7)), /8 to 64/);
    await assert.rejects(findSignature(pool, 'ffffffffff'), /No signature/);
  });

  test('dumps a signature for writing its rule', async () => {
    const full = crashSignature(crashLines);
    const { stdout } = await crashTop('--signature', full.slice(0, 10), '--dump', dumpDir);
    assert.match(stdout, new RegExp(`^${full}\\n`));
    const folder = join(dumpDir, full.slice(0, 12));
    assert.deepEqual((await readdir(folder)).sort(), ['case.json', 'lines.txt', 'samples', 'summary.json']);
    assert.equal((await readdir(join(folder, 'samples'))).length, 3);
    const lines = await readFile(join(folder, 'lines.txt'), 'utf8');
    assert.equal(lines, crashReportText(crashLines));
    const testCase = JSON.parse(await readFile(join(folder, 'case.json'), 'utf8')) as { docs: { crashReport: string } };
    assert.equal(testCase.docs.crashReport, lines);
  });

  test('resolving takes a signature off the list', async () => {
    const full = crashSignature(crashLines);
    const { stdout } = await crashTop('--resolve', full.slice(0, 12), '--rule', 'mods.create-carriage-tick');
    assert.equal(stdout.trim(), `${full} is resolved by mods.create-carriage-tick`);
    assert.deepEqual((await topSignatures(pool, 10)).map((row) => row.reportCount), [1]);
    assert.equal((await topSignatures(pool, 10, true)).length, 2);
    assert.equal((await samplesOf(pool, full)).length, 3);
  });

  test('refuses what is not a rule id, and says why', async () => {
    const full = crashSignature(otherLines(1));
    await assert.rejects(resolveSignature(pool, full, 'Not A Rule', now), /not a crash rule id/);
    await assert.rejects(crashTop('--resolve', full, '--rule', 'nope'), (error: { code?: number; stderr?: string }) => {
      assert.equal(error.code, 1);
      assert.match(error.stderr ?? '', /not a crash rule id/);
      return true;
    });
    await assert.rejects(crashTop('--resolve', full), /--rule/);
  });
});
