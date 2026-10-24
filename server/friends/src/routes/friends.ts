import type { FastifyInstance } from 'fastify';
import type pg from 'pg';
import { currentUser, day, hour, type Context, userJson } from '../context.ts';
import { withTransaction } from '../db.ts';
import { badRequest, HttpError, notFound } from '../errors.ts';
import { nameBody, uuidParams } from '../schemas.ts';
import { presenceColumns, statusView, type PresenceRow } from '../status.ts';
import { areFriends, findUserByName, hasBlockBetween, type UserRef } from '../users.ts';

const maxFriends = 500;
const maxPendingOutgoing = 100;

interface FriendRow extends PresenceRow {
  uuid: string;
  name: string;
  since: Date;
}

interface RequestRow {
  uuid: string;
  name: string;
  created_at: Date;
}

const noSuchPlayer = () => notFound('No such player');

function requestJson(row: RequestRow) {
  return { uuid: row.uuid, name: row.name, createdAt: row.created_at.toISOString() };
}

async function friendCount(client: pg.PoolClient, uuid: string): Promise<number> {
  const { rows } = await client.query<{ count: number }>(
    'select count(*)::int as count from friendships where user_a = $1 or user_b = $1',
    [uuid],
  );
  return rows[0]?.count ?? 0;
}

/** Turns a pending request from `from` to `to` into a friendship; false if there was none. */
async function acceptRequest(client: pg.PoolClient, from: string, to: string, now: Date): Promise<boolean> {
  const removed = await client.query('delete from friend_requests where from_uuid = $1 and to_uuid = $2', [from, to]);
  if (removed.rowCount === 0) return false;
  if ((await friendCount(client, from)) >= maxFriends || (await friendCount(client, to)) >= maxFriends) {
    throw new HttpError(409, 'limit_reached', `A player can have at most ${maxFriends} friends`);
  }
  await client.query('delete from friend_requests where from_uuid = $1 and to_uuid = $2', [to, from]);
  await client.query(
    `insert into friendships (user_a, user_b, created_at) values (least($1::uuid, $2::uuid), greatest($1::uuid, $2::uuid), $3)
     on conflict do nothing`,
    [from, to, now],
  );
  return true;
}

