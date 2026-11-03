import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { ErrorBody, Player, TestServer } from './helpers.ts';

const DAY = 86_400;
const TTL_DAYS = 90;

interface CreatedBody {
  code: string;
  createdAt: string;
  expiresAt: string;
}

interface FetchedBody extends CreatedBody {
  manifest: unknown;
}

interface ListBody {
  codes: Array<{ code: string; name: string; minecraftVersion: string; loader: unknown; createdAt: string; expiresAt: string }>;
}

const manifest = {
  name: 'Survival 1.21',
  minecraftVersion: '1.21.4',
  loader: { type: 'fabric', version: '0.16.10' },
  content: [
    { kind: 'mod', platform: 'modrinth', projectId: 'AANobbMI', versionId: 'IZskON6d' },
    { kind: 'shaderpack', platform: 'curseforge', projectId: 238_222, fileId: 5_432_100 },
  ],
};

describe('share codes', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  const create = (player: Player, body: unknown = manifest) =>
    s.api<CreatedBody & ErrorBody>('POST', '/v1/share-codes', { token: player.token, body });
  const fetchCode = (player: Player, code: string) =>
    s.api<FetchedBody & ErrorBody>('GET', `/v1/share-codes/${encodeURIComponent(code)}`, { token: player.token });
  const listCodes = async (player: Player) => (await s.api<ListBody>('GET', '/v1/share-codes', { token: player.token })).body.codes;

  test('creating one is 201 with a KITE-XXXX-XXXX code', async () => {
    const owner = await s.newPlayer();
    const res = await create(owner);
    assert.equal(res.status, 201);
    assert.match(res.body.code, /^KITE-[2-9A-HJKMNP-Z]{4}-[2-9A-HJKMNP-Z]{4}$/);
    assert.equal(res.body.createdAt, s.clock.iso());
    assert.equal(res.body.expiresAt, s.clock.iso(TTL_DAYS * DAY));
  });

  test('any signed-in player fetches the manifest by code', async () => {
    const owner = await s.newPlayer();
    const other = await s.newPlayer();
    const { code } = (await create(owner)).body;
    const res = await fetchCode(other, code);
    assert.equal(res.status, 200);
    assert.deepEqual(res.body.manifest, manifest);
    assert.equal(res.body.code, code);
    assert.equal('owner' in res.body, false);
  });

  test('the lookup ignores case, dashes and the prefix', async () => {
    const owner = await s.newPlayer();
    const { code } = (await create(owner)).body;
    const bare = code.slice(5).replace('-', '');
    for (const typed of [code.toLowerCase(), bare, bare.toLowerCase(), `kite${bare}`, code.slice(5)]) {
      const res = await fetchCode(owner, typed);
      assert.equal(res.status, 200, typed);
      assert.equal(res.body.code, code);
    }
  });

  test('a code that is not one is 400 and an unknown code is 404', async () => {
    const player = await s.newPlayer();
    for (const bad of ['KITE-0000-0000', 'KITE-ABCD', 'hello', 'KITE-ABCD-EFG!']) {
      const res = await fetchCode(player, bad);
      assert.equal(res.status, 400, bad);
      assert.equal(res.body.error, 'bad_request');
    }
    const unknown = await fetchCode(player, 'KITE-2345-6789');
    assert.equal(unknown.status, 404);
    assert.equal(unknown.body.error, 'not_found');
  });

  test('extra fields are refused and nothing extra is stored', async () => {
    const owner = await s.newPlayer();
    const withUrl = { ...manifest, content: [{ ...manifest.content[0], url: 'https://evil.example/x.jar' }] };
    const res = await create(owner, withUrl);
    assert.equal(res.status, 400);
    assert.equal(res.body.error, 'bad_request');
    assert.deepEqual(await listCodes(owner), []);
  });

  const rejected: Array<[string, unknown]> = [
    ['a bad loader', { ...manifest, loader: { type: 'optifine', version: '1' } }],
    ['a top-level URL', { ...manifest, downloadUrl: 'https://evil.example/pack.zip' }],
    ['a CurseForge id as a string', { ...manifest, content: [{ ...manifest.content[1], fileId: '5432100' }] }],
    [
      '501 entries',
      {
        ...manifest,
        content: Array.from({ length: 501 }, (_, i) => ({ kind: 'mod', platform: 'curseforge', projectId: i + 1, fileId: 1 })),
      },
    ],
    ['an array', [manifest]],
  ];
  for (const [title, body] of rejected) {
    test(`a manifest with ${title} is 400`, async () => {
      const owner = await s.newPlayer();
      const res = await create(owner, body);
      assert.equal(res.status, 400);
      assert.equal(res.body.error, 'bad_request');
    });
  }

  test('500 entries fit, a body over 64 KB is 413', async () => {
    const owner = await s.newPlayer();
    const content = Array.from({ length: 500 }, (_, i) => ({
      kind: 'resourcepack',
      platform: 'modrinth',
      projectId: `P${String(i).padStart(15, '0')}`,
      versionId: 'V'.repeat(16),
    }));
    const big = { ...manifest, loader: { type: 'vanilla' }, content };
    assert.ok(JSON.stringify(big).length > 8 * 1024);
    assert.equal((await create(owner, big)).status, 201);

    const oversize = { ...manifest, name: 'x'.repeat(70 * 1024) };
    const res = await create(owner, oversize);
    assert.equal(res.status, 413);
  });

  test('other routes keep the 8 KB limit', async () => {
    const player = await s.newPlayer();
    const res = await s.api('PATCH', '/v1/me/settings', { token: player.token, body: { pad: 'x'.repeat(9 * 1024) } });
    assert.equal(res.status, 413);
  });

  test('a code expires 90 days after it was last fetched', async () => {
    const owner = await s.newPlayer();
    const { code } = (await create(owner)).body;
    // Sessions last 30 days, so each jump signs in again.
    const later = async (seconds: number) => {
      s.clock.advance(seconds);
      return s.newPlayer();
    };
    const fetched = await fetchCode(await later(60 * DAY), code);
    assert.equal(fetched.status, 200);
    assert.equal(fetched.body.expiresAt, s.clock.iso(TTL_DAYS * DAY));
    assert.equal((await fetchCode(await later(TTL_DAYS * DAY - 1), code)).status, 200);
    assert.equal((await fetchCode(await later(TTL_DAYS * DAY), code)).status, 404);
    const again = { ...owner, token: await s.signIn(owner.name, owner.uuid) };
    assert.deepEqual(await listCodes(again), []);
  });

  test('a player lists only their own live codes, newest first', async () => {
    const owner = await s.newPlayer();
    const other = await s.newPlayer();
    const first = (await create(owner)).body;
    s.clock.advance(1);
    const second = (await create(owner, { ...manifest, name: 'Second', loader: { type: 'vanilla' }, content: [] })).body;
    await create(other);
    const codes = await listCodes(owner);
    assert.deepEqual(
      codes.map((c) => c.code),
      [second.code, first.code],
    );
    assert.deepEqual(codes[0], {
      code: second.code,
      name: 'Second',
      minecraftVersion: '1.21.4',
      loader: { type: 'vanilla' },
      createdAt: second.createdAt,
      expiresAt: second.expiresAt,
    });
  });

  test('only the owner can delete a code', async () => {
    const owner = await s.newPlayer();
    const other = await s.newPlayer();
    const { code } = (await create(owner)).body;
    const theirs = await s.api<ErrorBody>('DELETE', `/v1/share-codes/${code}`, { token: other.token });
    assert.equal(theirs.status, 404);
    assert.equal((await fetchCode(other, code)).status, 200);

    const mine = await s.api('DELETE', `/v1/share-codes/${code.toLowerCase()}`, { token: owner.token });
    assert.equal(mine.status, 204);
    assert.equal((await fetchCode(other, code)).status, 404);
    assert.equal((await s.api('DELETE', `/v1/share-codes/${code}`, { token: owner.token })).status, 404);
  });

  test('a player can keep 50 codes live', async () => {
    const owner = await s.newPlayer();
    for (let i = 0; i < 50; i += 1) assert.equal((await create(owner)).status, 201, `code ${i + 1}`);
    const res = await create(owner);
    assert.equal(res.status, 409);
    assert.equal(res.body.error, 'limit_reached');
  });

  test('deleting the account deletes its codes', async () => {
    const owner = await s.newPlayer();
    const other = await s.newPlayer();
    const { code } = (await create(owner)).body;
    assert.equal((await s.api('DELETE', '/v1/me', { token: owner.token })).status, 204);
    assert.equal((await fetchCode(other, code)).status, 404);
  });

  test('every endpoint needs a token', async () => {
    assert.equal((await s.api('POST', '/v1/share-codes', { body: manifest })).status, 401);
    assert.equal((await s.api('GET', '/v1/share-codes')).status, 401);
    assert.equal((await s.api('GET', '/v1/share-codes/KITE-2345-6789')).status, 401);
    assert.equal((await s.api('DELETE', '/v1/share-codes/KITE-2345-6789')).status, 401);
  });
});
