import type { FastifyBaseLogger } from 'fastify';
import type { ReportReason } from './chat.ts';

/*
 * Tells the moderators' Discord channel (MOD_WEBHOOK_URL) about new chat reports. Only the
 * report's id, its reason, the reported player's name and a count leave the server; what was
 * written stays in the database, for `pnpm mod show`. Sending never makes a report wait or fail.
 */

/** At most one message per this long; reports in between go out together. */
export const noticeIntervalMs = 60_000;
export const noticeTimeoutMs = 5_000;
/** Lines in one message past which the rest is only counted (Discord takes 2000 characters). */
export const maxNoticeLines = 10;

export interface ReportNotice {
  id: string;
  reason: ReportReason;
  /** Null when the server does not know it; a UUID never goes in a notice. */
  reportedName: string | null;
  /** Reports against the player in the last 24 hours, this one included. */
  reportsLastDay: number;
}

export type PostJson = (url: string, body: unknown, signal: AbortSignal) => Promise<void>;

/** A webhook that answered with an error status; the only failure whose message is logged. */
export class WebhookStatusError extends Error {}

export async function postJson(url: string, body: unknown, signal: AbortSignal): Promise<void> {
  const response = await fetch(url, {
    method: 'POST',
    headers: { 'content-type': 'application/json' },
    body: JSON.stringify(body),
    signal,
  });
  await response.body?.cancel();
  if (!response.ok) throw new WebhookStatusError(`the webhook answered ${response.status}`);
}

export const reasonText: Record<ReportReason, string> = { spam: 'spam', harassment: 'taciz', other: 'diğer' };

// Discord reads Markdown, and a name like jeb_ must not start italics.
const escapeMarkdown = (text: string) => text.replace(/[\\*_~|`>]/g, '\\$&');

export function noticeLine(notice: ReportNotice): string {
  return (
    `[Kite] Yeni şikâyet #${notice.id}: ${reasonText[notice.reason]}, ` +
    `şikâyet edilen ${notice.reportedName === null ? 'bilinmeyen oyuncu' : escapeMarkdown(notice.reportedName)}, ` +
    `son 24 saatte bu kişiye ${notice.reportsLastDay} şikâyet`
  );
}

export function noticeText(batch: readonly ReportNotice[]): string {
  const lines = batch.slice(0, maxNoticeLines).map(noticeLine);
  const rest = batch.length - lines.length;
  if (rest > 0) lines.push(`[Kite] ve ${rest} şikâyet daha: pnpm mod list --open`);
  return lines.join('\n');
}

/** The first report goes out at once; the ones after it at most once a minute, together. */
export class ReportNotices {
  private readonly url: string | null;
  private readonly log: FastifyBaseLogger;
  private readonly post: PostJson;
  private pending: ReportNotice[] = [];
  private timer: NodeJS.Timeout | null = null;
  private lastSentAt = Number.NEGATIVE_INFINITY;
  private readonly sending = new Set<Promise<void>>();

  constructor(url: string | null, log: FastifyBaseLogger, post: PostJson = postJson) {
    this.url = url;
    this.log = log;
    this.post = post;
  }

  /** Queues a notice; nothing without a webhook. Never throws. */
  add(notice: ReportNotice): void {
    if (this.url === null) return;
    this.pending.push(notice);
    if (this.timer !== null) return;
    const wait = this.lastSentAt + noticeIntervalMs - Date.now();
    if (wait <= 0) {
      this.flush();
      return;
    }
    this.timer = setTimeout(() => this.flush(), wait);
    this.timer.unref();
  }

  /** Sends what is waiting at once and waits for every send; on shutdown. */
  async close(): Promise<void> {
    if (this.timer !== null) clearTimeout(this.timer);
    this.flush();
    await Promise.all(this.sending);
  }

  private flush(): void {
    this.timer = null;
    if (this.url === null || this.pending.length === 0) return;
    const batch = this.pending;
    this.pending = [];
    this.lastSentAt = Date.now();
    const body = { content: noticeText(batch), allowed_mentions: { parse: [] } };
    const send = this.post(this.url, body, AbortSignal.timeout(noticeTimeoutMs)).catch((error: unknown) => {
      // Other errors' messages can repeat the URL, which is a secret. The reports are in the
      // database either way.
      const why = error instanceof WebhookStatusError ? error.message : error instanceof Error ? error.name : 'unknown error';
      this.log.warn(`report notice not sent: ${why}`);
    });
    this.sending.add(send);
    void send.finally(() => this.sending.delete(send));
  }
}
