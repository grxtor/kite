import assert from 'node:assert/strict';
import { beforeEach, describe, test } from 'node:test';
import { RateLimiter } from '../src/rateLimit.ts';

const MINUTE = 60_000;
const START = 600_000;

describe('RateLimiter', () => {
  let now = START;
  let limiter = new RateLimiter(() => now);

  beforeEach(() => {
    now = START;
    limiter = new RateLimiter(() => now);
  });

  test('allows hits up to the limit with no wait', () => {
    for (let i = 0; i < 3; i += 1) {
      assert.deepEqual(limiter.hit('k', 3, MINUTE), { ok: true, retryAfterSeconds: 0 });
    }
  });

  test('rejects the hit over the limit with a wait of at least a second', () => {
    for (let i = 0; i < 3; i += 1) limiter.hit('k', 3, MINUTE);
    const result = limiter.hit('k', 3, MINUTE);
    assert.equal(result.ok, false);
    assert.ok(result.retryAfterSeconds >= 1);
    assert.ok(result.retryAfterSeconds <= 60);
  });

  test('keeps rejecting until the window is over', () => {
    limiter.hit('k', 1, MINUTE);
    now += 30_000;
    assert.equal(limiter.hit('k', 1, MINUTE).ok, false);
    now += 29_000;
    assert.equal(limiter.hit('k', 1, MINUTE).ok, false);
  });

  test('allows again after the window', () => {
    limiter.hit('k', 1, MINUTE);
    assert.equal(limiter.hit('k', 1, MINUTE).ok, false);
    now += MINUTE + 1;
    assert.deepEqual(limiter.hit('k', 1, MINUTE), { ok: true, retryAfterSeconds: 0 });
  });

  test('the wait shrinks as time passes', () => {
    limiter.hit('k', 1, MINUTE);
    const early = limiter.hit('k', 1, MINUTE).retryAfterSeconds;
    now += 40_000;
    const late = limiter.hit('k', 1, MINUTE).retryAfterSeconds;
    assert.ok(late >= 1);
    assert.ok(late < early);
  });

  test('the wait is at least one second just before the window ends', () => {
    limiter.hit('k', 1, MINUTE);
    now += MINUTE - 1;
    const result = limiter.hit('k', 1, MINUTE);
    assert.equal(result.ok, false);
    assert.equal(result.retryAfterSeconds, 1);
  });

  test('keys are counted separately', () => {
    limiter.hit('a', 1, MINUTE);
    assert.equal(limiter.hit('a', 1, MINUTE).ok, false);
    assert.equal(limiter.hit('b', 1, MINUTE).ok, true);
  });

  test('a longer window waits longer', () => {
    limiter.hit('hour', 1, 60 * MINUTE);
    const result = limiter.hit('hour', 1, 60 * MINUTE);
    assert.equal(result.ok, false);
    assert.ok(result.retryAfterSeconds > 60);
    assert.ok(result.retryAfterSeconds <= 3600);
  });

  test('a rejected hit does not extend the block', () => {
    limiter.hit('k', 1, MINUTE);
    for (let i = 0; i < 10; i += 1) limiter.hit('k', 1, MINUTE);
    now += MINUTE + 1;
    assert.equal(limiter.hit('k', 1, MINUTE).ok, true);
  });

  test('sweep keeps counters that are still in their window', () => {
    limiter.hit('k', 1, MINUTE);
    now += 10_000;
    limiter.sweep();
    assert.equal(limiter.hit('k', 1, MINUTE).ok, false);
  });

  test('sweep forgets counters whose window has passed', () => {
    limiter.hit('k', 1, MINUTE);
    now += 2 * MINUTE;
    limiter.sweep();
    assert.equal(limiter.hit('k', 1, MINUTE).ok, true);
    assert.equal(limiter.hit('k', 1, MINUTE).ok, false);
  });
});
