import type pg from 'pg';
import { publishEvent, type ServerEvent } from './bus.ts';
import type { Context } from './context.ts';
import { presenceColumns, statusView, type PresenceRow } from './status.ts';

type Queryable = Pick<pg.Pool, 'query'>;

/** The friends of `uuid` with no block either way. */
export async function friendUuids(db: Queryable, uuid: string): Promise<string[]> {
  const { rows } = await db.query<{ uuid: string }>(
    `select case when f.user_a = $1 then f.user_b else f.user_a end as uuid
     from friendships f
     where (f.user_a = $1 or f.user_b = $1)
       and not exists (select 1 from blocks b
                       where (b.blocker = f.user_a and b.blocked = f.user_b) or (b.blocker = f.user_b and b.blocked = f.user_a))`,
    [uuid],
  );
  return rows.map((row) => row.uuid);
}

/** Publishes events after a change is saved. A failure is logged, not thrown: the change stands. */
export async function notify(ctx: Context, what: string, work: () => Promise<void> | void): Promise<void> {
  try {
    await work();
  } catch (error) {
    ctx.log.error(error, `pushing ${what} failed`);
  }
}

/** Tells `uuid`'s friends how their status looks now. */
export function pushStatus(ctx: Context, uuid: string): Promise<void> {
  return notify(ctx, 'a status', async () => {
    const { rows } = await ctx.pool.query<PresenceRow>(
      `select ${presenceColumns} from users u
       left join presence p on p.user_uuid = u.uuid and p.expires_at > $2
       where u.uuid = $1`,
      [uuid, ctx.now()],
    );
    const row = rows[0];
    if (!row) return;
    const event: ServerEvent = { type: 'friend.status', uuid, status: statusView(row) };
    for (const friend of await friendUuids(ctx.pool, uuid)) publishEvent(ctx.bus, friend, event);
  });
}

/** Tells both players they are no longer friends, so their lists drop each other. */
export function pushFriendRemoved(ctx: Context, a: string, b: string): void {
  publishEvent(ctx.bus, a, { type: 'friend.removed', uuid: b });
  publishEvent(ctx.bus, b, { type: 'friend.removed', uuid: a });
}
