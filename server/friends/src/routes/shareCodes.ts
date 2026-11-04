import type { FastifyInstance } from 'fastify';
import { currentUser, day, hour, type Context } from '../context.ts';
import { badRequest, HttpError, notFound } from '../errors.ts';
import { shareCodeParams } from '../schemas.ts';
import { displayShareCode, normalizeShareCode, newShareCode, parseManifest, type Manifest } from '../shareCodes.ts';

/** A code lives this long after it was made or last fetched. */
export const shareCodeTtlDays = 90;
const maxCodesPerUser = 50;
// 500 entries in compact JSON stay under this; every other route keeps the 8 KB limit.
const manifestBodyLimitBytes = 64 * 1024;
// There are 8.5 × 10^11 codes, so a clash is rare and a few tries always find a free one.
const maxCodeAttempts = 5;

interface CodeRow {
  code: string;
  manifest: Manifest;
  created_at: Date;
  expires_at: Date;
}

function codeFrom(raw: string): string {
  const code = normalizeShareCode(raw);
  if (!code) throw badRequest('Not a share code');
  return code;
}

export function registerShareCodeRoutes(app: FastifyInstance, ctx: Context) {
  const expiryFrom = (now: Date) => new Date(now.getTime() + shareCodeTtlDays * day);

  app.post('/v1/share-codes', { bodyLimit: manifestBodyLimitBytes }, async (request, reply) => {
    const me = currentUser(request).uuid;
    ctx.limit(`share-codes:${me}`, ctx.config.rateLimits.shareCodesPerHour, hour);
    const manifest = parseManifest(request.body);
    const now = ctx.now();

    const { rows } = await ctx.pool.query<{ count: number }>(
      'select count(*)::int as count from share_codes where owner_uuid = $1 and expires_at > $2',
      [me, now],
    );
    if ((rows[0]?.count ?? 0) >= maxCodesPerUser) {
      throw new HttpError(409, 'limit_reached', `At most ${maxCodesPerUser} share codes can be live`);
    }

    const expiresAt = expiryFrom(now);
    for (let attempt = 0; attempt < maxCodeAttempts; attempt += 1) {
      const code = newShareCode();
      const inserted = await ctx.pool.query(
        `insert into share_codes (code, owner_uuid, manifest, created_at, expires_at) values ($1, $2, $3, $4, $5)
         on conflict (code) do nothing`,
        [code, me, JSON.stringify(manifest), now, expiresAt],
      );
      if (inserted.rowCount) {
        return reply
          .status(201)
          .send({ code: displayShareCode(code), createdAt: now.toISOString(), expiresAt: expiresAt.toISOString() });
      }
    }
    throw new Error('no free share code found');
  });

  app.get('/v1/share-codes', async (request) => {
    const { rows } = await ctx.pool.query<CodeRow>(
      `select code, manifest, created_at, expires_at from share_codes
       where owner_uuid = $1 and expires_at > $2 order by created_at desc, code`,
      [currentUser(request).uuid, ctx.now()],
    );
    return {
      codes: rows.map((row) => ({
        code: displayShareCode(row.code),
        name: row.manifest.name,
        minecraftVersion: row.manifest.minecraftVersion,
        loader: row.manifest.loader,
        createdAt: row.created_at.toISOString(),
        expiresAt: row.expires_at.toISOString(),
      })),
    };
  });

  app.get<{ Params: { code: string } }>(
    '/v1/share-codes/:code',
    { schema: { params: shareCodeParams } },
    async (request) => {
      const me = currentUser(request).uuid;
      // Counted before the lookup, so codes cannot be guessed quickly.
      ctx.limit(`share-code-fetches:${me}`, ctx.config.rateLimits.shareCodeFetchesPerHour, hour);
      const code = codeFrom(request.params.code);
      const now = ctx.now();
      // Every fetch pushes the expiry out, so a code people still use stays alive.
      const { rows } = await ctx.pool.query<CodeRow>(
        `update share_codes set expires_at = $3 where code = $1 and expires_at > $2
         returning code, manifest, created_at, expires_at`,
        [code, now, expiryFrom(now)],
      );
      const row = rows[0];
      if (!row) throw notFound('No such share code');
      return {
        code: displayShareCode(row.code),
        manifest: row.manifest,
        createdAt: row.created_at.toISOString(),
        expiresAt: row.expires_at.toISOString(),
      };
    },
  );

  app.delete<{ Params: { code: string } }>(
    '/v1/share-codes/:code',
    { schema: { params: shareCodeParams } },
    async (request, reply) => {
      const removed = await ctx.pool.query('delete from share_codes where code = $1 and owner_uuid = $2', [
        codeFrom(request.params.code),
        currentUser(request).uuid,
      ]);
      // Someone else's code looks like an unknown one.
      if (removed.rowCount === 0) throw notFound('No such share code');
      return reply.status(204).send();
    },
  );
}
