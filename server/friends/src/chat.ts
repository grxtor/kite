import type pg from 'pg';
import { publishEvent } from './bus.ts';
import { day, hour, minute, type Context } from './context.ts';
import { withTransaction } from './db.ts';
import { badRequest, notFound } from './errors.ts';
import { cleanText } from './status.ts';
import { areFriends, hasBlockBetween, lockPair } from './users.ts';

export const messageRetentionDays = 30;
export const reportRetentionDays = 180;
/** In UTF-16 code units, the way Java and Qt count a string's length. */
export const maxMessageLength = 1000;
export const defaultPageSize = 50;
export const maxPageSize = 100;
const reportContextSize = 20;
// 18 digits always fit Postgres' bigint, so an id from a client never overflows it.
export const messageIdPattern = '^[1-9][0-9]{0,17}$';
const messageIdRegex = new RegExp(messageIdPattern);

export const reportReasons = ['spam', 'harassment', 'other'] as const;
export type ReportReason = (typeof reportReasons)[number];

type Queryable = Pick<pg.Pool, 'query'>;

export interface MessageJson {
  id: string;
  from: string;
  to: string;
  /** Null when the sender deleted their account. */
  text: string | null;
  createdAt: string;
  deleted: boolean;
}

interface MessageRow {
  id: string;
  sender_uuid: string;
  recipient_uuid: string;
  text: string | null;
  created_at: Date;
}

export interface Page {
  before?: string;
  after?: string;
  limit: number;
}

const messageColumns = 'm.id, m.sender_uuid, m.recipient_uuid, m.text, m.created_at';

/**
 * The messages between $1 (the reader) and $2 that $1 may see, newer than $3.
 * One side's rows are hidden once that side deleted their account.
 */
const conversation = `least(m.sender_uuid, m.recipient_uuid) = least($1::uuid, $2::uuid)
  and greatest(m.sender_uuid, m.recipient_uuid) = greatest($1::uuid, $2::uuid)
  and case when m.sender_uuid = $1::uuid then not m.hidden_for_sender else not m.hidden_for_recipient end
  and m.created_at > $3`;

function messageJson(row: MessageRow): MessageJson {
  return {
    id: row.id,
    from: row.sender_uuid,
    to: row.recipient_uuid,
    text: row.text,
    createdAt: row.created_at.toISOString(),
    deleted: row.text === null,
  };
}

export function retentionCutoff(now: Date): Date {
  return new Date(now.getTime() - messageRetentionDays * day);
}

export function isMessageId(value: unknown): value is string {
  return typeof value === 'string' && messageIdRegex.test(value);
}

function cleanMessage(raw: string): string {
  if (raw.length > maxMessageLength) throw badRequest(`A message is at most ${maxMessageLength} characters`);
  const text = cleanText(raw);
  if (text.length === 0) throw badRequest('The message is empty');
  return text;
}

/** Sends a message to a friend and pushes it to both players' sockets except `origin`. */
export async function sendMessage(ctx: Context, me: string, toRaw: string, rawText: string, origin?: string): Promise<MessageJson> {
  ctx.limit(`chat:${me}`, ctx.config.rateLimits.chatMessagesPerMinute, minute);
  const to = toRaw.toLowerCase();
  if (to === me) throw badRequest('That is you');
  const text = cleanMessage(rawText);

  const row = await withTransaction(ctx.pool, async (client) => {
    await lockPair(client, me, to);
    const blocks = await hasBlockBetween(client, me, to);
    if (blocks.byA || blocks.byB || !(await areFriends(client, me, to))) throw notFound('Not friends with this player');
    const { rows } = await client.query<MessageRow>(
      `insert into messages (sender_uuid, recipient_uuid, text, created_at) values ($1, $2, $3, $4)
       returning id, sender_uuid, recipient_uuid, text, created_at`,
      [me, to, text, ctx.now()],
    );
    return rows[0]!;
  });
  const message = messageJson(row);
  publishEvent(ctx.bus, to, { type: 'chat.message', message });
  publishEvent(ctx.bus, me, { type: 'chat.message', message }, origin);
  return message;
}

/** A page of the conversation, oldest first. A block either way hides it all. */
export async function history(ctx: Context, me: string, otherRaw: string, page: Page) {
  const other = otherRaw.toLowerCase();
  const blocks = await hasBlockBetween(ctx.pool, me, other);
  if (blocks.byA || blocks.byB) return { messages: [], more: false };

  const params: unknown[] = [me, other, retentionCutoff(ctx.now())];
  let where = conversation;
  let order = 'desc';
  if (page.after !== undefined) {
    params.push(page.after);
    where += ` and m.id > $${params.length}`;
    order = 'asc';
  } else if (page.before !== undefined) {
    params.push(page.before);
    where += ` and m.id < $${params.length}`;
  }
  params.push(page.limit + 1);
  const { rows } = await ctx.pool.query<MessageRow>(
    `select ${messageColumns} from messages m where ${where} order by m.id ${order} limit $${params.length}`,
    params,
  );
  const more = rows.length > page.limit;
  const shown = rows.slice(0, page.limit).map(messageJson);
  return { messages: order === 'desc' ? shown.reverse() : shown, more };
}

