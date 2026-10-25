import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { Player, StatusBody, TestServer } from './helpers.ts';

describe('joining a friend', () => {
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

  async function play(reporter: Player, world?: unknown): Promise<StatusBody> {
    const body = { state: 'playing', instance: 'Survival', version: '1.21.4', ...(world === undefined ? {} : { world }) };
    const res = await s.api<{ status: StatusBody }>('PUT', '/v1/me/status', { token: reporter.token, body });
    assert.equal(res.status, 200);
    return res.body.status;
  }

  async function setSettings(player: Player, body: unknown): Promise<void> {
    const res = await s.api('PATCH', '/v1/me/settings', { token: player.token, body });
    assert.equal(res.status, 200);
  }

  const hypixel = { type: 'server', host: 'mc.hypixel.net', port: 25565 };

  test('a friend on a shared server is joinable by default', async () => {
    const { reporter, viewer } = await pair();
    assert.equal((await play(reporter, hypixel)).joinable, true);
    assert.equal((await seen(viewer, reporter)).joinable, true);
  });

  test('allowJoin off makes them not joinable, at once and back again', async () => {
    const { reporter, viewer } = await pair();
    await play(reporter, hypixel);
    await setSettings(reporter, { allowJoin: false });
    const status = await seen(viewer, reporter);
    assert.equal(status.joinable, false);
    assert.deepEqual(status.world, hypixel);
    await setSettings(reporter, { allowJoin: true });
    assert.equal((await seen(viewer, reporter)).joinable, true);
  });

  test('a report while allowJoin is off says not joinable', async () => {
    const { reporter } = await pair();
    await setSettings(reporter, { allowJoin: false });
    assert.equal((await play(reporter, hypixel)).joinable, false);
  });

  test('hideServer makes them not joinable', async () => {
    const { reporter, viewer } = await pair();
    await setSettings(reporter, { hideServer: true });
    assert.equal((await play(reporter, hypixel)).joinable, false);
    assert.equal((await seen(viewer, reporter)).joinable, false);
  });

  test('an address that is not shared is not joinable until shareIpServers is on', async () => {
    const { reporter, viewer } = await pair();
    await setSettings(reporter, { shareIpServers: true });
    await play(reporter, { type: 'server', host: '192.168.1.20', port: 25565 });
    assert.equal((await seen(viewer, reporter)).joinable, true);
    await setSettings(reporter, { shareIpServers: false });
    assert.equal((await seen(viewer, reporter)).joinable, false);
  });

  test('single-player and the menus are not joinable', async () => {
    const { reporter, viewer } = await pair();
    await play(reporter, { type: 'singleplayer' });
    assert.equal((await seen(viewer, reporter)).joinable, false);
    await play(reporter);
    assert.equal((await seen(viewer, reporter)).joinable, false);
  });

  test('a lookup shows joinable the same way', async () => {
    const reporter = await s.newPlayer();
    const stranger = await s.newPlayer();
    await setSettings(reporter, { visibility: 'everyone' });
    await play(reporter, hypixel);
    const res = await s.api<{ status: StatusBody }>('GET', `/v1/players/${reporter.name}/status`, {
      token: stranger.token,
    });
    assert.equal(res.status, 200);
    assert.equal(res.body.status.joinable, true);
  });

  test('online and offline friends carry no joinable', async () => {
    const { reporter, viewer } = await pair();
    assert.equal('joinable' in (await seen(viewer, reporter)), false);
    await s.api('PUT', '/v1/me/status', { token: reporter.token, body: { state: 'online' } });
    assert.deepEqual(await seen(viewer, reporter), { state: 'online' });
  });
});
