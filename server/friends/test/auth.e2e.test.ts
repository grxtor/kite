import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { JEB, NOTCH, makeUuid, startServer } from './helpers.ts';
import type { ChallengeBody, ErrorBody, LoginBody, MeBody, TestServer } from './helpers.ts';

const DAY = 86_400;

describe('auth', () => {
  let s: TestServer;
  let counter = 0;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  function nextIdentity(): { name: string; uuid: string } {
    counter += 1;
    return { name: `Auth${counter}`, uuid: makeUuid(5000 + counter) };
  }

  async function challenge(name: string): Promise<ChallengeBody> {
    const res = await s.api<ChallengeBody>('POST', '/v1/auth/challenge', { body: { name } });
    assert.equal(res.status, 200);
    return res.body;
  }

  describe('sign in', () => {
    test('returns a token with the canonical name and uuid from Mojang', async () => {
      const started = s.clock.iso();
      const ch = await challenge('notch');
      assert.match(ch.challengeId, /^[0-9a-f-]{36}$/);
      assert.ok(ch.serverId.length >= 16);
      assert.equal(ch.expiresAt, s.clock.iso(120));
      assert.ok(Date.parse(ch.expiresAt) > Date.parse(started));

      s.mojang.join(NOTCH.name, NOTCH.uuid, ch.serverId);
      const login = await s.api<LoginBody>('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
      assert.equal(login.status, 200);
      assert.match(login.body.token, /^kfs_/);
      assert.equal(login.body.expiresAt, s.clock.iso(30 * DAY));
      assert.deepEqual(login.body.user, { uuid: NOTCH.uuid, name: 'Notch' });
    });

    test('asks Mojang with the username and serverId of the challenge', async () => {
      const id = nextIdentity();
      const ch = await challenge(id.name);
      s.mojang.join(id.name, id.uuid, ch.serverId);
      s.mojang.requests.length = 0;
      await s.api('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
      assert.equal(s.mojang.requests.length, 1);
      const request = s.mojang.requests[0];
      assert.equal(request?.username, id.name);
      assert.equal(request?.serverId, ch.serverId);
    });

    test('the token works for /v1/me', async () => {
      const id = nextIdentity();
      const token = await s.signIn(id.name, id.uuid);
      const me = await s.api<MeBody>('GET', '/v1/me', { token });
      assert.equal(me.status, 200);
      assert.equal(me.body.name, id.name);
      assert.equal(me.body.uuid, id.uuid);
    });

    test('every challenge gets its own id and serverId', async () => {
      const a = await challenge('Notch');
      const b = await challenge('Notch');
      assert.notEqual(a.challengeId, b.challengeId);
      assert.notEqual(a.serverId, b.serverId);
    });

    test('login before join is 401 not_joined and the challenge still works after joining', async () => {
      const id = nextIdentity();
      const ch = await challenge(id.name);
      const early = await s.api<ErrorBody>('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
      assert.equal(early.status, 401);
      assert.equal(early.body.error, 'not_joined');

      s.mojang.join(id.name, id.uuid, ch.serverId);
      const late = await s.api<LoginBody>('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
      assert.equal(late.status, 200);
      assert.equal(late.body.user.uuid, id.uuid);
    });

    test('a challenge allows five login attempts', async () => {
      const id = nextIdentity();
      const ch = await challenge(id.name);
      for (let attempt = 0; attempt < 5; attempt += 1) {
        const res = await s.api<ErrorBody>('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
        assert.equal(res.body.error, 'not_joined');
      }
      s.mojang.join(id.name, id.uuid, ch.serverId);
      const sixth = await s.api<ErrorBody>('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
      assert.equal(sixth.status, 400);
      assert.equal(sixth.body.error, 'challenge_invalid');
      assert.equal(s.mojang.requests.filter((r) => r.serverId === ch.serverId).length, 5);
    });

    test('a challenge works for one login only', async () => {
      const id = nextIdentity();
      const ch = await challenge(id.name);
      s.mojang.join(id.name, id.uuid, ch.serverId);
      const first = await s.api('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
      assert.equal(first.status, 200);
      const second = await s.api<ErrorBody>('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
      assert.equal(second.status, 400);
      assert.equal(second.body.error, 'challenge_invalid');
    });

    test('an unknown challenge is challenge_invalid', async () => {
      const res = await s.api<ErrorBody>('POST', '/v1/auth/login', { body: { challengeId: makeUuid(999_999) } });
      assert.equal(res.status, 400);
      assert.equal(res.body.error, 'challenge_invalid');
    });

    test('a missing challengeId is 400', async () => {
      const res = await s.api<ErrorBody>('POST', '/v1/auth/login', { body: {} });
      assert.equal(res.status, 400);
    });

    test('an expired challenge is challenge_invalid even after joining', async () => {
      const id = nextIdentity();
      const ch = await challenge(id.name);
      s.mojang.join(id.name, id.uuid, ch.serverId);
      s.clock.advance(121);
      const res = await s.api<ErrorBody>('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
      assert.equal(res.status, 400);
      assert.equal(res.body.error, 'challenge_invalid');
    });

    test('a challenge still works just before it expires', async () => {
      const id = nextIdentity();
      const ch = await challenge(id.name);
      s.mojang.join(id.name, id.uuid, ch.serverId);
      s.clock.advance(119);
      const res = await s.api('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
      assert.equal(res.status, 200);
    });

    test('a Mojang failure is 502 mojang_unavailable', async () => {
      const id = nextIdentity();
      const ch = await challenge(id.name);
      s.mojang.join(id.name, id.uuid, ch.serverId);
      s.mojang.setFailing(true);
      try {
        const res = await s.api<ErrorBody>('POST', '/v1/auth/login', { body: { challengeId: ch.challengeId } });
        assert.equal(res.status, 502);
        assert.equal(res.body.error, 'mojang_unavailable');
      } finally {
        s.mojang.setFailing(false);
      }
    });
  });

  describe('challenge validation', () => {
    const invalidNames: Array<[string, unknown]> = [
      ['a space in it', 'a b'],
      ['17 characters', 'a'.repeat(17)],
      ['empty', ''],
      ['a dash', 'bad-name'],
      ['a non-ASCII letter', 'Bjørn'],
    ];
    for (const [title, name] of invalidNames) {
      test(`rejects a name that is ${title}`, async () => {
        const res = await s.api<ErrorBody>('POST', '/v1/auth/challenge', { body: { name } });
        assert.equal(res.status, 400);
        assert.equal(res.body.error, 'bad_request');
      });
    }

    test('rejects a missing name', async () => {
      const res = await s.api<ErrorBody>('POST', '/v1/auth/challenge', { body: {} });
      assert.equal(res.status, 400);
      assert.equal(res.body.error, 'bad_request');
    });

    test('accepts 16 characters and underscores', async () => {
      const res = await s.api('POST', '/v1/auth/challenge', { body: { name: 'a_b_c_d_e_f_g_h_' } });
      assert.equal(res.status, 200);
    });
  });

  describe('tokens', () => {
    test('a request without a token is 401 unauthorized', async () => {
      const res = await s.api<ErrorBody>('GET', '/v1/me');
      assert.equal(res.status, 401);
      assert.equal(res.body.error, 'unauthorized');
    });

    test('an unknown token is 401 unauthorized', async () => {
      const res = await s.api<ErrorBody>('GET', '/v1/friends', { token: 'kfs_doesnotexist' });
      assert.equal(res.status, 401);
      assert.equal(res.body.error, 'unauthorized');
    });

    test('a header that is not a bearer token is 401 unauthorized', async () => {
      const id = nextIdentity();
      const token = await s.signIn(id.name, id.uuid);
      const res = await s.api<ErrorBody>('GET', '/v1/me', { headers: { authorization: token } });
      assert.equal(res.status, 401);
      assert.equal(res.body.error, 'unauthorized');
    });

    test('logout is 204 and revokes only that token', async () => {
      const id = nextIdentity();
      const first = await s.signIn(id.name, id.uuid);
      const second = await s.signIn(id.name, id.uuid);
      const out = await s.api('POST', '/v1/auth/logout', { token: first });
      assert.equal(out.status, 204);

      const revoked = await s.api<ErrorBody>('GET', '/v1/me', { token: first });
      assert.equal(revoked.status, 401);
      assert.equal(revoked.body.error, 'unauthorized');
      const other = await s.api('GET', '/v1/me', { token: second });
      assert.equal(other.status, 200);
    });

    test('logout without a token is 401', async () => {
      const res = await s.api<ErrorBody>('POST', '/v1/auth/logout');
      assert.equal(res.status, 401);
      assert.equal(res.body.error, 'unauthorized');
    });

    test('a token expires after 30 days', async () => {
      const id = nextIdentity();
      const token = await s.signIn(id.name, id.uuid);
      s.clock.advance(29 * DAY);
      assert.equal((await s.api('GET', '/v1/me', { token })).status, 200);
      s.clock.advance(DAY + 1);
      const res = await s.api<ErrorBody>('GET', '/v1/me', { token });
      assert.equal(res.status, 401);
      assert.equal(res.body.error, 'unauthorized');
    });

    test('an eleventh login drops the oldest token', async () => {
      const id = nextIdentity();
      const tokens: string[] = [];
      for (let i = 0; i < 11; i += 1) {
        tokens.push(await s.signIn(id.name, id.uuid));
        s.clock.advance(1);
      }
      const statuses = await Promise.all(tokens.map(async (token) => (await s.api('GET', '/v1/me', { token })).status));
      assert.deepEqual(statuses, [401, 200, 200, 200, 200, 200, 200, 200, 200, 200, 200]);
    });

    test('ten logins keep all tokens', async () => {
      const id = nextIdentity();
      const tokens: string[] = [];
      for (let i = 0; i < 10; i += 1) tokens.push(await s.signIn(id.name, id.uuid));
      const statuses = await Promise.all(tokens.map(async (token) => (await s.api('GET', '/v1/me', { token })).status));
      assert.deepEqual(statuses, Array(10).fill(200));
    });
  });

  describe('renamed accounts', () => {
    test('signing in again with a new name updates the name friends see', async () => {
      const uuid = makeUuid(777_001);
      const old = await s.newPlayer('OldName', uuid);
      const friend = await s.newPlayer();
      await s.makeFriends(old, friend);
      assert.equal((await s.friendsOf(friend)).friends[0]?.name, 'OldName');

      const renamedToken = await s.signIn('NewName', uuid);
      const me = await s.api<MeBody>('GET', '/v1/me', { token: renamedToken });
      assert.equal(me.body.name, 'NewName');
      assert.equal(me.body.uuid, uuid);

      const friends = await s.friendsOf(friend);
      assert.equal(friends.friends.length, 1);
      assert.deepEqual(
        { uuid: friends.friends[0]?.uuid, name: friends.friends[0]?.name },
        { uuid, name: 'NewName' },
      );
    });

    test('the same account keeps its friends and settings when signing in again', async () => {
      const a = await s.newPlayer(JEB.name, JEB.uuid);
      const b = await s.newPlayer();
      await s.makeFriends(a, b);
      await s.api('PATCH', '/v1/me/settings', { token: a.token, body: { visibility: 'everyone' } });

      const again = await s.signIn(JEB.name, JEB.uuid);
      const me = await s.api<MeBody>('GET', '/v1/me', { token: again });
      assert.equal(me.body.settings.visibility, 'everyone');
      assert.equal((await s.friendsOf({ ...a, token: again })).friends.length, 1);
    });
  });
});
