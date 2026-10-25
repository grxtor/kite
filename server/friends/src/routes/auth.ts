import { createHash, randomBytes, randomUUID } from 'node:crypto';
import type { FastifyInstance, FastifyRequest } from 'fastify';
import { currentUser, day, minute, type Context, userJson } from '../context.ts';
import { withTransaction } from '../db.ts';
import { HttpError } from '../errors.ts';
import { hasJoined } from '../mojang.ts';
import { challengeBody, nameBody } from '../schemas.ts';

const tokenPrefix = 'kfs_';
const tokenPattern = /^Bearer (kfs_[A-Za-z0-9_-]{43})$/;
const maxSessionsPerUser = 10;
// Each login attempt asks Mojang, so a challenge allows only a few.
const maxLoginAttempts = 5;

export function hashToken(token: string): Buffer {
  return createHash('sha256').update(token).digest();
}

const challengeInvalid = () => new HttpError(400, 'challenge_invalid', 'Unknown, used or expired challenge');

/** Resolves the bearer token to its user, or throws 401. */
export function authenticator(ctx: Context) {
  return async (request: FastifyRequest): Promise<void> => {
    const match = tokenPattern.exec(request.headers.authorization ?? '');
    if (!match?.[1]) throw new HttpError(401, 'unauthorized', 'Missing or malformed token');
    const tokenHash = hashToken(match[1]);
    const { rows } = await ctx.pool.query<{ uuid: string; name: string }>(
      `select u.uuid, u.name from sessions s join users u on u.uuid = s.user_uuid
       where s.token_hash = $1 and s.expires_at > $2`,
      [tokenHash, ctx.now()],
    );
    const row = rows[0];
    if (!row) throw new HttpError(401, 'unauthorized', 'Unknown or expired token');
    request.user = { uuid: row.uuid, name: row.name, tokenHash };
    ctx.limit(`user:${row.uuid}`, ctx.config.rateLimits.userPerMinute, minute);
  };
}

export function registerAuthRoutes(app: FastifyInstance, ctx: Context, clientIp: (request: FastifyRequest) => string) {
  const limitAuth = async (request: FastifyRequest) => {
    ctx.limit(`auth:${clientIp(request)}`, ctx.config.rateLimits.authPerMinute, minute);
  };

  app.post<{ Body: { name: string } }>(
    '/v1/auth/challenge',
    { schema: { body: nameBody }, onRequest: limitAuth },
    async (request) => {
      const id = randomUUID();
      const serverId = randomBytes(20).toString('hex');
      const expiresAt = new Date(ctx.now().getTime() + ctx.config.challengeTtlSeconds * 1000);
      await ctx.pool.query('insert into challenges (id, name, server_id, expires_at) values ($1, $2, $3, $4)', [
        id,
        request.body.name,
        serverId,
        expiresAt,
      ]);
      return { challengeId: id, serverId, expiresAt: expiresAt.toISOString() };
    },
  );

  app.post<{ Body: { challengeId: string } }>(
    '/v1/auth/login',
    { schema: { body: challengeBody }, onRequest: limitAuth },
    async (request) => {
      const { challengeId } = request.body;
      const { rows } = await ctx.pool.query<{ name: string; server_id: string; attempts: number }>(
        `update challenges set attempts = attempts + 1 where id = $1 and expires_at > $2
         returning name, server_id, attempts`,
        [challengeId, ctx.now()],
      );
      const challenge = rows[0];
      if (!challenge || challenge.attempts > maxLoginAttempts) throw challengeInvalid();

      // The challenge survives a failed check so the launcher can retry once its join lands.
      const profile = await hasJoined(ctx.config.mojangSessionUrl, challenge.name, challenge.server_id);
      if (!profile) throw new HttpError(401, 'not_joined', 'Mojang has no join for this challenge');

      const token = tokenPrefix + randomBytes(32).toString('base64url');
      const now = ctx.now();
      const expiresAt = new Date(now.getTime() + ctx.config.sessionTtlDays * day);
      await withTransaction(ctx.pool, async (client) => {
        const consumed = await client.query('delete from challenges where id = $1 and expires_at > $2', [
          challengeId,
          now,
        ]);
        if (consumed.rowCount === 0) throw challengeInvalid();
        await client.query(
          `insert into users (uuid, name, name_lower, created_at, name_seen_at) values ($1, $2, lower($2), $3, $3)
           on conflict (uuid) do update set name = excluded.name, name_lower = excluded.name_lower,
             name_seen_at = excluded.name_seen_at`,
          [profile.uuid, profile.name, now],
        );
        await client.query(
          `delete from sessions where token_hash in (
             select token_hash from sessions where user_uuid = $1 order by seq desc offset $2)`,
          [profile.uuid, maxSessionsPerUser - 1],
        );
        await client.query(
          'insert into sessions (token_hash, user_uuid, created_at, expires_at) values ($1, $2, $3, $4)',
          [hashToken(token), profile.uuid, now, expiresAt],
        );
      });
      return { token, expiresAt: expiresAt.toISOString(), user: userJson(profile) };
    },
  );
}

export function registerLogoutRoute(app: FastifyInstance, ctx: Context) {
  app.post('/v1/auth/logout', async (request, reply) => {
    await ctx.pool.query('delete from sessions where token_hash = $1', [currentUser(request).tokenHash]);
    return reply.status(204).send();
  });
}
