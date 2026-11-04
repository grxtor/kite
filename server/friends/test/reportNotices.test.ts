import assert from 'node:assert/strict';
import { afterEach, beforeEach, describe, mock, test } from 'node:test';
import type { FastifyBaseLogger } from 'fastify';
import {
  maxNoticeLines,
  noticeIntervalMs,
  noticeLine,
  noticeText,
  ReportNotices,
  WebhookStatusError,
  type ReportNotice,
} from '../src/reportNotices.ts';

const url = 'https://discord.test/api/webhooks/1/s3cret';

function notice(id: number, extra: Partial<ReportNotice> = {}): ReportNotice {
  return { id: String(id), reason: 'harassment', reportedName: 'Griefer', reportsLastDay: 1, ...extra };
}

interface Sent {
  url: string;
  at: number;
  body: { content: string; allowed_mentions: unknown };
}

function setup(fail?: (to: string) => Error) {
  const sent: Sent[] = [];
  const warnings: string[] = [];
  const log = { warn: (message: string) => warnings.push(message) } as unknown as FastifyBaseLogger;
  const notices = new ReportNotices(url, log, async (to, body) => {
    sent.push({ url: to, at: Date.now(), body: body as Sent['body'] });
    if (fail) throw fail(to);
  });
  return { sent, warnings, notices };
}

describe('report notice text', () => {
  test('names the report, its reason, the player and the count of the last day', () => {
    assert.equal(
      noticeLine(notice(12, { reason: 'spam', reportsLastDay: 3 })),
      '[Kite] Yeni şikâyet #12: spam, şikâyet edilen Griefer, son 24 saatte bu kişiye 3 şikâyet',
    );
    assert.match(noticeLine(notice(1)), /: taciz,/);
    assert.match(noticeLine(notice(1, { reason: 'other' })), /: diğer,/);
  });

  test('escapes Markdown in names, and has no UUID for an unknown player', () => {
    assert.match(noticeLine(notice(1, { reportedName: 'jeb_' })), /şikâyet edilen jeb\\_,/);
    assert.match(noticeLine(notice(1, { reportedName: null })), /şikâyet edilen bilinmeyen oyuncu,/);
  });

  test(`lists at most ${maxNoticeLines} reports and counts the rest`, () => {
    const text = noticeText(Array.from({ length: maxNoticeLines + 3 }, (_, i) => notice(i + 1)));
    const lines = text.split('\n');
    assert.equal(lines.length, maxNoticeLines + 1);
    assert.equal(lines.at(-1), '[Kite] ve 3 şikâyet daha: pnpm mod list --open');
    assert.ok(text.length < 2000);
  });
});

describe('report notices', () => {
  beforeEach(() => mock.timers.enable({ apis: ['setTimeout', 'Date'], now: Date.parse('2026-11-04T12:00:00Z') }));
  afterEach(() => mock.timers.reset());

  test('the first report goes out at once, with no mentions allowed', () => {
    const { sent, notices } = setup();
    notices.add(notice(1));
    assert.equal(sent.length, 1);
    assert.equal(sent[0]!.url, url);
    assert.deepEqual(sent[0]!.body, { content: noticeLine(notice(1)), allowed_mentions: { parse: [] } });
  });

  test('reports within a minute of a notice go out together a minute after it', () => {
    const { sent, notices } = setup();
    notices.add(notice(1));
    mock.timers.tick(10_000);
    notices.add(notice(2));
    mock.timers.tick(5_000);
    notices.add(notice(3));
    assert.equal(sent.length, 1);
    mock.timers.tick(noticeIntervalMs - 15_000 - 1);
    assert.equal(sent.length, 1);
    mock.timers.tick(1);
    assert.equal(sent.length, 2);
    assert.equal(sent[1]!.body.content, noticeText([notice(2), notice(3)]));
  });

  test('a flood is one message a minute and loses no report', () => {
    const { sent, notices } = setup();
    for (let second = 0; second < 5 * 60; second += 1) {
      notices.add(notice(second + 1));
      mock.timers.tick(1000);
    }
    mock.timers.tick(noticeIntervalMs);
    // At 0, 60, 120, 180, 240 and 300 seconds.
    assert.equal(sent.length, 6);
    for (let i = 1; i < sent.length; i += 1) assert.ok(sent[i]!.at - sent[i - 1]!.at >= noticeIntervalMs);
    const reported = sent.flatMap((one) => one.body.content.split('\n')).filter((line) => line.includes('Yeni şikâyet'));
    const counted = sent.reduce((sum, one) => sum + Number(/ve (\d+) şikâyet daha/.exec(one.body.content)?.[1] ?? 0), 0);
    assert.equal(reported.length + counted, 300);
  });

  test('a report after a quiet minute goes out at once again', () => {
    const { sent, notices } = setup();
    notices.add(notice(1));
    mock.timers.tick(noticeIntervalMs);
    notices.add(notice(2));
    assert.equal(sent.length, 2);
  });

  test('nothing is sent without a webhook', () => {
    let calls = 0;
    const notices = new ReportNotices(null, {} as FastifyBaseLogger, async () => {
      calls += 1;
    });
    notices.add(notice(1));
    mock.timers.tick(noticeIntervalMs);
    assert.equal(calls, 0);
  });

  test('a failed send is logged without the URL, even when the error repeats it, and does not throw', async () => {
    const { warnings, notices } = setup((to) => new TypeError(`Request cannot be constructed from ${to}`));
    assert.doesNotThrow(() => notices.add(notice(1)));
    await notices.close();
    assert.deepEqual(warnings, ['report notice not sent: TypeError']);
  });

  test('a webhook error status is logged', async () => {
    const { warnings, notices } = setup(() => new WebhookStatusError('the webhook answered 404'));
    notices.add(notice(1));
    await notices.close();
    assert.deepEqual(warnings, ['report notice not sent: the webhook answered 404']);
  });

  test('closing sends what is waiting', async () => {
    const { sent, notices } = setup();
    notices.add(notice(1));
    notices.add(notice(2));
    await notices.close();
    assert.equal(sent.length, 2);
    assert.equal(sent[1]!.body.content, noticeLine(notice(2)));
  });
});
