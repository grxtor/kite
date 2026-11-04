import assert from 'node:assert/strict';
import { describe, test } from 'node:test';
import { defaultRateLimits, loadConfig } from '../src/config.ts';

describe('defaultRateLimits', () => {
  test('match the documented limits', () => {
    assert.deepEqual(defaultRateLimits, {
      ipPerMinute: 300,
      authPerMinute: 20,
      userPerMinute: 120,
      friendRequestsPerHour: 30,
      friendRequestsPerPairPerDay: 3,
      lookupsPerMinute: 30,
      invitesPerHour: 30,
      invitesPerPairPerHour: 5,
      shareCodesPerHour: 20,
      shareCodeFetchesPerHour: 30,
      chatMessagesPerMinute: 30,
      chatMessagesPerDay: 2000,
      chatReportsPerDay: 20,
      socketFramesPerMinute: 120,
      crashReportsPerHour: 20,
    });
  });
});

describe('loadConfig', () => {
  test('throws without DATABASE_URL', () => {
    assert.throws(() => loadConfig({}), /DATABASE_URL/);
    assert.throws(() => loadConfig({ PORT: '9000' }), /DATABASE_URL/);
  });

  test('uses the documented defaults', () => {
    assert.deepEqual(loadConfig({ DATABASE_URL: 'postgres://db/kite' }), {
      port: 8080,
      host: '0.0.0.0',
      databaseUrl: 'postgres://db/kite',
      mojangSessionUrl: 'https://sessionserver.mojang.com',
      clientIpHeader: null,
      logLevel: 'info',
      statusTtlSeconds: 180,
      sessionTtlDays: 30,
      challengeTtlSeconds: 120,
      heartbeatMs: 30_000,
      modWebhookUrl: null,
      rateLimits: defaultRateLimits,
    });
  });

  test('takes MOD_WEBHOOK_URL when it is an http(s) URL, and never repeats it in an error', () => {
    const secret = 'https://discord.com/api/webhooks/1/s3cret';
    assert.equal(loadConfig({ DATABASE_URL: 'x', MOD_WEBHOOK_URL: secret }).modWebhookUrl, secret);
    assert.equal(loadConfig({ DATABASE_URL: 'x', MOD_WEBHOOK_URL: '' }).modWebhookUrl, null);
    for (const bad of ['not a url s3cret', 'ftp://example.com/s3cret', 'https://user:s3cret@example.com/hook']) {
      assert.throws(
        () => loadConfig({ DATABASE_URL: 'x', MOD_WEBHOOK_URL: bad }),
        (error: Error) => /MOD_WEBHOOK_URL/.test(error.message) && !error.message.includes('s3cret'),
      );
    }
  });

  test('reads every variable from the environment', () => {
    const config = loadConfig({
      DATABASE_URL: 'postgres://db/other',
      PORT: '9100',
      HOST: '127.0.0.1',
      MOJANG_SESSION_URL: 'http://localhost:9999',
      CLIENT_IP_HEADER: 'cf-connecting-ip',
      STATUS_TTL_SECONDS: '90',
      SESSION_TTL_DAYS: '7',
      LOG_LEVEL: 'warn',
    });
    assert.equal(config.databaseUrl, 'postgres://db/other');
    assert.equal(config.port, 9100);
    assert.equal(config.host, '127.0.0.1');
    assert.equal(config.mojangSessionUrl, 'http://localhost:9999');
    assert.equal(config.clientIpHeader, 'cf-connecting-ip');
    assert.equal(config.statusTtlSeconds, 90);
    assert.equal(config.sessionTtlDays, 7);
    assert.equal(config.logLevel, 'warn');
  });

  test('parses numbers as numbers', () => {
    const config = loadConfig({ DATABASE_URL: 'x', PORT: '3000', STATUS_TTL_SECONDS: '60', SESSION_TTL_DAYS: '1' });
    assert.equal(typeof config.port, 'number');
    assert.equal(typeof config.statusTtlSeconds, 'number');
    assert.equal(typeof config.sessionTtlDays, 'number');
  });
});