/** Moves the read marker for a conversation up to message `upTo`; it never moves back. */
export async function markRead(ctx: Context, me: string, otherRaw: string, upTo: string, origin?: string): Promise<string> {
  const other = otherRaw.toLowerCase();
  const blocks = await hasBlockBetween(ctx.pool, me, other);
  const found =
    !blocks.byA &&
    !blocks.byB &&
    (await ctx.pool.query(`select 1 from messages m where m.id = $4 and ${conversation}`, [me, other, retentionCutoff(ctx.now()), upTo]))
      .rowCount;
  if (!found) throw notFound('No such message');
  const { rows } = await ctx.pool.query<{ last_read_id: string }>(
    `insert into chat_reads (reader_uuid, other_uuid, last_read_id) values ($1, $2, $3)
     on conflict (reader_uuid, other_uuid) do update set last_read_id = greatest(chat_reads.last_read_id, excluded.last_read_id)
     returning last_read_id`,
    [me, other, upTo],
  );
  const marker = rows[0]!.last_read_id;
  publishEvent(ctx.bus, me, { type: 'chat.read', with: other, upTo: marker }, origin);
  return marker;
}

/** Unread messages per sender, for the friends list. */
export async function unreadCounts(db: Queryable, me: string, now: Date): Promise<Map<string, number>> {
  const { rows } = await db.query<{ uuid: string; count: number }>(
    `select m.sender_uuid as uuid, count(*)::int as count
     from messages m
     left join chat_reads r on r.reader_uuid = $1 and r.other_uuid = m.sender_uuid
     where m.recipient_uuid = $1 and not m.hidden_for_recipient and m.text is not null and m.created_at > $2
       and m.id > coalesce(r.last_read_id, 0)
     group by m.sender_uuid`,
    [me, retentionCutoff(now)],
  );
  return new Map(rows.map((row) => [row.uuid, row.count]));
}

/** Files a report on a message `other` sent to `me`, with a copy of it and what came before. */
export async function reportMessage(ctx: Context, me: string, otherRaw: string, id: string, reason: ReportReason): Promise<void> {
  ctx.limit(`chat-reports:${me}`, ctx.config.rateLimits.chatReportsPerHour, hour);
  const other = otherRaw.toLowerCase();
  const cutoff = retentionCutoff(ctx.now());
  // Allowed after a block too: blocking and then reporting is the usual order.
  const { rows: reported } = await ctx.pool.query<MessageRow>(
    `select ${messageColumns} from messages m
     where m.id = $1 and m.sender_uuid = $2 and m.recipient_uuid = $3 and m.text is not null
       and not m.hidden_for_recipient and m.created_at > $4`,
    [id, other, me, cutoff],
  );
  const message = reported[0];
  if (!message) throw notFound('No such message from this player');
  const { rows: before } = await ctx.pool.query<MessageRow>(
    `select ${messageColumns} from messages m where ${conversation} and m.id < $4 order by m.id desc limit $5`,
    [me, other, cutoff, id, reportContextSize],
  );
  const { rows: names } = await ctx.pool.query<{ name: string }>('select name from users where uuid = $1', [other]);
  const snapshot = {
    reported: { uuid: other, name: names[0]?.name ?? null },
    messages: [...before.reverse(), message].map(({ id: messageId, sender_uuid, text, created_at }) => ({
      id: messageId,
      from: sender_uuid,
      text,
      createdAt: created_at.toISOString(),
    })),
  };
  await ctx.pool.query(
    `insert into chat_reports (reporter_uuid, reported_uuid, message_id, reason, snapshot, created_at)
     values ($1, $2, $3, $4, $5, $6) on conflict (reporter_uuid, message_id) do nothing`,
    [me, other, id, reason, JSON.stringify(snapshot), ctx.now()],
  );
}

/**
 * Empties what `uuid` sent and hides what they received, for account deletion. Returns the
 * players who still see placeholders for their messages.
 */
export async function forgetMessages(client: pg.PoolClient, uuid: string): Promise<string[]> {
  const { rows } = await client.query<{ uuid: string }>(
    `select distinct recipient_uuid as uuid from messages
     where sender_uuid = $1 and text is not null and not hidden_for_recipient`,
    [uuid],
  );
  await client.query('update messages set text = null, hidden_for_sender = true where sender_uuid = $1', [uuid]);
  await client.query('update messages set hidden_for_recipient = true where recipient_uuid = $1', [uuid]);
  // Rows neither side can see any more.
  await client.query(
    `delete from messages where hidden_for_sender and hidden_for_recipient and (sender_uuid = $1 or recipient_uuid = $1)`,
    [uuid],
  );
  await client.query('delete from chat_reads where other_uuid = $1', [uuid]);
  return rows.map((row) => row.uuid);
}

/** Removes messages past retention and old reports. */
export async function purgeMessages(db: Queryable, now: Date): Promise<void> {
  await db.query('delete from messages where created_at <= $1', [retentionCutoff(now)]);
  await db.query('delete from chat_reports where created_at <= $1', [new Date(now.getTime() - reportRetentionDays * day)]);
}
