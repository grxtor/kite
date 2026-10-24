import assert from 'node:assert/strict';
import { afterEach, describe, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { ApiResponse, ErrorBody, Player, TestConfigOverrides, TestServer } from './helpers.ts';

const MINUTE = 60;
const HOUR = 3600;
const DAY = 86_400;

describe('rate limits', () => {
  let s: TestServer | undefined;

  afterEach(async () => {
    await s?.close();
    s = undefined;
  });

  async function start(overrides: TestConfigOverrides): Promise<TestServer> {
    s = await startServer(overrides);
    return s;
  }

  function assertLimited(res: ApiResponse<ErrorBody>, maxWaitSeconds: number): void {
    assert.equal(res.status, 429);
    assert.equal(res.body.error, 'rate_limited');
    assert.equal(typeof res.body.message, 'string');
    const retryAfter = Number(res.headers.get('retry-after'));
    assert.ok(Number.isInteger(retryAfter), 'Retry-After is a whole number of seconds');
    assert.ok(retryAfter >= 1, `Retry-After ${retryAfter} is at least 1`);
    assert.ok(retryAfter <= maxWaitSeconds, `Retry-After ${retryAfter} is at most ${maxWaitSeconds}`);
  }

  const sendRequest = (server: TestServer, from: Player, to: Player) =>
    server.api<ErrorBody>('POST', '/v1/friends/requests', { token: from.token, body: { name: to.name } });

  describe('friend requests', () => {
    test('are limited per hour and allowed again after the window', async () => {
      const server = await start({ rateLimits: { friendRequestsPerHour: 2 } });
      const a = await server.newPlayer();
      const targets = [await server.newPlayer(), await server.newPlayer(), await server.newPlayer()];
      const [first, second, third] = targets;
      assert.ok(first && second && third);

      assert.equal((await sendRequest(server, a, first)).status, 201);
      assert.equal((await sendRequest(server, a, second)).status, 201);
      assertLimited(await sendRequest(server, a, third), HOUR);

      server.clock.advance(HOUR + 1);
      assert.equal((await sendRequest(server, a, third)).status, 201);
    });

    test('the hourly limit is per sender', async () => {
      const server = await start({ rateLimits: { friendRequestsPerHour: 1 } });
      const a = await server.newPlayer();
      const b = await server.newPlayer();
      const target = await server.newPlayer();
      const other = await server.newPlayer();
      assert.equal((await sendRequest(server, a, target)).status, 201);
      assertLimited(await sendRequest(server, a, other), HOUR);
      assert.equal((await sendRequest(server, b, other)).status, 201);
    });

    test('are limited to 3 a day to the same player', async () => {
      const server = await start({ rateLimits: { friendRequestsPerPairPerDay: 3 } });
      const a = await server.newPlayer();
      const b = await server.newPlayer();
      for (let i = 0; i < 3; i += 1) {
        assert.equal((await sendRequest(server, a, b)).status, 201, `send ${i + 1}`);
        const cancel = await server.api('DELETE', `/v1/friends/requests/${b.uuid}`, { token: a.token });
        assert.equal(cancel.status, 204);
      }
      assertLimited(await sendRequest(server, a, b), DAY);

      const other = await server.newPlayer();
      assert.equal((await sendRequest(server, a, other)).status, 201);

      server.clock.advance(DAY + 1);
      assert.equal((await sendRequest(server, a, b)).status, 201);
    });
  });

  describe('auth endpoints', () => {
    test('are limited per IP', async () => {
      const server = await start({ rateLimits: { authPerMinute: 3 } });
      for (let i = 0; i < 3; i += 1) {
        const res = await server.api('POST', '/v1/auth/challenge', { body: { name: 'Notch' } });
        assert.equal(res.status, 200);
      }
      assertLimited(await server.api<ErrorBody>('POST', '/v1/auth/challenge', { body: { name: 'Notch' } }), MINUTE);
      assertLimited(await server.api<ErrorBody>('POST', '/v1/auth/login', { body: { challengeId: 'x' } }), MINUTE);

      server.clock.advance(MINUTE + 1);
      assert.equal((await server.api('POST', '/v1/auth/challenge', { body: { name: 'Notch' } })).status, 200);
    });

    test('are counted per client address when a client IP header is set', async () => {
      const server = await start({ clientIpHeader: 'x-client-ip', rateLimits: { authPerMinute: 2 } });
      const from = (ip: string) => ({ body: { name: 'Notch' }, headers: { 'x-client-ip': ip } });
      assert.equal((await server.api('POST', '/v1/auth/challenge', from('198.51.100.1'))).status, 200);
      assert.equal((await server.api('POST', '/v1/auth/challenge', from('198.51.100.1'))).status, 200);
      assertLimited(await server.api<ErrorBody>('POST', '/v1/auth/challenge', from('198.51.100.1')), MINUTE);
      assert.equal((await server.api('POST', '/v1/auth/challenge', from('198.51.100.2'))).status, 200);
    });

    test('do not use up the limit of other endpoints', async () => {
      const server = await start({ rateLimits: { authPerMinute: 2 } });
      const player = await server.newPlayer();
      for (let i = 0; i < 5; i += 1) {
        assert.equal((await server.api('GET', '/v1/me', { token: player.token })).status, 200);
      }
    });
  });

  describe('per IP', () => {
    test('any request counts, 429 once the limit is passed', async () => {
      const server = await start({ rateLimits: { ipPerMinute: 10 } });
      const player = await server.newPlayer();
      let ok = 0;
      let limited: ApiResponse<ErrorBody> | undefined;
      for (let i = 0; i < 20 && limited === undefined; i += 1) {
        const res = await server.api<ErrorBody>('GET', '/v1/me', { token: player.token });
        if (res.status === 200) ok += 1;
        else limited = res;
      }
      assert.ok(limited, 'a request was limited');
      assert.equal(ok, 8, 'ten requests per minute includes the two used to sign in');
      assertLimited(limited, MINUTE);

      server.clock.advance(MINUTE + 1);
      assert.equal((await server.api('GET', '/v1/me', { token: player.token })).status, 200);
    });
  });

  describe('player lookups', () => {
    test('are limited per user and allowed again after a minute', async () => {
      const server = await start({ rateLimits: { lookupsPerMinute: 3 } });
      const a = await server.newPlayer();
      const b = await server.newPlayer();
      const lookup = (player: Player, name: string) =>
        server.api<ErrorBody>('GET', `/v1/players/${name}/status`, { token: player.token });

      for (let i = 0; i < 3; i += 1) assert.equal((await lookup(a, a.name)).status, 200);
      assertLimited(await lookup(a, a.name), MINUTE);
      assertLimited(await lookup(a, 'NobodyHere'), MINUTE);
      assert.equal((await lookup(b, b.name)).status, 200);

      server.clock.advance(MINUTE + 1);
      assert.equal((await lookup(a, a.name)).status, 200);
    });

    test('do not use up the limit of other endpoints', async () => {
      const server = await start({ rateLimits: { lookupsPerMinute: 1 } });
      const a = await server.newPlayer();
      for (let i = 0; i < 5; i += 1) {
        assert.equal((await server.api('GET', '/v1/friends', { token: a.token })).status, 200);
      }
    });
  });

  describe('per user', () => {
    test('signed-in requests are limited per user and allowed again after a minute', async () => {
      const server = await start({ rateLimits: { userPerMinute: 5 } });
      const a = await server.newPlayer();
      const b = await server.newPlayer();
      for (let i = 0; i < 5; i += 1) {
        assert.equal((await server.api('GET', '/v1/me', { token: a.token })).status, 200);
      }
      assertLimited(await server.api<ErrorBody>('GET', '/v1/me', { token: a.token }), MINUTE);
      assertLimited(await server.api<ErrorBody>('GET', '/v1/friends', { token: a.token }), MINUTE);
      assert.equal((await server.api('GET', '/v1/me', { token: b.token })).status, 200);

      server.clock.advance(MINUTE + 1);
      assert.equal((await server.api('GET', '/v1/me', { token: a.token })).status, 200);
    });

    test('the limit follows the user across tokens', async () => {
      const server = await start({ rateLimits: { userPerMinute: 3 } });
      const a = await server.newPlayer();
      const second = await server.signIn(a.name, a.uuid);
      assert.equal((await server.api('GET', '/v1/me', { token: a.token })).status, 200);
      assert.equal((await server.api('GET', '/v1/me', { token: second })).status, 200);
      assert.equal((await server.api('GET', '/v1/me', { token: a.token })).status, 200);
      assertLimited(await server.api<ErrorBody>('GET', '/v1/me', { token: second }), MINUTE);
    });
  });
});
