import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { ErrorBody, Player, TestServer } from './helpers.ts';

describe('status validation', () => {
  let s: TestServer;
  let player: Player;

  before(async () => {
    s = await startServer();
    player = await s.newPlayer();
  });

  after(async () => {
    await s.close();
  });

  const playing = { state: 'playing', instance: 'Survival 1.21', version: '1.21.4' };
  const server = (host: unknown, port?: unknown) => ({ ...playing, world: { type: 'server', host, port } });

  const rejected: Array<[string, unknown]> = [
    ['an empty body', {}],
    ['an unknown state', { state: 'away' }],
    ['state offline', { state: 'offline' }],
    ['online with an instance', { state: 'online', instance: 'Survival' }],
    ['online with a version', { state: 'online', version: '1.21.4' }],
    ['online with a world', { state: 'online', world: { type: 'singleplayer' } }],
    ['playing without a version', { state: 'playing', instance: 'Survival 1.21' }],
    ['an empty instance', { ...playing, instance: '' }],
    ['a 65 character instance', { ...playing, instance: 'x'.repeat(65) }],
    ['an empty version', { ...playing, version: '' }],
    ['a 33 character version', { ...playing, version: '1'.repeat(33) }],
    ['a version with a bad character', { ...playing, version: '1.21!' }],
    ['an unknown world type', { ...playing, world: { type: 'realm' } }],
    ['a server world without a host', { ...playing, world: { type: 'server', port: 25565 } }],
    ['the host bad_host', server('bad_host')],
    ['the host -x.com', server('-x.com')],
    ['an empty host', server('')],
    ['a host with a port in it', server('example.com:25565')],
    ['a host that is not a string', server(1234)],
    ['port 0', server('mc.hypixel.net', 0)],
    ['port 70000', server('mc.hypixel.net', 70_000)],
    ['port 65536', server('mc.hypixel.net', 65_536)],
    ['a negative port', server('mc.hypixel.net', -1)],
    ['a fractional port', server('mc.hypixel.net', 25_565.5)],
  ];
  for (const [title, body] of rejected) {
    test(`rejects ${title}`, async () => {
      const res = await s.api<ErrorBody>('PUT', '/v1/me/status', { token: player.token, body });
      assert.equal(res.status, 400);
      assert.equal(res.body.error, 'bad_request');
    });
  }

  const accepted: Array<[string, unknown]> = [
    ['online', { state: 'online' }],
    ['a 64 character instance', { ...playing, instance: 'x'.repeat(64) }],
    ['a 32 character version', { ...playing, version: '1'.repeat(32) }],
    ['a version with every allowed character', { ...playing, version: '1.21_pre-1+fabric 2' }],
    ['port 1', server('mc.hypixel.net', 1)],
    ['port 65535', server('mc.hypixel.net', 65_535)],
    ['an IPv4 host', server('203.0.113.7')],
    ['an IPv6 host', server('2001:db8::1', 25_570)],
    ['a bracketed IPv6 host', server('[2001:db8::1]', 25_570)],
    ['a host with a trailing dot and capitals', server('MC.Hypixel.NET.')],
    ['a single-label host', server('survival')],
  ];
  for (const [title, body] of accepted) {
    test(`accepts ${title}`, async () => {
      const res = await s.api('PUT', '/v1/me/status', { token: player.token, body });
      assert.equal(res.status, 200);
    });
  }

  test('a rejected report leaves the old status alone', async () => {
    const friend = await s.newPlayer();
    await s.makeFriends(player, friend);
    await s.api('PUT', '/v1/me/status', { token: player.token, body: { state: 'online' } });
    const res = await s.api('PUT', '/v1/me/status', { token: player.token, body: { state: 'playing' } });
    assert.equal(res.status, 400);
    const seen = (await s.friendsOf(friend)).friends.find((f) => f.uuid === player.uuid);
    assert.deepEqual(seen?.status, { state: 'online' });
  });
});
