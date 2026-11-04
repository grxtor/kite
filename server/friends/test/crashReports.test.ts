import assert from 'node:assert/strict';
import { describe, test } from 'node:test';
import {
  crashSignature,
  maxCrashLineLength,
  maxCrashLines,
  maxCrashMods,
  parseCrashReport,
  sampleOf,
} from '../src/crashReports.ts';
import { HttpError } from '../src/errors.ts';
import { crashBody, crashLines } from './crashFixtures.ts';

function rejects(body: unknown, message?: RegExp): void {
  assert.throws(
    () => parseCrashReport(body),
    (error: unknown) =>
      error instanceof HttpError && error.status === 400 && (message === undefined || message.test(error.message)),
  );
}

const withLines = (lines: string[]) => crashBody(lines);

describe('crash signature', () => {
  test('is the SHA-256 of the lines joined by newlines', () => {
    // printf 'a\nb' | shasum -a 256
    assert.equal(crashSignature(['a', 'b']), '7e18f737311b2dc3b2f269dd78396b0351f14fb66efa879f768cb23181883c78');
    assert.notEqual(crashSignature(['a', 'b']), crashSignature(['b', 'a']));
    assert.match(crashSignature(crashLines), /^[0-9a-f]{64}$/);
  });

  test('hashes the UTF-8 bytes', () => {
    // printf 'ç' | shasum -a 256
    assert.equal(crashSignature(['ç']), '8bfa829b8119a6f39b91fd8decec63830b556e4d88a9da29334d7b0558829f2d');
  });
});

