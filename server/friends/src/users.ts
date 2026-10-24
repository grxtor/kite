import type pg from 'pg';

export interface UserRef {
  uuid: string;
  name: string;
}

type Queryable = Pick<pg.Pool, 'query'>;

/** The current holder of a Minecraft name. Names move between accounts, so the latest sign-in wins. */
export async function findUserByName(db: Queryable, name: string): Promise<UserRef | null> {
  const { rows } = await db.query<UserRef>(
    'select uuid, name from users where name_lower = lower($1) order by name_seen_at desc limit 1',
    [name],
  );
  return rows[0] ?? null;
}

export async function findUserByUuid(db: Queryable, uuid: string): Promise<UserRef | null> {
  const { rows } = await db.query<UserRef>('select uuid, name from users where uuid = $1', [uuid]);
  return rows[0] ?? null;
}

export async function hasBlockBetween(db: Queryable, a: string, b: string): Promise<{ byA: boolean; byB: boolean }> {
  const { rows } = await db.query<{ blocker: string }>(
    'select blocker from blocks where (blocker = $1 and blocked = $2) or (blocker = $2 and blocked = $1)',
    [a, b],
  );
  return { byA: rows.some((row) => row.blocker === a), byB: rows.some((row) => row.blocker === b) };
}

export async function areFriends(db: Queryable, a: string, b: string): Promise<boolean> {
  const { rowCount } = await db.query(
    'select 1 from friendships where user_a = least($1::uuid, $2::uuid) and user_b = greatest($1::uuid, $2::uuid)',
    [a, b],
  );
  return (rowCount ?? 0) > 0;
}
