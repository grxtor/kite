import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { makeUuid, startServer } from './helpers.ts';
import type { ErrorBody, TestServer, UserRef } from './helpers.ts';

interface RequestResult {
  status: string;
  user: UserRef;
}

describe('friends', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  const unknownUuid = makeUuid(424_242);

  describe('sending requests', () => {
    test('a request by name is 201 pending and shows in both lists', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      s.clock.advance(5);
      const res = await s.api<RequestResult>('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      assert.equal(res.status, 201);
      assert.deepEqual(res.body, { status: 'pending', user: { uuid: b.uuid, name: b.name } });

      const mine = await s.friendsOf(a);
      assert.deepEqual(mine.requests.incoming, []);
      assert.deepEqual(mine.requests.outgoing, [{ uuid: b.uuid, name: b.name, createdAt: s.clock.iso() }]);
      const theirs = await s.friendsOf(b);
      assert.deepEqual(theirs.requests.outgoing, []);
      assert.deepEqual(theirs.requests.incoming, [{ uuid: a.uuid, name: a.name, createdAt: s.clock.iso() }]);
      assert.deepEqual(mine.friends, []);
    });

    test('the name is matched case-insensitively and answered with the real name', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer('MixedCaseName');
      const res = await s.api<RequestResult>('POST', '/v1/friends/requests', {
        token: a.token,
        body: { name: 'mixedcasename' },
      });
      assert.equal(res.status, 201);
      assert.deepEqual(res.body.user, { uuid: b.uuid, name: 'MixedCaseName' });
    });

    test('repeating a request is 200 pending and changes nothing', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      const initial = await s.friendsOf(a);
      s.clock.advance(60);
      const again = await s.api<RequestResult>('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      assert.equal(again.status, 200);
      assert.equal(again.body.status, 'pending');
      assert.deepEqual((await s.friendsOf(a)).requests, initial.requests);
      assert.equal((await s.friendsOf(b)).requests.incoming.length, 1);
    });

    test('a mutual request is 200 accepted and makes them friends', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      const res = await s.api<RequestResult>('POST', '/v1/friends/requests', { token: b.token, body: { name: a.name } });
      assert.equal(res.status, 200);
      assert.deepEqual(res.body, { status: 'accepted', user: { uuid: a.uuid, name: a.name } });

      for (const [me, other] of [[a, b], [b, a]] as const) {
        const list = await s.friendsOf(me);
        assert.deepEqual(list.friends.map((f) => f.uuid), [other.uuid]);
        assert.deepEqual(list.requests, { incoming: [], outgoing: [] });
      }
    });

    test('a request to yourself is 400', async () => {
      const a = await s.newPlayer();
      const res = await s.api<ErrorBody>('POST', '/v1/friends/requests', { token: a.token, body: { name: a.name.toUpperCase() } });
      assert.equal(res.status, 400);
      assert.equal(res.body.error, 'bad_request');
    });

    test('an unknown name is 404 not_found', async () => {
      const a = await s.newPlayer();
      const res = await s.api<ErrorBody>('POST', '/v1/friends/requests', { token: a.token, body: { name: 'NobodyHere' } });
      assert.equal(res.status, 404);
      assert.equal(res.body.error, 'not_found');
    });

    test('a request to a friend is 409 already_friends', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.makeFriends(a, b);
      for (const [from, to] of [[a, b], [b, a]] as const) {
        const res = await s.api<ErrorBody>('POST', '/v1/friends/requests', { token: from.token, body: { name: to.name } });
        assert.equal(res.status, 409);
        assert.equal(res.body.error, 'already_friends');
      }
    });

    test('a missing or malformed name is 400', async () => {
      const a = await s.newPlayer();
      for (const body of [{}, { name: '' }, { name: 'a b' }]) {
        const res = await s.api<ErrorBody>('POST', '/v1/friends/requests', { token: a.token, body });
        assert.equal(res.status, 400);
        assert.equal(res.body.error, 'bad_request');
      }
    });

    test('needs a token', async () => {
      const res = await s.api<ErrorBody>('POST', '/v1/friends/requests', { body: { name: 'Notch' } });
      assert.equal(res.status, 401);
      assert.equal(res.body.error, 'unauthorized');
    });

    test('the 101st pending outgoing request is 409 limit_reached', async () => {
      const a = await s.newPlayer();
      const targets = [];
      for (let i = 0; i < 101; i += 1) targets.push(await s.newPlayer());
      for (const target of targets.slice(0, 100)) {
        const res = await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: target.name } });
        assert.equal(res.status, 201);
      }
      const last = targets[100];
      assert.ok(last);
      const res = await s.api<ErrorBody>('POST', '/v1/friends/requests', { token: a.token, body: { name: last.name } });
      assert.equal(res.status, 409);
      assert.equal(res.body.error, 'limit_reached');
    });
  });

  describe('answering requests', () => {
    test('accept makes both sides friends with a since time', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      s.clock.advance(90);
      const res = await s.api<{ user: UserRef }>('POST', `/v1/friends/requests/${a.uuid}/accept`, { token: b.token });
      assert.equal(res.status, 200);
      assert.deepEqual(res.body, { user: { uuid: a.uuid, name: a.name } });

      const mine = await s.friendsOf(a);
      const theirs = await s.friendsOf(b);
      assert.deepEqual(mine.friends.map((f) => [f.uuid, f.name, f.since]), [[b.uuid, b.name, s.clock.iso()]]);
      assert.deepEqual(theirs.friends.map((f) => [f.uuid, f.name, f.since]), [[a.uuid, a.name, s.clock.iso()]]);
      assert.deepEqual(mine.requests, { incoming: [], outgoing: [] });
      assert.deepEqual(theirs.requests, { incoming: [], outgoing: [] });
    });

    test('friends are sorted by name', async () => {
      const me = await s.newPlayer();
      const zed = await s.newPlayer('Sorted_Zed');
      const amy = await s.newPlayer('Sorted_Amy');
      const kim = await s.newPlayer('Sorted_Kim');
      await s.makeFriends(zed, me);
      await s.makeFriends(amy, me);
      await s.makeFriends(kim, me);
      const names = (await s.friendsOf(me)).friends.map((f) => f.name);
      assert.deepEqual(names, ['Sorted_Amy', 'Sorted_Kim', 'Sorted_Zed']);
    });

    test('only the receiver can accept', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      const res = await s.api<ErrorBody>('POST', `/v1/friends/requests/${b.uuid}/accept`, { token: a.token });
      assert.equal(res.status, 404);
      assert.equal(res.body.error, 'not_found');
    });

    test('decline is 204, the request disappears for both and the sender is not told', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      const res = await s.api('POST', `/v1/friends/requests/${a.uuid}/decline`, { token: b.token });
      assert.equal(res.status, 204);

      assert.deepEqual((await s.friendsOf(a)).requests.outgoing, []);
      assert.deepEqual((await s.friendsOf(b)).requests.incoming, []);
      assert.deepEqual((await s.friendsOf(a)).friends, []);
    });

    test('cancel is 204 and removes the request from the receiver', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      const res = await s.api('DELETE', `/v1/friends/requests/${b.uuid}`, { token: a.token });
      assert.equal(res.status, 204);
      assert.deepEqual((await s.friendsOf(a)).requests.outgoing, []);
      assert.deepEqual((await s.friendsOf(b)).requests.incoming, []);

      const again = await s.api<ErrorBody>('DELETE', `/v1/friends/requests/${b.uuid}`, { token: a.token });
      assert.equal(again.status, 404);
    });

    test('remove friend is 204 on both sides and then 404', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.makeFriends(a, b);
      const res = await s.api('DELETE', `/v1/friends/${b.uuid}`, { token: a.token });
      assert.equal(res.status, 204);
      assert.deepEqual((await s.friendsOf(a)).friends, []);
      assert.deepEqual((await s.friendsOf(b)).friends, []);

      const again = await s.api<ErrorBody>('DELETE', `/v1/friends/${b.uuid}`, { token: a.token });
      assert.equal(again.status, 404);
      assert.equal(again.body.error, 'not_found');
    });

    test('the other side can remove the friend too', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.makeFriends(a, b);
      const res = await s.api('DELETE', `/v1/friends/${a.uuid}`, { token: b.token });
      assert.equal(res.status, 204);
      assert.deepEqual((await s.friendsOf(a)).friends, []);
    });
  });

  describe('things that do not exist', () => {
    const calls: Array<[string, string, (uuid: string) => string]> = [
      ['accept', 'POST', (uuid) => `/v1/friends/requests/${uuid}/accept`],
      ['decline', 'POST', (uuid) => `/v1/friends/requests/${uuid}/decline`],
      ['cancel', 'DELETE', (uuid) => `/v1/friends/requests/${uuid}`],
      ['remove friend', 'DELETE', (uuid) => `/v1/friends/${uuid}`],
    ];
    for (const [title, method, path] of calls) {
      test(`${title} with an unknown player is 404 not_found`, async () => {
        const a = await s.newPlayer();
        const res = await s.api<ErrorBody>(method, path(unknownUuid), { token: a.token });
        assert.equal(res.status, 404);
        assert.equal(res.body.error, 'not_found');
      });

      test(`${title} with a player who has no request or friendship is 404`, async () => {
        const a = await s.newPlayer();
        const b = await s.newPlayer();
        const res = await s.api<ErrorBody>(method, path(b.uuid), { token: a.token });
        assert.equal(res.status, 404);
        assert.equal(res.body.error, 'not_found');
      });

      test(`${title} with a malformed uuid is 400 bad_request`, async () => {
        const a = await s.newPlayer();
        for (const bad of ['not-a-uuid', '12345', 'Notch']) {
          const res = await s.api<ErrorBody>(method, path(bad), { token: a.token });
          assert.equal(res.status, 400, bad);
          assert.equal(res.body.error, 'bad_request');
        }
      });
    }
  });
});