describe('parseCrashReport', () => {
  test('accepts a valid report and copies only known fields', () => {
    const report = parseCrashReport(crashBody());
    assert.equal(report.signature, crashSignature(crashLines));
    assert.deepEqual(report.lines, crashLines);
    assert.deepEqual(report.loader, { type: 'forge', version: '47.3.0' });
    assert.equal(report.culprit, 'create');
  });

  test('accepts no culprit, an unknown Java and a loader without a version', () => {
    const body = crashBody(crashLines, { java: null, loader: { type: 'fabric' } });
    delete body.culprit;
    const report = parseCrashReport(body);
    assert.equal(report.java, null);
    assert.deepEqual(report.loader, { type: 'fabric' });
    assert.equal('culprit' in report, false);
  });

  test('accepts vanilla with no mods', () => {
    assert.deepEqual(parseCrashReport(crashBody(crashLines, { loader: { type: 'vanilla' }, mods: [] })).loader, {
      type: 'vanilla',
    });
  });

  test('refuses a field that is not listed, at any level', () => {
    rejects(crashBody(crashLines, { instance: 'My World' }), /not allowed/);
    rejects(crashBody(crashLines, { user: 'Steve' }), /not allowed/);
    rejects(crashBody(crashLines, { loader: { type: 'forge', version: '47.3.0', path: '/x' } }), /not allowed/);
    rejects(crashBody(crashLines, { mods: [{ id: 'create', version: '1', file: 'create.jar' }] }), /not allowed/);
  });

  test('refuses another format version', () => {
    rejects(crashBody(crashLines, { formatVersion: 2 }), /formatVersion/);
    rejects(crashBody(crashLines, { formatVersion: '1' }), /formatVersion/);
  });

  test('refuses a signature that does not match the lines', () => {
    rejects(crashBody(crashLines, { signature: crashSignature(['something else']) }), /does not match/);
    rejects(crashBody(crashLines, { signature: crashSignature(crashLines).toUpperCase() }), /signature/);
    rejects(crashBody(crashLines, { signature: 'abc' }), /signature/);
  });

  test('refuses too many, empty or too long lines', () => {
    rejects(withLines([]), /empty/);
    rejects(withLines(Array.from({ length: maxCrashLines + 1 }, (_, i) => `at a.B.c${i}(B.java)`)), /more than/);
    rejects(withLines(['x'.repeat(maxCrashLineLength + 1)]), /characters/);
    rejects(withLines(['']), /characters/);
    assert.equal(parseCrashReport(withLines(['x'.repeat(maxCrashLineLength)])).lines.length, 1);
  });

  test('refuses control and bidi characters', () => {
    rejects(withLines(['java.lang.Error: a\nb']), /control/);
    rejects(withLines(['java.lang.Error: \u001b[31mred']), /control/);
    rejects(withLines(['java.lang.Error: \u202eevil']), /control/);
  });

  test('refuses lines that still hold something personal', () => {
    for (const line of [
      'java.lang.Error: player 069a79f4-44e9-4726-a5be-fca90e38aaf5 left',
      'java.net.ConnectException: Connection refused: /192.168.1.20:25565',
      'java.net.ConnectException: to 2001:db8:85a3:0:0:8a2e:370:7334',
      'java.io.FileNotFoundException: /Users/steve/Library/Application Support/kite/x.json',
      'java.io.FileNotFoundException: /home/steve/.kite/x.json',
      'java.io.FileNotFoundException: C:\\Users\\Steve\\AppData\\Roaming\\kite\\x.json',
      'java.io.FileNotFoundException: file:/Users/steve/x.jar',
      'java.lang.Error: write to steve@example.com',
    ]) {
      rejects(withLines([line]), /personal/);
    }
  });

  test('keeps lines the launcher scrubbed, versions and module frames', () => {
    for (const line of [
      'java.io.FileNotFoundException: <path>/options.txt',
      'at TRANSFORMER/create@0.5.1.j/com.simibubi.create.Foo.bar(Foo.java)',
      'java.lang.IllegalStateException: Player@<hex>,level.dimension broke',
      'java.lang.Error: Mod create requires flywheel 1.0.0',
      'at java.base/java.lang.Thread.run(Thread.java)',
      'java.lang.Error: <ip> and <uuid> are gone',
    ]) {
      assert.equal(parseCrashReport(withLines([line])).lines[0], line);
    }
  });

  test('checks every other field', () => {
    rejects(crashBody(crashLines, { os: 'beos' }), /os/);
    rejects(crashBody(crashLines, { arch: 'mips' }), /arch/);
    rejects(crashBody(crashLines, { java: 4 }), /java/);
    rejects(crashBody(crashLines, { java: 17.5 }), /java/);
    rejects(crashBody(crashLines, { kite: '1.2.0; rm -rf /' }), /kite/);
    rejects(crashBody(crashLines, { minecraft: '' }), /minecraft/);
    rejects(crashBody(crashLines, { loader: { type: 'bukkit' } }), /loader/);
    rejects(crashBody(crashLines, { loader: { type: 'vanilla', version: '1' } }), /vanilla/);
    rejects(crashBody(crashLines, { loader: { type: 'vanilla' } }), /vanilla loads no mods/);
    rejects(crashBody(crashLines, { culprit: 'Create Mod' }), /culprit/);
    rejects(crashBody(crashLines, { rules: ['Mods Crashed'] }), /rules/);
    rejects(crashBody(crashLines, { rules: ['mods.crashed', 'mods.crashed'] }), /twice/);
    rejects(crashBody(crashLines, { rules: 'mods.crashed' }), /array/);
  });

  test('checks the mod list', () => {
    rejects(crashBody(crashLines, { mods: [{ id: 'Create', version: '1' }] }), /mods\[0\]\.id/);
    rejects(crashBody(crashLines, { mods: [{ id: 'create', version: '1 beta' }] }), /mods\[0\]\.version/);
    rejects(crashBody(crashLines, { mods: [{ id: 'create', version: '1' }, { id: 'create', version: '2' }] }), /twice/);
    const many = Array.from({ length: maxCrashMods + 1 }, (_, i) => ({ id: `mod${i}`, version: '1' }));
    rejects(crashBody(crashLines, { mods: many }), /more than/);
    assert.equal(parseCrashReport(crashBody(crashLines, { mods: many.slice(1) })).mods.length, maxCrashMods);
    assert.equal(parseCrashReport(crashBody(crashLines, { mods: [{ id: 'create', version: '' }] })).mods[0]?.version, '');
  });

  test('refuses a body that is not an object', () => {
    for (const body of [null, [], 'report', 42]) rejects(body);
  });

  test('a sample keeps everything but the signature and lines', () => {
    const sample = sampleOf(parseCrashReport(crashBody()));
    assert.equal('signature' in sample, false);
    assert.equal('lines' in sample, false);
    assert.equal(sample.culprit, 'create');
    assert.equal(sample.mods.length, 2);
  });
});
