import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { Player, StatusBody, TestServer } from './helpers.ts';

interface PutResult {
  expiresAt: string;
  status: StatusBody;
}

describe('status', () => {
  let s: TestServer;

  before(async () => {
    s = await startServer();
  });

  after(async () => {
    await s.close();
  });

  async function pair(): Promise<{ reporter: Player; viewer: Player }> {
    const reporter = await s.newPlayer();
    const viewer = await s.newPlayer();
    await s.makeFriends(reporter, viewer);
    return { reporter, viewer };
  }

  async function seen(viewer: Player, reporter: Player): Promise<StatusBody> {
    const friend = (await s.friendsOf(viewer)).friends.find((f) => f.uuid === reporter.uuid);
    assert.ok(friend, 'reporter is in the friends list');
    return friend.status;
  }

  async function report(reporter: Player, body: unknown): Promise<PutResult> {
    const res = await s.api<PutResult>('PUT', '/v1/me/status', { token: reporter.token, body });
    assert.equal(res.status, 200);
    return res.body;
  }

  async function setSettings(player: Player, body: unknown): Promise<void> {
    const res = await s.api('PATCH', '/v1/me/settings', { token: player.token, body });
    assert.equal(res.status, 200);
  }

  const playing = (world?: unknown) => ({
    state: 'playing',
    instance: 'Survival 1.21',
    version: '1.21.4',
    ...(world === undefined ? {} : { world }),
  });

  describe('what friends see', () => {
    test('a friend who never reported is offline with no last seen time', async () => {
      const { reporter, viewer } = await pair();
      assert.deepEqual(await seen(viewer, reporter), { state: 'offline', lastSeenAt: null });
    });

    test('online', async () => {
      const { reporter, viewer } = await pair();
      const res = await report(reporter, { state: 'online' });
      assert.equal(res.expiresAt, s.clock.iso(180));
      assert.deepEqual(res.status, { state: 'online' });
      assert.deepEqual(await seen(viewer, reporter), { state: 'online' });
    });

    test('playing with instance, version and world', async () => {
      const { reporter, viewer } = await pair();
      const world = { type: 'server', host: 'mc.hypixel.net', port: 25565 };
      const res = await report(reporter, playing(world));
      assert.deepEqual(res.status, { state: 'playing', instance: 'Survival 1.21', version: '1.21.4', world });
      assert.deepEqual(await seen(viewer, reporter), res.status);
    });

    test('a domain server keeps its own port', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, playing({ type: 'server', host: 'play.example.org', port: 25570 }));
      const status = await seen(viewer, reporter);
      assert.deepEqual(status.world, { type: 'server', host: 'play.example.org', port: 25570 });
    });

    test('the port defaults to 25565', async () => {
      const { reporter, viewer } = await pair();
      const res = await report(reporter, playing({ type: 'server', host: 'mc.hypixel.net' }));
      assert.deepEqual(res.status.world, { type: 'server', host: 'mc.hypixel.net', port: 25565 });
      assert.deepEqual((await seen(viewer, reporter)).world, { type: 'server', host: 'mc.hypixel.net', port: 25565 });
    });

    test('single-player', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, playing({ type: 'singleplayer' }));
      assert.deepEqual((await seen(viewer, reporter)).world, { type: 'singleplayer' });
    });

    test('playing without a world shows world null', async () => {
      const { reporter, viewer } = await pair();
      const res = await report(reporter, playing());
      assert.deepEqual(res.status, { state: 'playing', instance: 'Survival 1.21', version: '1.21.4', world: null });
      assert.deepEqual(await seen(viewer, reporter), res.status);
    });

    test('playing with world null shows world null', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, playing(null));
      assert.equal((await seen(viewer, reporter)).world, null);
    });

    test('a new report replaces the old one', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, playing({ type: 'singleplayer' }));
      await report(reporter, { state: 'online' });
      assert.deepEqual(await seen(viewer, reporter), { state: 'online' });
    });
  });

  describe('addresses that are not shared', () => {
    const hosts = ['203.0.113.7', '192.168.1.20', 'localhost', 'myserver.local', 'survival', '2001:db8::1', '1.2.3.4.nip.io'];
    for (const host of hosts) {
      test(`${host} is shown as a plain server`, async () => {
        const { reporter, viewer } = await pair();
        const res = await report(reporter, playing({ type: 'server', host, port: 25570 }));
        assert.deepEqual(res.status.world, { type: 'server' });
        assert.deepEqual((await seen(viewer, reporter)).world, { type: 'server' });
      });
    }

    test('with shareIpServers they are shown in full', async () => {
      const { reporter, viewer } = await pair();
      await setSettings(reporter, { shareIpServers: true });
      for (const host of ['203.0.113.7', '192.168.1.20', 'localhost']) {
        const res = await report(reporter, playing({ type: 'server', host, port: 25570 }));
        assert.deepEqual(res.status.world, { type: 'server', host, port: 25570 });
        assert.deepEqual((await seen(viewer, reporter)).world, { type: 'server', host, port: 25570 });
      }
    });

    test('turning shareIpServers off hides an address already reported', async () => {
      const { reporter, viewer } = await pair();
      await setSettings(reporter, { shareIpServers: true });
      await report(reporter, playing({ type: 'server', host: '203.0.113.7', port: 25565 }));
      await setSettings(reporter, { shareIpServers: false });
      assert.deepEqual((await seen(viewer, reporter)).world, { type: 'server' });
    });
  });

  describe('hideServer', () => {
    test('hides the world and keeps the rest', async () => {
      const { reporter, viewer } = await pair();
      await setSettings(reporter, { hideServer: true });
      const res = await report(reporter, playing({ type: 'server', host: 'mc.hypixel.net', port: 25565 }));
      assert.deepEqual(res.status, { state: 'playing', instance: 'Survival 1.21', version: '1.21.4', world: null });
      assert.deepEqual(await seen(viewer, reporter), res.status);
    });

    test('hides single-player too', async () => {
      const { reporter, viewer } = await pair();
      await setSettings(reporter, { hideServer: true });
      await report(reporter, playing({ type: 'singleplayer' }));
      assert.equal((await seen(viewer, reporter)).world, null);
    });

    test('turning it on after reporting hides the world on the very next read', async () => {
      const { reporter, viewer } = await pair();
      const world = { type: 'server', host: 'mc.hypixel.net', port: 25565 };
      await report(reporter, playing(world));
      assert.deepEqual((await seen(viewer, reporter)).world, world);
      await setSettings(reporter, { hideServer: true });
      assert.equal((await seen(viewer, reporter)).world, null);
      await setSettings(reporter, { hideServer: false });
      assert.deepEqual((await seen(viewer, reporter)).world, world);
    });
  });

  describe('expiry and going offline', () => {
    test('a status expires after 180 seconds with the last report as last seen', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, { state: 'online' });
      const reportedAt = s.clock.iso();
      s.clock.advance(179);
      assert.deepEqual(await seen(viewer, reporter), { state: 'online' });
      s.clock.advance(2);
      assert.deepEqual(await seen(viewer, reporter), { state: 'offline', lastSeenAt: reportedAt });
    });

    test('reporting again keeps the status alive', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, { state: 'online' });
      s.clock.advance(120);
      await report(reporter, { state: 'online' });
      s.clock.advance(120);
      assert.deepEqual(await seen(viewer, reporter), { state: 'online' });
    });

    test('last seen follows the latest report', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, { state: 'online' });
      s.clock.advance(100);
      await report(reporter, playing());
      const lastReport = s.clock.iso();
      s.clock.advance(500);
      assert.deepEqual(await seen(viewer, reporter), { state: 'offline', lastSeenAt: lastReport });
    });

    test('DELETE /v1/me/status is 204 and goes offline at once', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, { state: 'online' });
      const res = await s.api('DELETE', '/v1/me/status', { token: reporter.token });
      assert.equal(res.status, 204);
      assert.equal((await seen(viewer, reporter)).state, 'offline');
    });

    test('DELETE /v1/me/status without a status is still 204', async () => {
      const reporter = await s.newPlayer();
      const res = await s.api('DELETE', '/v1/me/status', { token: reporter.token });
      assert.equal(res.status, 204);
    });
  });

  describe('invisible', () => {
    test('the PUT response and friends see offline with no last seen time', async () => {
      const { reporter, viewer } = await pair();
      await setSettings(reporter, { visibility: 'invisible' });
      const res = await report(reporter, playing({ type: 'singleplayer' }));
      assert.deepEqual(res.status, { state: 'offline', lastSeenAt: null });
      assert.deepEqual(await seen(viewer, reporter), { state: 'offline', lastSeenAt: null });
    });

    test('going invisible after reporting hides the status at once', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, { state: 'online' });
      await setSettings(reporter, { visibility: 'invisible' });
      assert.deepEqual(await seen(viewer, reporter), { state: 'offline', lastSeenAt: null });
    });

    test('nothing is stored while invisible', async () => {
      const { reporter, viewer } = await pair();
      await setSettings(reporter, { visibility: 'invisible' });
      await report(reporter, { state: 'online' });
      await setSettings(reporter, { visibility: 'friends' });
      assert.equal((await seen(viewer, reporter)).state, 'offline');
    });

    test('going invisible forgets the last seen time', async () => {
      const { reporter, viewer } = await pair();
      await report(reporter, { state: 'online' });
      await setSettings(reporter, { visibility: 'invisible' });
      await setSettings(reporter, { visibility: 'friends' });
      assert.deepEqual(await seen(viewer, reporter), { state: 'offline', lastSeenAt: null });
    });
  });

  test('reporting needs a token', async () => {
    const res = await s.api('PUT', '/v1/me/status', { body: { state: 'online' } });
    assert.equal(res.status, 401);
  });
});
