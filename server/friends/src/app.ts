import Fastify, { LogController, type FastifyError, type FastifyInstance, type FastifyRequest } from 'fastify';
import type pg from 'pg';
import type { Config } from './config.ts';
import { createContext, minute, type Context } from './context.ts';
import { HttpError } from './errors.ts';
import { RateLimiter } from './rateLimit.ts';
import { authenticator, registerAuthRoutes, registerLogoutRoute } from './routes/auth.ts';
import { registerBlockRoutes } from './routes/blocks.ts';
import { registerFriendRoutes } from './routes/friends.ts';
import { registerMeRoutes } from './routes/me.ts';
import { registerPlayerRoutes } from './routes/players.ts';

export interface AppDeps {
  config: Config;
  pool: pg.Pool;
  clock?: () => number;
}

const bodyLimitBytes = 8 * 1024;

function clientIpReader(config: Config) {
  return (request: FastifyRequest): string => {
    if (config.clientIpHeader) {
      const header = request.headers[config.clientIpHeader];
      const value = (Array.isArray(header) ? header[0] : header)?.split(',')[0]?.trim();
      if (value) return value;
    }
    return request.ip;
  };
}

/** Drops expired challenges, sessions and statuses, and forgets finished rate-limit windows. */
async function purgeExpired(ctx: Context): Promise<void> {
  const now = ctx.now();
  await ctx.pool.query('delete from challenges where expires_at <= $1', [now]);
  await ctx.pool.query('delete from sessions where expires_at <= $1', [now]);
  await ctx.pool.query('delete from presence where expires_at <= $1', [now]);
  ctx.limiter.sweep();
}

export async function buildApp({ config, pool, clock = Date.now }: AppDeps): Promise<FastifyInstance> {
  const app = Fastify({
    logger: { level: config.logLevel, redact: ['req.headers.authorization'] },
    // Requests carry player names and IPs, so only errors are logged.
    logController: new LogController({ disableRequestLogging: true }),
    bodyLimit: bodyLimitBytes,
  });
  const ctx = createContext(config, pool, clock, new RateLimiter(clock));
  const clientIp = clientIpReader(config);

  app.decorateRequest('user', null);

  app.addHook('onRequest', async (request, reply) => {
    reply.header('cache-control', 'no-store');
    ctx.limit(`ip:${clientIp(request)}`, config.rateLimits.ipPerMinute, minute);
  });

  app.setErrorHandler((error: FastifyError | HttpError, request, reply) => {
    if (error instanceof HttpError) {
      if (error.retryAfterSeconds !== null) reply.header('retry-after', String(error.retryAfterSeconds));
      return reply.status(error.status).send({ error: error.code, message: error.message });
    }
    if (error.validation || (error.statusCode && error.statusCode < 500)) {
      return reply.status(error.validation ? 400 : (error.statusCode ?? 400)).send({ error: 'bad_request', message: error.message });
    }
    request.log.error(error);
    return reply.status(500).send({ error: 'internal', message: 'Something went wrong' });
  });

  app.setNotFoundHandler((_request, reply) => reply.status(404).send({ error: 'not_found', message: 'No such endpoint' }));

  app.get('/healthz', async () => {
    await pool.query('select 1');
    return { ok: true };
  });

  registerAuthRoutes(app, ctx, clientIp);

  await app.register(async (scope) => {
    scope.addHook('onRequest', authenticator(ctx));
    registerLogoutRoute(scope, ctx);
    registerMeRoutes(scope, ctx);
    registerFriendRoutes(scope, ctx);
    registerBlockRoutes(scope, ctx);
    registerPlayerRoutes(scope, ctx);
  });

  // An idle connection dropped by Postgres must not take the process down; the pool replaces it.
  const onPoolError = (error: Error) => app.log.error(error, 'idle Postgres connection failed');
  pool.on('error', onPoolError);

  const purgeTimer = setInterval(() => {
    purgeExpired(ctx).catch((error: unknown) => app.log.error(error, 'purging expired rows failed'));
  }, minute);
  purgeTimer.unref();
  app.addHook('onClose', async () => {
    clearInterval(purgeTimer);
    pool.off('error', onPoolError);
  });

  return app;
}
