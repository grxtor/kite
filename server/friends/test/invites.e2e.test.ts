import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { makeUuid, startServer } from './helpers.ts';
import type { ErrorBody, InviteEntry, InvitesBody, Player, TestServer } from './helpers.ts';

const INVITE_TTL = 600;

describe('invites', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  const hypixel = { host: 'mc.hypixel.net', port: 25565 };

  async function friends(): Promise<{ a: Player; b: Player }> {
    const a = await s.newPlayer();
    const b = await s.newPlayer();
    await s.makeFriends(a, b);
    return { a, b };
  }

  function invite(from: Player, to: Player | string, body: unknown = hypixel) {
    const uuid = typeof to === 'string' ? to : to.uuid;
    return s.api<InviteEntry & ErrorBody>('PUT', `/v1/invites/${uuid}`, { token: from.token, body });
  }

  async function invitesOf(player: Player): Promise<InvitesBody> {
    return (await s.friendsOf(player)).invites;
  }

  async function setSettings(player: Player, body: unknown): Promise<void> {
    assert.equal((await s.api('PATCH', '/v1/me/settings', { token: player.token, body })).status, 200);
  }

  describe('sending', () => {
    test('an invite to a friend is 200 and shows on both sides of GET /v1/friends', async () => {
      const { a, b } = await friends();
      const res = await invite(a, b);
      assert.equal(res.status, 200);
      const expected = { ...hypixel, createdAt: s.clock.iso(), expiresAt: s.clock.iso(INVITE_TTL) };
      assert.deepEqual(res.body, { uuid: b.uuid, name: b.name, ...expected });
      assert.deepEqual(await invitesOf(a), { incoming: [], outgoing: [{ uuid: b.uuid, name: b.name, ...expected }] });
      assert.deepEqual(await invitesOf(b), { incoming: [{ uuid: a.uuid, name: a.name, ...expected }], outgoing: [] });
    });

    test('GET /v1/invites returns the same lists', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      for (const player of [a, b]) {
        const res = await s.api<InvitesBody>('GET', '/v1/invites', { token: player.token });
        assert.equal(res.status, 200);
        assert.deepEqual(res.body, await invitesOf(player));
      }
    });

    test('the port defaults to 25565 and the host is normalized', async () => {
      const { a, b } = await friends();
      const res = await invite(a, b, { host: 'Play.Example.COM.' });
      assert.equal(res.status, 200);
      assert.equal(res.body.host, 'play.example.com');
      assert.equal(res.body.port, 25565);
    });

    test('a new invite to the same friend replaces the old one', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      s.clock.advance(120);
      const res = await invite(a, b, { host: 'play.cubecraft.net', port: 25570 });
      assert.equal(res.status, 200);
      const incoming = (await invitesOf(b)).incoming;
      assert.equal(incoming.length, 1);
      assert.deepEqual(incoming[0], {
        uuid: a.uuid,
        name: a.name,
        host: 'play.cubecraft.net',
        port: 25570,
        createdAt: s.clock.iso(),
        expiresAt: s.clock.iso(INVITE_TTL),
      });
    });

    test('invites from several friends are listed newest first', async () => {
      const { a: me, b: first } = await friends();
      const second = await s.newPlayer();
      await s.makeFriends(me, second);
      await invite(first, me);
      s.clock.advance(1);
      await invite(second, me);
      assert.deepEqual(
        (await invitesOf(me)).incoming.map((i) => i.uuid),
        [second.uuid, first.uuid],
      );
    });

    test('both friends can invite each other at the same time', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      await invite(b, a);
      const mine = await invitesOf(a);
      assert.equal(mine.incoming.length, 1);
      assert.equal(mine.outgoing.length, 1);
    });
  });

  describe('refused', () => {
    test('to someone who is not a friend, with 404', async () => {
      const a = await s.newPlayer();
      const stranger = await s.newPlayer();
      const res = await invite(a, stranger);
      assert.equal(res.status, 404);
      assert.equal(res.body.error, 'not_found');
      assert.deepEqual((await invitesOf(stranger)).incoming, []);
    });

    test('to someone with only a pending request, with 404', async () => {
      const a = await s.newPlayer();
      const b = await s.newPlayer();
      await s.api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
      assert.equal((await invite(a, b)).status, 404);
      assert.equal((await invite(b, a)).status, 404);
    });

    test('to an unknown uuid, with 404', async () => {
      const a = await s.newPlayer();
      assert.equal((await invite(a, makeUuid(777_777))).status, 404);
    });

    test('to yourself, with 400', async () => {
      const a = await s.newPlayer();
      const res = await invite(a, a);
      assert.equal(res.status, 400);
      assert.equal(res.body.error, 'bad_request');
    });

    for (const direction of ['sender blocked recipient', 'recipient blocked sender'] as const) {
      test(`when the ${direction}, with 404`, async () => {
        const { a, b } = await friends();
        const [blocker, target] = direction === 'sender blocked recipient' ? [a, b] : [b, a];
        assert.equal((await s.api('PUT', `/v1/blocks/${target.uuid}`, { token: blocker.token })).status, 204);
        const res = await invite(a, b);
        assert.equal(res.status, 404);
        assert.equal(res.body.error, 'not_found');
        assert.deepEqual((await invitesOf(b)).incoming, []);
      });
    }

    for (const host of ['203.0.113.7', '192.168.1.20', 'localhost', 'nas.fritz.box', '1.2.3.4.nip.io', '[2001:db8::1]']) {
      test(`to the unshared address ${host}, with 409 address_not_shared`, async () => {
        const { a, b } = await friends();
        const res = await invite(a, b, { host, port: 25565 });
        assert.equal(res.status, 409);
        assert.equal(res.body.error, 'address_not_shared');
        assert.deepEqual((await invitesOf(b)).incoming, []);
      });
    }

    const malformed: Array<[string, unknown]> = [
      ['no host', { port: 25565 }],
      ['an empty host', { host: '' }],
      ['a host with a port in it', { host: 'mc.hypixel.net:25565' }],
      ['an invalid host', { host: 'bad_host' }],
      ['a numeric last label', { host: '2130706433' }],
      ['port 0', { host: 'mc.hypixel.net', port: 0 }],
      ['port 65536', { host: 'mc.hypixel.net', port: 65_536 }],
      ['a fractional port', { host: 'mc.hypixel.net', port: 25_565.5 }],
      ['a 256 character host', { host: `${'a'.repeat(252)}.com` }],
    ];
    for (const [title, body] of malformed) {
      test(`with ${title}, with 400`, async () => {
        const { a, b } = await friends();
        const res = await invite(a, b, body);
        assert.equal(res.status, 400);
        assert.equal(res.body.error, 'bad_request');
      });
    }

    test('when the recipient refuses invites, with 409 invites_off', async () => {
      const { a, b } = await friends();
      await setSettings(b, { allowInvites: false });
      const res = await invite(a, b);
      assert.equal(res.status, 409);
      assert.equal(res.body.error, 'invites_off');
      assert.deepEqual((await invitesOf(b)).incoming, []);
      await setSettings(b, { allowInvites: true });
      assert.equal((await invite(a, b)).status, 200);
    });

    test('a non-friend learns nothing about invite settings', async () => {
      const a = await s.newPlayer();
      const stranger = await s.newPlayer();
      await setSettings(stranger, { allowInvites: false });
      assert.equal((await invite(a, stranger)).status, 404);
    });
  });

  describe('raw IP and local addresses', () => {
    test('are allowed with shareIpServers on', async () => {
      const { a, b } = await friends();
      await setSettings(a, { shareIpServers: true });
      const res = await invite(a, b, { host: '192.168.1.20', port: 25566 });
      assert.equal(res.status, 200);
      assert.equal((await invitesOf(b)).incoming[0]?.host, '192.168.1.20');
    });

    test('only the sender’s setting counts', async () => {
      const { a, b } = await friends();
      await setSettings(b, { shareIpServers: true });
      assert.equal((await invite(a, b, { host: '203.0.113.7' })).status, 409);
    });

    test('turning shareIpServers off drops invites to them and keeps the rest', async () => {
      const { a, b } = await friends();
      const c = await s.newPlayer();
      await s.makeFriends(a, c);
      await setSettings(a, { shareIpServers: true });
      await invite(a, b, { host: '203.0.113.7' });
      await invite(a, c, hypixel);
      await setSettings(a, { shareIpServers: false });
      assert.deepEqual((await invitesOf(b)).incoming, []);
      assert.deepEqual(
        (await invitesOf(a)).outgoing.map((i) => i.host),
        ['mc.hypixel.net'],
      );
    });
  });

  describe('ending an invite', () => {
    test('it expires after ten minutes', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      s.clock.advance(INVITE_TTL - 1);
      assert.equal((await invitesOf(b)).incoming.length, 1);
      s.clock.advance(1);
      assert.deepEqual(await invitesOf(b), { incoming: [], outgoing: [] });
      assert.deepEqual(await invitesOf(a), { incoming: [], outgoing: [] });
    });

    test('an expired invite cannot be dismissed or cancelled', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      s.clock.advance(INVITE_TTL);
      assert.equal((await s.api('POST', `/v1/invites/${a.uuid}/dismiss`, { token: b.token })).status, 404);
      assert.equal((await s.api('DELETE', `/v1/invites/${b.uuid}`, { token: a.token })).status, 404);
    });

    test('the recipient dismisses it, 204, and it is gone for both', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      assert.equal((await s.api('POST', `/v1/invites/${a.uuid}/dismiss`, { token: b.token })).status, 204);
      assert.deepEqual((await invitesOf(b)).incoming, []);
      assert.deepEqual((await invitesOf(a)).outgoing, []);
      assert.equal((await s.api('POST', `/v1/invites/${a.uuid}/dismiss`, { token: b.token })).status, 404);
    });

    test('the sender cancels it, 204, and it is gone for both', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      assert.equal((await s.api('DELETE', `/v1/invites/${b.uuid}`, { token: a.token })).status, 204);
      assert.deepEqual((await invitesOf(b)).incoming, []);
      assert.deepEqual((await invitesOf(a)).outgoing, []);
    });

    test('only the recipient can dismiss and only the sender can cancel', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      assert.equal((await s.api('POST', `/v1/invites/${b.uuid}/dismiss`, { token: a.token })).status, 404);
      assert.equal((await s.api('DELETE', `/v1/invites/${a.uuid}`, { token: b.token })).status, 404);
      const outsider = await s.newPlayer();
      assert.equal((await s.api('POST', `/v1/invites/${a.uuid}/dismiss`, { token: outsider.token })).status, 404);
      assert.equal((await invitesOf(b)).incoming.length, 1);
    });

    for (const direction of ['sender blocks recipient', 'recipient blocks sender'] as const) {
      test(`a block (${direction}) deletes invites both ways`, async () => {
        const { a, b } = await friends();
        await invite(a, b);
        await invite(b, a);
        const [blocker, target] = direction === 'sender blocks recipient' ? [a, b] : [b, a];
        await s.api('PUT', `/v1/blocks/${target.uuid}`, { token: blocker.token });
        assert.deepEqual(await invitesOf(a), { incoming: [], outgoing: [] });
        assert.deepEqual(await invitesOf(b), { incoming: [], outgoing: [] });
      });
    }

    test('removing the friend deletes invites both ways', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      await invite(b, a);
      assert.equal((await s.api('DELETE', `/v1/friends/${b.uuid.toUpperCase()}`, { token: a.token })).status, 204);
      assert.deepEqual(await invitesOf(a), { incoming: [], outgoing: [] });
      assert.deepEqual(await invitesOf(b), { incoming: [], outgoing: [] });
      await s.makeFriends(a, b);
      assert.deepEqual(await invitesOf(b), { incoming: [], outgoing: [] });
    });

    test('turning allowInvites off drops invites already received, not the ones sent', async () => {
      const { a, b } = await friends();
      await invite(a, b);
      await invite(b, a);
      await setSettings(b, { allowInvites: false });
      const theirs = await invitesOf(b);
      assert.deepEqual(theirs.incoming, []);
      assert.equal(theirs.outgoing.length, 1);
    });

    test('deleting the account deletes invites both ways', async () => {
      const { a, b } = await friends();
      const c = await s.newPlayer();
      await s.makeFriends(a, c);
      await invite(a, b);
      await invite(c, a);
      assert.equal((await s.api('DELETE', '/v1/me', { token: a.token })).status, 204);
      assert.deepEqual(await invitesOf(b), { incoming: [], outgoing: [] });
      assert.deepEqual(await invitesOf(c), { incoming: [], outgoing: [] });
    });
  });

  test('every endpoint needs a token', async () => {
    const uuid = makeUuid(1);
    assert.equal((await s.api('GET', '/v1/invites')).status, 401);
    assert.equal((await s.api('PUT', `/v1/invites/${uuid}`, { body: hypixel })).status, 401);
    assert.equal((await s.api('DELETE', `/v1/invites/${uuid}`)).status, 401);
    assert.equal((await s.api('POST', `/v1/invites/${uuid}/dismiss`)).status, 401);
  });

  test('a malformed uuid is 400', async () => {
    const a = await s.newPlayer();
    assert.equal((await s.api('PUT', '/v1/invites/not-a-uuid', { token: a.token, body: hypixel })).status, 400);
    assert.equal((await s.api('POST', '/v1/invites/not-a-uuid/dismiss', { token: a.token })).status, 400);
  });
});
