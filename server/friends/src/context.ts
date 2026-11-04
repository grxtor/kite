import type { FastifyBaseLogger, FastifyRequest } from 'fastify';
import type pg from 'pg';
import type { EventBus } from './bus.ts';
import type { Config } from './config.ts';
import { HttpError, rateLimited } from './errors.ts';
import type { RateLimiter } from './rateLimit.ts';

export interface AuthUser {
  uuid: string;
  name: string;
  tokenHash: Buffer;
  sessionExpiresAt: Date;
}

declare module 'fastify' {
  interface FastifyRequest {
    user: AuthUser | null;
  }
}

export interface Context {
  config: Config;
  pool: pg.Pool;
  limiter: RateLimiter;
  bus: EventBus;
  log: FastifyBaseLogger;
  now(): Date;
  /** Counts one hit against `key` and throws 429 once `limit` is used up for the window. */
  limit(key: string, limit: number, windowMs: number): void;
}

export const minute = 60_000;
export const hour = 60 * minute;
export const day = 24 * hour;

export interface ContextDeps {
  config: Config;
  pool: pg.Pool;
  clock: () => number;
  limiter: RateLimiter;
  bus: EventBus;
  log: FastifyBaseLogger;
}

export function createContext({ config, pool, clock, limiter, bus, log }: ContextDeps): Context {
  return {
    config,
    pool,
    limiter,
    bus,
    log,
    now: () => new Date(clock()),
    limit(key, limit, windowMs) {
      const result = limiter.hit(key, limit, windowMs);
      if (!result.ok) throw rateLimited(result.retryAfterSeconds);
    },
  };
}

export function currentUser(request: FastifyRequest): AuthUser {
  if (!request.user) throw new HttpError(401, 'unauthorized', 'Sign in first');
  return request.user;
}

export function userJson(row: { uuid: string; name: string }): { uuid: string; name: string } {
  return { uuid: row.uuid, name: row.name };
}
