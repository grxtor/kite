import assert from 'node:assert/strict';
import { describe, test } from 'node:test';
import { statusView, visibleWorld } from '../src/status.ts';
import type { PresenceRow, World } from '../src/status.ts';

const open = { hideServer: false, shareIpServers: false };
const sharing = { hideServer: false, shareIpServers: true };
const hidden = { hideServer: true, shareIpServers: false };
const hiddenAndSharing = { hideServer: true, shareIpServers: true };

const server = (host: string, port = 25565): World => ({ type: 'server', host, port });

describe('visibleWorld', () => {
  test('null stays null', () => {
    assert.equal(visibleWorld(null, open), null);
    assert.equal(visibleWorld(null, sharing), null);
    assert.equal(visibleWorld(null, hidden), null);
  });

  test('hideServer hides a server', () => {
    assert.equal(visibleWorld(server('mc.hypixel.net'), hidden), null);
  });

  test('hideServer hides single-player', () => {
    assert.equal(visibleWorld({ type: 'singleplayer' }, hidden), null);
  });

  test('hideServer wins over shareIpServers', () => {
    assert.equal(visibleWorld(server('203.0.113.7'), hiddenAndSharing), null);
    assert.equal(visibleWorld(server('mc.hypixel.net'), hiddenAndSharing), null);
  });

  test('single-player is kept', () => {
    assert.deepEqual(visibleWorld({ type: 'singleplayer' }, open), { type: 'singleplayer' });
  });

  test('a domain server keeps host and port', () => {
    assert.deepEqual(visibleWorld(server('mc.hypixel.net'), open), server('mc.hypixel.net'));
    assert.deepEqual(visibleWorld(server('play.example.org', 25570), open), server('play.example.org', 25570));
  });

  test('a domain server is unchanged with shareIpServers on', () => {
    assert.deepEqual(visibleWorld(server('mc.hypixel.net'), sharing), server('mc.hypixel.net'));
  });

  const addresses: Array<[string, string]> = [
    ['public IPv4', '203.0.113.7'],
    ['private IPv4', '192.168.1.20'],
    ['loopback IPv4', '127.0.0.1'],
    ['public IPv6', '2001:db8::1'],
    ['loopback IPv6', '::1'],
    ['localhost', 'localhost'],
    ['.local name', 'myserver.local'],
    ['single label', 'survival'],
    ['nip.io name', '1.2.3.4.nip.io'],
  ];
  for (const [title, host] of addresses) {
    test(`${title} is shown as a plain server`, () => {
      assert.deepEqual(visibleWorld(server(host), open), { type: 'server' });
    });

    test(`${title} is kept with shareIpServers`, () => {
      assert.deepEqual(visibleWorld(server(host, 25570), sharing), server(host, 25570));
    });
  }

  test('a server without host is unchanged', () => {
    assert.deepEqual(visibleWorld({ type: 'server' }, open), { type: 'server' });
  });

  test('does not change its input', () => {
    const world = server('203.0.113.7');
    visibleWorld(world, open);
    assert.deepEqual(world, server('203.0.113.7'));
  });
});

describe('statusView joinable', () => {
  const row = (world: World | null, overrides: Partial<PresenceRow> = {}): PresenceRow => ({
    visibility: 'friends',
    hide_server: false,
    share_ip_servers: false,
    allow_join: true,
    last_seen_at: null,
    state: 'playing',
    instance: 'Survival',
    version: '1.21.4',
    world,
    ...overrides,
  });
  const joinable = (r: PresenceRow) => {
    const view = statusView(r);
    return view.state === 'playing' ? view.joinable : null;
  };

  test('a shared server is joinable', () => {
    assert.equal(joinable(row(server('mc.hypixel.net'))), true);
  });

  test('not with allow_join off', () => {
    assert.equal(joinable(row(server('mc.hypixel.net'), { allow_join: false })), false);
  });

  test('not when the server is hidden', () => {
    assert.equal(joinable(row(server('mc.hypixel.net'), { hide_server: true })), false);
  });

  test('not for a raw IP the owner does not share, but yes once shared', () => {
    assert.equal(joinable(row(server('203.0.113.7'))), false);
    assert.equal(joinable(row(server('203.0.113.7'), { share_ip_servers: true })), true);
  });

  test('not for single-player or the menus', () => {
    assert.equal(joinable(row({ type: 'singleplayer' })), false);
    assert.equal(joinable(row(null)), false);
  });

  test('online and offline statuses carry no joinable', () => {
    assert.deepEqual(statusView(row(null, { state: 'online' })), { state: 'online' });
    assert.deepEqual(statusView(row(null, { state: null })), { state: 'offline', lastSeenAt: null });
  });
});
