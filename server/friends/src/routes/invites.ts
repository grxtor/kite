import type { FastifyInstance } from 'fastify';
import type pg from 'pg';
import { publishEvent } from '../bus.ts';
import { currentUser, hour, type Context } from '../context.ts';
import { withTransaction } from '../db.ts';
import { badRequest, HttpError, notFound } from '../errors.ts';
import { inviteBody, uuidParams } from '../schemas.ts';
import { classifyHost, normalizeHost } from '../serverAddress.ts';
import { defaultMinecraftPort } from '../status.ts';
import { areFriends, hasBlockBetween, lockPair } from '../users.ts';

export const inviteTtlSeconds = 10 * 60;

type Queryable = Pick<pg.Pool, 'query'>;

interface InviteRow {
  uuid: string;
  name: string;
  host: string;
  port: number;
  created_at: Date;
  expires_at: Date;
}

export interface InviteJson {
  uuid: string;
  name: string;
  host: string;
  port: number;
  createdAt: string;
  expiresAt: string;
}

interface InviteUserRow {
  uuid: string;
  share_ip_servers: boolean;
  allow_invites: boolean;
}

function inviteJson(row: InviteRow): InviteJson {
  return {
    uuid: row.uuid,
    name: row.name,
    host: row.host,
    port: row.port,
    createdAt: row.created_at.toISOString(),
    expiresAt: row.expires_at.toISOString(),
  };
}

// The column naming the player on the other end of the invite. Constants only, never input.
const otherSide = { incoming: ['to_uuid', 'from_uuid'], outgoing: ['from_uuid', 'to_uuid'] } as const;

async function listInvites(db: Queryable, me: string, now: Date, direction: keyof typeof otherSide) {
  const [mine, theirs] = otherSide[direction];
  const { rows } = await db.query<InviteRow>(
    `select u.uuid, u.name, i.host, i.port, i.created_at, i.expires_at
     from invites i join users u on u.uuid = i.${theirs}
     where i.${mine} = $1 and i.expires_at > $2
     order by i.created_at desc, u.uuid`,
    [me, now],
  );
  return rows.map(inviteJson);
}

/** Live invites to and from `me`. Part of GET /v1/friends, so the launcher's poll picks them up. */
export async function liveInvites(db: Queryable, me: string, now: Date) {
  return { incoming: await listInvites(db, me, now, 'incoming'), outgoing: await listInvites(db, me, now, 'outgoing') };
}

/** Drops `sender`'s invites to addresses others may no longer see; for when shareIpServers is off. */
export async function dropUnsharedInvites(client: pg.PoolClient, sender: string): Promise<void> {
  const { rows } = await client.query<{ to_uuid: string; host: string }>(
    'select to_uuid, host from invites where from_uuid = $1',
    [sender],
  );
  const unshared = rows.filter((row) => classifyHost(row.host) !== 'domain').map((row) => row.to_uuid);
  if (unshared.length === 0) return;
  await client.query('delete from invites where from_uuid = $1 and to_uuid = any($2::uuid[])', [sender, unshared]);
}

/** Deletes invites in both directions between two players. The caller holds the pair lock. */
export async function deleteInvitesBetween(client: pg.PoolClient, a: string, b: string): Promise<void> {
  await client.query('delete from invites where (from_uuid = $1 and to_uuid = $2) or (from_uuid = $2 and to_uuid = $1)', [
    a,
    b,
  ]);
}

export function registerInviteRoutes(app: FastifyInstance, ctx: Context) {
  app.get('/v1/invites', async (request) => liveInvites(ctx.pool, currentUser(request).uuid, ctx.now()));

  app.put<{ Params: { uuid: string }; Body: { host: string; port?: number } }>(
    '/v1/invites/:uuid',
    { schema: { params: uuidParams, body: inviteBody } },
    async (request) => {
      const sender = currentUser(request);
      const me = sender.uuid;
      const to = request.params.uuid.toLowerCase();
      const limits = ctx.config.rateLimits;
      ctx.limit(`invites:${me}`, limits.invitesPerHour, hour);
      if (to === me) throw badRequest('That is you');
      const host = normalizeHost(request.body.host);
      if (!host) throw badRequest('host is not a valid server address');
      const port = request.body.port ?? defaultMinecraftPort;

      const now = ctx.now();
      const expiresAt = new Date(now.getTime() + inviteTtlSeconds * 1000);
      const row = await withTransaction(ctx.pool, async (client): Promise<InviteRow> => {
        await lockPair(client, me, to);
        const blocks = await hasBlockBetween(client, me, to);
        // Not friends, unknown and blocked all look the same.
        if (blocks.byA || blocks.byB || !(await areFriends(client, me, to))) throw notFound('Not friends with this player');

        // Share locks hold off a settings change on either side until the invite is written.
        const { rows: users } = await client.query<InviteUserRow>(
          'select uuid, share_ip_servers, allow_invites from users where uuid in ($1, $2) order by uuid for share',
          [me, to],
        );
        const from = users.find((user) => user.uuid === me);
        const recipient = users.find((user) => user.uuid === to);
        if (!from || !recipient) throw notFound('Not friends with this player');
        if (classifyHost(host) !== 'domain' && !from.share_ip_servers) {
          throw new HttpError(409, 'address_not_shared', 'Raw IP and local addresses need shareIpServers on');
        }
        if (!recipient.allow_invites) throw new HttpError(409, 'invites_off', 'This player does not take invites');

        ctx.limit(`invite-pair:${me}:${to}`, limits.invitesPerPairPerHour, hour);
        const { rows } = await client.query<InviteRow>(
          `with saved as (
             insert into invites (from_uuid, to_uuid, host, port, created_at, expires_at) values ($1, $2, $3, $4, $5, $6)
             on conflict (from_uuid, to_uuid) do update set host = excluded.host, port = excluded.port,
               created_at = excluded.created_at, expires_at = excluded.expires_at
             returning to_uuid, host, port, created_at, expires_at)
           select u.uuid, u.name, s.host, s.port, s.created_at, s.expires_at from saved s join users u on u.uuid = s.to_uuid`,
          [me, to, host, port, now, expiresAt],
        );
        return rows[0]!;
      });
      const invite = inviteJson(row);
      publishEvent(ctx.bus, to, { type: 'invite', invite: { ...invite, uuid: me, name: sender.name } });
      return invite;
    },
  );

  app.delete<{ Params: { uuid: string } }>('/v1/invites/:uuid', { schema: { params: uuidParams } }, async (request, reply) => {
    const me = currentUser(request).uuid;
    const to = request.params.uuid.toLowerCase();
    const removed = await ctx.pool.query('delete from invites where from_uuid = $1 and to_uuid = $2 and expires_at > $3', [
      me,
      to,
      ctx.now(),
    ]);
    if (removed.rowCount === 0) throw notFound('No invite to this player');
    publishEvent(ctx.bus, to, { type: 'invite.cancelled', uuid: me });
    return reply.status(204).send();
  });

  app.post<{ Params: { uuid: string } }>(
    '/v1/invites/:uuid/dismiss',
    { schema: { params: uuidParams } },
    async (request, reply) => {
      const removed = await ctx.pool.query('delete from invites where from_uuid = $1 and to_uuid = $2 and expires_at > $3', [
        request.params.uuid,
        currentUser(request).uuid,
        ctx.now(),
      ]);
      if (removed.rowCount === 0) throw notFound('No invite from this player');
      return reply.status(204).send();
    },
  );
}
