import type { FastifyInstance } from 'fastify';
import { currentUser, type Context } from '../context.ts';
import { withTransaction } from '../db.ts';
import { badRequest, notFound } from '../errors.ts';
import { uuidParams } from '../schemas.ts';
import { findUserByUuid } from '../users.ts';

export function registerBlockRoutes(app: FastifyInstance, ctx: Context) {
  app.get('/v1/blocks', async (request) => {
    const { rows } = await ctx.pool.query<{ uuid: string; name: string; created_at: Date }>(
      `select u.uuid, u.name, b.created_at from blocks b join users u on u.uuid = b.blocked
       where b.blocker = $1 order by b.created_at desc`,
      [currentUser(request).uuid],
    );
    return { blocked: rows.map((row) => ({ uuid: row.uuid, name: row.name, createdAt: row.created_at.toISOString() })) };
  });

  app.put<{ Params: { uuid: string } }>('/v1/blocks/:uuid', { schema: { params: uuidParams } }, async (request, reply) => {
    const me = currentUser(request).uuid;
    const target = request.params.uuid.toLowerCase();
    if (target === me) throw badRequest('You cannot block yourself');
    if (!(await findUserByUuid(ctx.pool, target))) throw notFound('No such player');
    await withTransaction(ctx.pool, async (client) => {
      await client.query(
        'insert into blocks (blocker, blocked, created_at) values ($1, $2, $3) on conflict do nothing',
        [me, target, ctx.now()],
      );
      await client.query(
        'delete from friendships where user_a = least($1::uuid, $2::uuid) and user_b = greatest($1::uuid, $2::uuid)',
        [me, target],
      );
      await client.query(
        `delete from friend_requests
         where (from_uuid = $1 and to_uuid = $2) or (from_uuid = $2 and to_uuid = $1)`,
        [me, target],
      );
    });
    return reply.status(204).send();
  });

  app.delete<{ Params: { uuid: string } }>('/v1/blocks/:uuid', { schema: { params: uuidParams } }, async (request, reply) => {
    const removed = await ctx.pool.query('delete from blocks where blocker = $1 and blocked = $2', [
      currentUser(request).uuid,
      request.params.uuid,
    ]);
    if (removed.rowCount === 0) throw notFound('This player is not blocked');
    return reply.status(204).send();
  });
}
