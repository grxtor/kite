import assert from 'node:assert/strict';
import { describe, test } from 'node:test';
import { HttpError } from '../src/errors.ts';
import {
  displayShareCode,
  maxContentEntries,
  newShareCode,
  normalizeShareCode,
  parseManifest,
  shareCodeAlphabet,
  shareCodeLength,
} from '../src/shareCodes.ts';

const modrinth = { kind: 'mod', platform: 'modrinth', projectId: 'AANobbMI', versionId: 'IZskON6d' };
const curseforge = { kind: 'resourcepack', platform: 'curseforge', projectId: 238_222, fileId: 5_432_100 };
const entries = (count: number) => Array.from({ length: count }, (_, i) => ({ ...curseforge, projectId: i + 1 }));
const valid = {
  name: 'Survival 1.21',
  minecraftVersion: '1.21.4',
  loader: { type: 'fabric', version: '0.16.10' },
  content: [modrinth, curseforge],
};

function rejects(body: unknown): void {
  assert.throws(
    () => parseManifest(body),
    (error: unknown) => error instanceof HttpError && error.status === 400 && error.code === 'bad_request',
  );
}

describe('share code format', () => {
  test('the alphabet has no 0, O, 1, I or L', () => {
    for (const ambiguous of '0O1IL') assert.equal(shareCodeAlphabet.includes(ambiguous), false, ambiguous);
    assert.equal(new Set(shareCodeAlphabet).size, shareCodeAlphabet.length);
  });

  test('codes are long enough that guessing one is impractical', () => {
    const bits = shareCodeLength * Math.log2(shareCodeAlphabet.length);
    assert.ok(bits >= 39, `${bits.toFixed(1)} bits`);
  });

  test('new codes use only the alphabet and do not repeat', () => {
    const codes = new Set<string>();
    for (let i = 0; i < 5000; i += 1) {
      const code = newShareCode();
      assert.match(code, new RegExp(`^[${shareCodeAlphabet}]{${shareCodeLength}}$`));
      codes.add(code);
    }
    assert.equal(codes.size, 5000);
  });

  test('every character of the alphabet turns up', () => {
    const seen = new Set<string>();
    for (let i = 0; i < 2000; i += 1) for (const c of newShareCode()) seen.add(c);
    assert.equal(seen.size, shareCodeAlphabet.length);
  });

  test('is displayed as KITE-XXXX-XXXX', () => {
    assert.equal(displayShareCode('7F3K9QXM'), 'KITE-7F3K-9QXM');
  });

  const accepted: Array<[string, string]> = [
    ['KITE-7F3K-9QXM', 'the displayed form'],
    ['kite-7f3k-9qxm', 'lower case'],
    ['Kite-7f3K-9QxM', 'mixed case'],
    ['7F3K-9QXM', 'no prefix'],
    ['7F3K9QXM', 'no prefix or dash'],
    ['KITE7F3K9QXM', 'no dashes'],
    [' KITE - 7F3K - 9QXM ', 'spaces'],
  ];
  for (const [raw, title] of accepted) {
    test(`normalizes ${title}`, () => {
      assert.equal(normalizeShareCode(raw), '7F3K9QXM');
    });
  }

  const refused: Array<[string, string]> = [
    ['', 'an empty string'],
    ['KITE-7F3K-9QX', 'seven characters'],
    ['KITE-7F3K-9QXMM', 'nine characters'],
    ['KITE-7F3K-9QX0', 'a zero'],
    ['KITE-7F3K-9QXO', 'an O'],
    ['KITE-7F3K-9QX1', 'a one'],
    ['KITE-7F3K-9QXI', 'an I'],
    ['KITE-7F3K-9QXL', 'an L'],
    ['KITE-7F3K-9QX!', 'punctuation'],
    ['KITE-7F3K/9QXM', 'a slash'],
    ['KITE', 'only the prefix'],
  ];
  for (const [raw, title] of refused) {
    test(`refuses ${title}`, () => {
      assert.equal(normalizeShareCode(raw), null);
    });
  }
});