export function registerFriendRoutes(app: FastifyInstance, ctx: Context) {
  app.get('/v1/friends', async (request) => {
    const me = currentUser(request).uuid;
    const friends = await ctx.pool.query<FriendRow>(
      `select u.uuid, u.name, f.created_at as since, ${presenceColumns}
       from friendships f
       join users u on u.uuid = case when f.user_a = $1 then f.user_b else f.user_a end
       left join presence p on p.user_uuid = u.uuid and p.expires_at > $2
       where f.user_a = $1 or f.user_b = $1
       order by u.name_lower, u.uuid`,
      [me, ctx.now()],
    );
    const incoming = await ctx.pool.query<RequestRow>(
      `select u.uuid, u.name, r.created_at from friend_requests r join users u on u.uuid = r.from_uuid
       where r.to_uuid = $1 order by r.created_at desc`,
      [me],
    );
    const outgoing = await ctx.pool.query<RequestRow>(
      `select u.uuid, u.name, r.created_at from friend_requests r join users u on u.uuid = r.to_uuid
       where r.from_uuid = $1 order by r.created_at desc`,
      [me],
    );
    return {
      friends: friends.rows.map((row) => ({
        uuid: row.uuid,
        name: row.name,
        since: row.since.toISOString(),
        status: statusView(row),
      })),
      requests: { incoming: incoming.rows.map(requestJson), outgoing: outgoing.rows.map(requestJson) },
    };
  });

  app.post<{ Body: { name: string } }>('/v1/friends/requests', { schema: { body: nameBody } }, async (request, reply) => {
    const me = currentUser(request).uuid;
    const limits = ctx.config.rateLimits;
    // Counted before the lookup, so the endpoint cannot be used to probe names quickly.
    ctx.limit(`friend-requests:${me}`, limits.friendRequestsPerHour, hour);

    const target = await findUserByName(ctx.pool, request.body.name);
    if (!target) throw noSuchPlayer();
    if (target.uuid === me) throw badRequest('That is you');
    const blocks = await hasBlockBetween(ctx.pool, me, target.uuid);
    if (blocks.byA) throw new HttpError(409, 'blocked_by_you', 'You blocked this player; unblock them first');
    if (blocks.byB) throw noSuchPlayer();
    if (await areFriends(ctx.pool, me, target.uuid)) throw new HttpError(409, 'already_friends', 'Already friends');

    const result = await withTransaction(ctx.pool, async (client) => {
      const now = ctx.now();
      if (await acceptRequest(client, target.uuid, me, now)) return 'accepted';
      const existing = await client.query('select 1 from friend_requests where from_uuid = $1 and to_uuid = $2', [
        me,
        target.uuid,
      ]);
      if (existing.rowCount) return 'unchanged';

      const pending = await client.query<{ count: number }>(
        'select count(*)::int as count from friend_requests where from_uuid = $1',
        [me],
      );
      if ((pending.rows[0]?.count ?? 0) >= maxPendingOutgoing) {
        throw new HttpError(409, 'limit_reached', `At most ${maxPendingOutgoing} requests can be pending`);
      }
      if ((await friendCount(client, me)) >= maxFriends) {
        throw new HttpError(409, 'limit_reached', `A player can have at most ${maxFriends} friends`);
      }
      ctx.limit(`friend-request-pair:${me}:${target.uuid}`, limits.friendRequestsPerPairPerDay, day);
      await client.query('insert into friend_requests (from_uuid, to_uuid, created_at) values ($1, $2, $3)', [
        me,
        target.uuid,
        now,
      ]);
      return 'created';
    });

    const user = userJson(target);
    if (result === 'accepted') return { status: 'accepted', user };
    return reply.status(result === 'created' ? 201 : 200).send({ status: 'pending', user });
  });

  app.post<{ Params: { uuid: string } }>(
    '/v1/friends/requests/:uuid/accept',
    { schema: { params: uuidParams } },
    async (request) => {
      const me = currentUser(request).uuid;
      const from = request.params.uuid.toLowerCase();
      const user = await withTransaction(ctx.pool, async (client): Promise<UserRef | null> => {
        if (!(await acceptRequest(client, from, me, ctx.now()))) return null;
        const { rows } = await client.query<UserRef>('select uuid, name from users where uuid = $1', [from]);
        return rows[0] ?? null;
      });
      if (!user) throw notFound('No pending request from this player');
      return { user: userJson(user) };
    },
  );

  app.post<{ Params: { uuid: string } }>(
    '/v1/friends/requests/:uuid/decline',
    { schema: { params: uuidParams } },
    async (request, reply) => {
      const removed = await ctx.pool.query('delete from friend_requests where from_uuid = $1 and to_uuid = $2', [
        request.params.uuid,
        currentUser(request).uuid,
      ]);
      if (removed.rowCount === 0) throw notFound('No pending request from this player');
      return reply.status(204).send();
    },
  );

  app.delete<{ Params: { uuid: string } }>(
    '/v1/friends/requests/:uuid',
    { schema: { params: uuidParams } },
    async (request, reply) => {
      const removed = await ctx.pool.query('delete from friend_requests where from_uuid = $1 and to_uuid = $2', [
        currentUser(request).uuid,
        request.params.uuid,
      ]);
      if (removed.rowCount === 0) throw notFound('No pending request to this player');
      return reply.status(204).send();
    },
  );

  app.delete<{ Params: { uuid: string } }>('/v1/friends/:uuid', { schema: { params: uuidParams } }, async (request, reply) => {
    const removed = await ctx.pool.query(
      'delete from friendships where user_a = least($1::uuid, $2::uuid) and user_b = greatest($1::uuid, $2::uuid)',
      [currentUser(request).uuid, request.params.uuid],
    );
    if (removed.rowCount === 0) throw notFound('Not friends with this player');
    return reply.status(204).send();
  });
}
