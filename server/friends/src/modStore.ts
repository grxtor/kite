import type pg from 'pg';
import type { ReportReason } from './chat.ts';
import { withTransaction } from './db.ts';

/*
 * What `pnpm mod` reads and changes. Every change is written to moderation_log in the same
 * transaction. The CLI is the only actor so far.
 */

type Queryable = Pick<pg.Pool, 'query'>;

export const actor = 'cli';

export type ReportStatus = 'open' | 'resolved' | 'dismissed';
export type ModAction = 'mute' | 'unmute' | 'ban' | 'unban' | 'resolve' | 'dismiss';

/** A refused action: the CLI prints the message and exits with 1. */
export class ModError extends Error {}

export interface SnapshotMessage {
  id: string;
  from: 'reporter' | 'reported';
  text: string | null;
  createdAt: string;
}

export interface Report {
  id: string;
  createdAt: Date;
  reason: ReportReason;
  /** Null once the reporter deleted their account. */
  reporterUuid: string | null;
  reporterName: string | null;
  reportedUuid: string;
  reportedName: string | null;
  messageId: string;
  status: ReportStatus;
  closedAt: Date | null;
  resolutionNote: string | null;
  messages: SnapshotMessage[];
}

interface ReportRow {
  id: string;
  created_at: Date;
  reason: ReportReason;
  reporter_uuid: string | null;
  reporter_name: string | null;
  reported_uuid: string;
  reported_name: string | null;
  message_id: string;
  status: ReportStatus;
  closed_at: Date | null;
  resolution_note: string | null;
  snapshot: { messages: SnapshotMessage[] };
}

// The name the report kept, else the one the player has now.
const reportSelect = `select r.id, r.created_at, r.reason, r.reporter_uuid, reporter.name as reporter_name,
    r.reported_uuid, coalesce(r.snapshot->'reported'->>'name', reported.name) as reported_name, r.message_id,
    r.status, r.closed_at, r.resolution_note, r.snapshot
  from chat_reports r
  left join users reporter on reporter.uuid = r.reporter_uuid
  left join users reported on reported.uuid = r.reported_uuid`;

function reportOf(row: ReportRow): Report {
  return {
    id: row.id,
    createdAt: row.created_at,
    reason: row.reason,
    reporterUuid: row.reporter_uuid,
    reporterName: row.reporter_name,
    reportedUuid: row.reported_uuid,
    reportedName: row.reported_name,
    messageId: row.message_id,
    status: row.status,
    closedAt: row.closed_at,
    resolutionNote: row.resolution_note,
    messages: row.snapshot.messages,
  };
}

/** Newest first. */
export async function listReports(db: Queryable, openOnly: boolean, limit: number): Promise<Report[]> {
  const { rows } = await db.query<ReportRow>(
    `${reportSelect} where ($1::boolean is false or r.status = 'open') order by r.id desc limit $2`,
    [openOnly, limit],
  );
  return rows.map(reportOf);
}

export async function findReport(db: Queryable, id: string): Promise<Report> {
  const { rows } = await db.query<ReportRow>(`${reportSelect} where r.id = $1`, [id]);
  if (!rows[0]) throw new ModError(`#${id} diye bir şikâyet yok`);
  return reportOf(rows[0]);
}

async function logAction(client: pg.PoolClient, action: ModAction, target: string, note: string | null, now: Date) {
  await client.query('insert into moderation_log (actor, at, action, target, note) values ($1, $2, $3, $4, $5)', [
    actor,
    now,
    action,
    target,
    note,
  ]);
}

// A row with neither a running mute nor a ban says nothing.
async function dropEmptySanction(client: pg.PoolClient, uuid: string, now: Date) {
  await client.query(
    'delete from sanctions where uuid = $1 and banned_at is null and (muted_until is null or muted_until <= $2)',
    [uuid, now],
  );
}

export async function mute(pool: pg.Pool, uuid: string, until: Date, now: Date): Promise<void> {
  await withTransaction(pool, async (client) => {
    await client.query(
      `insert into sanctions (uuid, muted_until) values ($1, $2)
       on conflict (uuid) do update set muted_until = excluded.muted_until`,
      [uuid, until],
    );
    await logAction(client, 'mute', uuid, `until ${until.toISOString()}`, now);
  });
}

export async function unmute(pool: pg.Pool, uuid: string, now: Date): Promise<void> {
  await withTransaction(pool, async (client) => {
    const { rowCount } = await client.query(
      'update sanctions set muted_until = null where uuid = $1 and muted_until > $2',
      [uuid, now],
    );
    if (!rowCount) throw new ModError(`${uuid} susturulmuş değil`);
    await dropEmptySanction(client, uuid, now);
    await logAction(client, 'unmute', uuid, null, now);
  });
}

export async function ban(pool: pg.Pool, uuid: string, reason: string | null, now: Date): Promise<void> {
  await withTransaction(pool, async (client) => {
    const { rowCount } = await client.query(
      `insert into sanctions (uuid, banned_at, ban_reason) values ($1, $2, $3)
       on conflict (uuid) do update set banned_at = excluded.banned_at, ban_reason = excluded.ban_reason
       where sanctions.banned_at is null`,
      [uuid, now, reason],
    );
    if (!rowCount) throw new ModError(`${uuid} zaten yasaklı (sebebi değiştirmek için önce unban)`);
    await logAction(client, 'ban', uuid, reason, now);
  });
}

export async function unban(pool: pg.Pool, uuid: string, now: Date): Promise<void> {
  await withTransaction(pool, async (client) => {
    const { rowCount } = await client.query(
      'update sanctions set banned_at = null, ban_reason = null where uuid = $1 and banned_at is not null',
      [uuid],
    );
    if (!rowCount) throw new ModError(`${uuid} yasaklı değil`);
    await dropEmptySanction(client, uuid, now);
    await logAction(client, 'unban', uuid, null, now);
  });
}

/** Closes an open report as resolved or dismissed. */
export async function closeReport(
  pool: pg.Pool,
  id: string,
  status: Exclude<ReportStatus, 'open'>,
  note: string | null,
  now: Date,
): Promise<void> {
  await withTransaction(pool, async (client) => {
    const { rowCount } = await client.query(
      `update chat_reports set status = $2, closed_at = $3, resolution_note = $4 where id = $1 and status = 'open'`,
      [id, status, now, note],
    );
    if (!rowCount) {
      // Read after the update, so a moderator who lost a race sees what the other one chose.
      const { rows } = await client.query<{ status: ReportStatus }>('select status from chat_reports where id = $1', [id]);
      if (!rows[0]) throw new ModError(`#${id} diye bir şikâyet yok`);
      throw new ModError(`#${id} zaten kapalı (${rows[0].status})`);
    }
    await logAction(client, status === 'resolved' ? 'resolve' : 'dismiss', id, note, now);
  });
}