describe('parseManifest', () => {
  test('accepts a valid manifest and returns an equal copy', () => {
    const parsed = parseManifest(valid);
    assert.deepEqual(parsed, valid);
    assert.notEqual(parsed, valid);
    assert.notEqual(parsed.content[0], valid.content[0]);
  });

  for (const type of ['fabric', 'quilt', 'forge', 'neoforge']) {
    test(`accepts the ${type} loader`, () => {
      assert.equal(parseManifest({ ...valid, loader: { type, version: '47.3.0' } }).loader.type, type);
    });
  }

  test('accepts vanilla without a loader version and without mods', () => {
    const body = { ...valid, loader: { type: 'vanilla' }, content: [{ ...modrinth, kind: 'resourcepack' }] };
    assert.deepEqual(parseManifest(body).loader, { type: 'vanilla' });
  });

  for (const kind of ['mod', 'resourcepack', 'shaderpack', 'datapack']) {
    test(`accepts the kind ${kind}`, () => {
      assert.equal(parseManifest({ ...valid, content: [{ ...modrinth, kind }] }).content[0]?.kind, kind);
    });
  }

  test('accepts empty content', () => {
    assert.deepEqual(parseManifest({ ...valid, content: [] }).content, []);
  });

  test(`accepts ${maxContentEntries} entries`, () => {
    assert.equal(parseManifest({ ...valid, content: entries(maxContentEntries) }).content.length, maxContentEntries);
  });

  test('cleans control and bidi characters out of the name', () => {
    assert.equal(parseManifest({ ...valid, name: ' My‮ Pack\u0007 ' }).name, 'My Pack');
  });

  const rejected: Array<[string, unknown]> = [
    ['a body that is not an object', [valid]],
    ['null', null],
    ['a string', 'manifest'],
    ['an unknown top-level field', { ...valid, url: 'https://evil.example/pack.zip' }],
    ['a missing name', { ...valid, name: undefined }],
    ['an empty name', { ...valid, name: '' }],
    ['a name of only control characters', { ...valid, name: '\u0007‮' }],
    ['a 65 character name', { ...valid, name: 'x'.repeat(65) }],
    ['a name that is not a string', { ...valid, name: 42 }],
    ['a missing Minecraft version', { ...valid, minecraftVersion: undefined }],
    ['a Minecraft version with a slash', { ...valid, minecraftVersion: '1.21/../..' }],
    ['a 33 character Minecraft version', { ...valid, minecraftVersion: '1'.repeat(33) }],
    ['an unknown loader', { ...valid, loader: { type: 'optifine', version: '1' } }],
    ['a loader that is a string', { ...valid, loader: 'fabric' }],
    ['a loader without a version', { ...valid, loader: { type: 'fabric' } }],
    ['a loader version with a space', { ...valid, loader: { type: 'forge', version: '47 3' } }],
    ['a loader with a URL', { ...valid, loader: { type: 'fabric', version: '0.16.10', url: 'https://x' } }],
    ['vanilla with a loader version', { ...valid, loader: { type: 'vanilla', version: '1' } }],
    ['vanilla with a mod', { ...valid, loader: { type: 'vanilla' } }],
    ['content that is not an array', { ...valid, content: { 0: modrinth } }],
    [`${maxContentEntries + 1} entries`, { ...valid, content: entries(maxContentEntries + 1) }],
    ['an entry that is not an object', { ...valid, content: ['AANobbMI'] }],
    ['an unknown kind', { ...valid, content: [{ ...modrinth, kind: 'plugin' }] }],
    ['an unknown platform', { ...valid, content: [{ ...modrinth, platform: 'github' }] }],
    ['an entry with a url', { ...valid, content: [{ ...modrinth, url: 'https://cdn.example/x.jar' }] }],
    ['an entry with a path', { ...valid, content: [{ ...modrinth, path: '../../mods/x.jar' }] }],
    ['an entry with file bytes', { ...valid, content: [{ ...modrinth, data: 'UEsDBAo=' }] }],
    ['an entry with a sha1', { ...valid, content: [{ ...modrinth, sha1: 'a'.repeat(40) }] }],
    ['a Modrinth entry with a fileId', { ...valid, content: [{ ...modrinth, fileId: 1 }] }],
    ['a CurseForge entry with a versionId', { ...valid, content: [{ ...curseforge, versionId: 'IZskON6d' }] }],
    ['a Modrinth project id that is a URL', { ...valid, content: [{ ...modrinth, projectId: 'https://modrinth.com/mod/x' }] }],
    ['a Modrinth version id with a dot', { ...valid, content: [{ ...modrinth, versionId: 'IZsk.ON6d' }] }],
    ['a Modrinth id that is a number', { ...valid, content: [{ ...modrinth, projectId: 12345678 }] }],
    ['a 17 character Modrinth id', { ...valid, content: [{ ...modrinth, projectId: 'A'.repeat(17) }] }],
    ['a missing Modrinth version id', { ...valid, content: [{ ...modrinth, versionId: undefined }] }],
    ['a CurseForge id given as a string', { ...valid, content: [{ ...curseforge, projectId: '238222' }] }],
    ['a CurseForge id of 0', { ...valid, content: [{ ...curseforge, fileId: 0 }] }],
    ['a negative CurseForge id', { ...valid, content: [{ ...curseforge, projectId: -1 }] }],
    ['a fractional CurseForge id', { ...valid, content: [{ ...curseforge, fileId: 1.5 }] }],
    ['a CurseForge id past 32 bits', { ...valid, content: [{ ...curseforge, fileId: 2_147_483_648 }] }],
    ['the same project twice', { ...valid, content: [modrinth, { ...modrinth, versionId: 'AAAAAAAA' }] }],
  ];
  for (const [title, body] of rejected) {
    test(`rejects ${title}`, () => {
      rejects(body);
    });
  }

  test('the same project as two kinds is allowed', () => {
    const content = [modrinth, { ...modrinth, kind: 'datapack', versionId: 'AAAAAAAA' }];
    assert.equal(parseManifest({ ...valid, content }).content.length, 2);
  });
});
