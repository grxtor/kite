import type pg from 'pg';
import { reportRetentionDays } from './chat.ts';
import { day } from './context.ts';
import { HttpError } from './errors.ts';

/*
 * Mutes and bans, set with `pnpm mod` (scripts/mod.ts) and enforced here. A mute stops sending
 * chat messages until it runs out; a ban stops chat, invites and new friend requests until it
 * is lifted. Neither stops signing in, reading or deleting the account, so a player can see why.
 */

type Queryable = Pick<pg.Pool, 'query'>;

export interface Sanction {
  /** Null when there is no mute or it ran out. */
  mutedUntil: Date | null;
  bannedAt: Date | null;
  banReason: string | null;
}

interface SanctionRow {
  muted_until: Date | null;
  banned_at: Date | null;
  ban_reason: string | null;
}

/** What is in force on `uuid` at `now`. */
export async function sanctionOf(db: Queryable, uuid: string, now: Date): Promise<Sanction> {
  const { rows } = await db.query<SanctionRow>('select muted_until, banned_at, ban_reason from sanctions where uuid = $1', [
    uuid,
  ]);
  const row = rows[0];
  return {
    mutedUntil: row?.muted_until && row.muted_until > now ? row.muted_until : null,
    bannedAt: row?.banned_at ?? null,
    banReason: row?.ban_reason ?? null,
  };
}

export function moderationJson(sanction: Sanction) {
  return {
    mutedUntil: sanction.mutedUntil?.toISOString() ?? null,
    banned: sanction.bannedAt ? { at: sanction.bannedAt.toISOString(), reason: sanction.banReason } : null,
  };
}

const bannedError = (reason: string | null) =>
  new HttpError(403, 'banned', 'This account may not chat, invite or send friend requests', null, { reason });

const mutedError = (until: Date) =>
  new HttpError(403, 'muted', `You can send messages again at ${until.toISOString()}`, null, { until: until.toISOString() });

/** Throws `banned` or `muted` when `uuid` may not send chat messages. */
export async function assertMaySendMessages(db: Queryable, uuid: string, now: Date): Promise<void> {
  const sanction = await sanctionOf(db, uuid, now);
  if (sanction.bannedAt) throw bannedError(sanction.banReason);
  if (sanction.mutedUntil) throw mutedError(sanction.mutedUntil);
}

/** Throws `banned` when `uuid` is banned; for invites and friend requests. */
export async function assertNotBanned(db: Queryable, uuid: string, now: Date): Promise<void> {
  const sanction = await sanctionOf(db, uuid, now);
  if (sanction.bannedAt) throw bannedError(sanction.banReason);
}

/** The players among `uuids` who are banned. */
export async function bannedAmong(db: Queryable, uuids: readonly string[]): Promise<string[]> {
  if (uuids.length === 0) return [];
  const { rows } = await db.query<{ uuid: string }>(
    'select uuid from sanctions where banned_at is not null and uuid = any($1::uuid[])',
    [uuids],
  );
  return rows.map((row) => row.uuid);
}

/**
 * Drops mutes that ran out with nothing else on the player, and log entries as old as the
 * reports purged with them, except those about a player still muted or banned.
 */
export async function purgeModeration(db: Queryable, now: Date): Promise<void> {
  await db.query('delete from sanctions where banned_at is null and (muted_until is null or muted_until <= $1)', [now]);
  await db.query('delete from moderation_log where at <= $1 and target not in (select uuid::text from sanctions)', [
    new Date(now.getTime() - reportRetentionDays * day),
  ]);
}
