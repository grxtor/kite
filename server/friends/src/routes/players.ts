import type { FastifyInstance } from 'fastify';
import { currentUser, minute, type Context } from '../context.ts';
import { notFound } from '../errors.ts';
import { nameParams } from '../schemas.ts';
import { presenceColumns, statusView, type PresenceRow } from '../status.ts';

interface LookupRow extends PresenceRow {
  uuid: string;
  name: string;
  is_friend: boolean;
  is_blocked: boolean;
}

export function registerPlayerRoutes(app: FastifyInstance, ctx: Context) {
  app.get<{ Params: { name: string } }>(
    '/v1/players/:name/status',
    { schema: { params: nameParams } },
    async (request) => {
      const me = currentUser(request).uuid;
      ctx.limit(`lookups:${me}`, ctx.config.rateLimits.lookupsPerMinute, minute);

      const { rows } = await ctx.pool.query<LookupRow>(
        `select u.uuid, u.name, ${presenceColumns},
           exists (select 1 from friendships f
                   where f.user_a = least($1::uuid, u.uuid) and f.user_b = greatest($1::uuid, u.uuid)) as is_friend,
           exists (select 1 from blocks b
                   where (b.blocker = $1 and b.blocked = u.uuid) or (b.blocker = u.uuid and b.blocked = $1)) as is_blocked
         from users u
         left join presence p on p.user_uuid = u.uuid and p.expires_at > $3
         where u.name_lower = lower($2)
         order by u.name_seen_at desc
         limit 1`,
        [me, request.params.name, ctx.now()],
      );
      const row = rows[0];
      // Unknown, private and blocked all look the same from outside.
      const visible =
        row && !row.is_blocked && (row.uuid === me || row.is_friend || row.visibility === 'everyone');
      if (!visible) throw notFound('No such player');
      return { uuid: row.uuid, name: row.name, status: statusView(row) };
    },
  );
}
