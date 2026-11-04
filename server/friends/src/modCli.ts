import { parseArgs } from 'node:util';
import type pg from 'pg';
import { messageIdPattern } from './chat.ts';
import { day, hour, minute } from './context.ts';
import { ban, closeReport, findReport, listReports, ModError, mute, unban, unmute, type Report } from './modStore.ts';
import { reasonText } from './reportNotices.ts';
import { uuidPattern } from './schemas.ts';
import { cleanText } from './status.ts';

/* `pnpm mod` (scripts/mod.ts): what it accepts and what it prints, apart from the process. */

export const usage = `pnpm mod list [--open] [--limit 50]
pnpm mod show <id>
pnpm mod mute <uuid> <duration>        (30m, 12h, 7d, 2w; replaces a running mute)
pnpm mod unmute <uuid>
pnpm mod ban <uuid> [--reason <text>]  (the player sees the reason: no names or report numbers)
pnpm mod unban <uuid>
pnpm mod resolve <id> [--note <text>]
pnpm mod dismiss <id> [--note <text>]`;

const defaultListLimit = 50;
const maxMuteDays = 365;
const maxNoteLength = 500;
const units: Record<string, number> = { m: minute, h: hour, d: day, w: 7 * day };
const uuidRegex = new RegExp(uuidPattern);
const idRegex = new RegExp(messageIdPattern);

const statusText = { open: 'açık', resolved: 'çözüldü', dismissed: 'reddedildi' } as const;
const deletedAccount = 'silinmiş hesap';

/** "30m", "12h", "7d" or "2w" in milliseconds, up to a year. */
export function parseDuration(text: string): number {
  const match = /^([1-9][0-9]{0,5})([mhdw])$/.exec(text);
  const ms = match ? Number(match[1]) * units[match[2]!]! : 0;
  if (ms === 0 || ms > maxMuteDays * day) throw new ModError(`süre ${text} anlaşılmadı: 30m, 12h, 7d, 2w gibi, en çok ${maxMuteDays}d`);
  return ms;
}

function uuidArg(value: string | undefined): string {
  if (value === undefined || !uuidRegex.test(value)) throw new ModError('tireli bir UUID gerekiyor');
  return value.toLowerCase();
}

function idArg(value: string | undefined): string {
  const id = value?.replace(/^#/, '');
  if (id === undefined || !idRegex.test(id)) throw new ModError('bir şikâyet numarası gerekiyor');
  return id;
}

function noteArg(value: string | undefined, name: string): string | null {
  if (value === undefined) return null;
  const note = cleanText(value);
  if (note.length === 0 || note.length > maxNoteLength) throw new ModError(`--${name} 1 ile ${maxNoteLength} karakter arası olmalı`);
  return note;
}

const timeText = (date: Date) => date.toISOString().slice(0, 16).replace('T', ' ');
const reporterText = (report: Report) => (report.reporterUuid === null ? deletedAccount : (report.reporterName ?? report.reporterUuid));
const reportedText = (report: Report) => report.reportedName ?? report.reportedUuid;

export function reportLine(report: Report): string {
  return [
    `#${report.id}`,
    timeText(report.createdAt),
    reasonText[report.reason],
    `şikâyet eden ${reporterText(report)}`,
    `şikâyet edilen ${reportedText(report)}`,
    statusText[report.status],
  ].join('  ');
}

const sideText = { reporter: 'şikâyet eden  ', reported: 'şikâyet edilen' } as const;

/** The report with the copy of the conversation; `>>` marks the reported message. */
export function reportDetail(report: Report): string {
  const reporter = report.reporterUuid === null ? deletedAccount : `${report.reporterName ?? '?'} (${report.reporterUuid})`;
  const lines = [
    `#${report.id}  ${statusText[report.status]}${report.closedAt ? ` ${timeText(report.closedAt)}` : ''}`,
    `Zaman: ${timeText(report.createdAt)} UTC`,
    `Sebep: ${reasonText[report.reason]}`,
    `Şikâyet eden: ${reporter}`,
    `Şikâyet edilen: ${report.reportedName ?? '?'} (${report.reportedUuid})`,
    ...(report.resolutionNote ? [`Not: ${report.resolutionNote}`] : []),
    'Mesajlar:',
    ...report.messages.map((message) => {
      const mark = message.id === report.messageId ? '>>' : '  ';
      const text = message.text ?? '(silindi)';
      return `${mark} ${timeText(new Date(message.createdAt))}  [${sideText[message.from]}]  ${text}`;
    }),
  ];
  return lines.join('\n');
}

const flagsOf: Record<string, readonly string[]> = {
  list: ['open', 'limit'],
  ban: ['reason'],
  resolve: ['note'],
  dismiss: ['note'],
};

/** Runs one `pnpm mod` command and returns what to print. Refusals are ModErrors. */
export async function runMod(pool: pg.Pool, argv: readonly string[], now: Date): Promise<string> {
  let parsed;
  try {
    parsed = parseArgs({
      args: [...argv],
      options: { open: { type: 'boolean' }, limit: { type: 'string' }, reason: { type: 'string' }, note: { type: 'string' } },
      allowPositionals: true,
      strict: true,
    });
  } catch (error) {
    throw new ModError(`${error instanceof Error ? error.message : error}\n${usage}`);
  }
  const { values, positionals } = parsed;
  const [command, first, second, ...extra] = positionals;
  const allowed = flagsOf[command ?? ''] ?? [];
  const stray = Object.keys(values).find((flag) => !allowed.includes(flag));
  if (stray) throw new ModError(`--${stray} ${command ?? ''} ile kullanılmaz\n${usage}`);
  if (extra.length > 0 || (second !== undefined && command !== 'mute')) throw new ModError(`fazla argüman\n${usage}`);

  switch (command) {
    case 'list': {
      const limit = values.limit === undefined ? defaultListLimit : Number(values.limit);
      if (!Number.isInteger(limit) || limit <= 0) throw new ModError('--limit pozitif bir tam sayı olmalı');
      if (first !== undefined) throw new ModError(`fazla argüman\n${usage}`);
      const reports = await listReports(pool, values.open === true, limit);
      if (reports.length === 0) return values.open ? 'Açık şikâyet yok.' : 'Şikâyet yok.';
      return reports.map(reportLine).join('\n');
    }
    case 'show':
      return reportDetail(await findReport(pool, idArg(first)));
    case 'mute': {
      const uuid = uuidArg(first);
      if (second === undefined) throw new ModError(`süre gerekiyor\n${usage}`);
      const until = new Date(now.getTime() + parseDuration(second));
      await mute(pool, uuid, until, now);
      return `${uuid} ${timeText(until)} UTC'ye kadar susturuldu`;
    }
    case 'unmute': {
      const uuid = uuidArg(first);
      await unmute(pool, uuid, now);
      return `${uuid} artık susturulmuş değil`;
    }
    case 'ban': {
      const uuid = uuidArg(first);
      await ban(pool, uuid, noteArg(values.reason, 'reason'), now);
      return `${uuid} yasaklandı; açık soketleri 30 saniye içinde 4403 ile kapanır`;
    }
    case 'unban': {
      const uuid = uuidArg(first);
      await unban(pool, uuid, now);
      return `${uuid} artık yasaklı değil`;
    }
    case 'resolve':
    case 'dismiss': {
      const id = idArg(first);
      await closeReport(pool, id, command === 'resolve' ? 'resolved' : 'dismissed', noteArg(values.note, 'note'), now);
      return `#${id} ${command === 'resolve' ? 'çözüldü' : 'reddedildi'}`;
    }
    default:
      throw new ModError(usage);
  }
}
