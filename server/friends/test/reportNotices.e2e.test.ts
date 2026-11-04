import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import { startServer } from './helpers.ts';
import type { Player, TestServer } from './helpers.ts';
import { startWebhookCatcher, type WebhookCatcher } from './infra.ts';

const send = (s: TestServer, from: Player, to: Player, text: string) =>
  s.api<{ id: string }>('POST', `/v1/chats/${to.uuid}/messages`, { token: from.token, body: { text } });
const fileReport = (s: TestServer, reporter: Player, other: Player, id: string, reason = 'harassment') =>
  s.api('POST', `/v1/chats/${other.uuid}/messages/${id}/report`, { token: reporter.token, body: { reason } });

describe('report notices', () => {
  let s: TestServer;
  let hook: WebhookCatcher;
  let closed = false;

  before(async () => {
    hook = await startWebhookCatcher();
    s = await startServer({ modWebhookUrl: hook.url });
  });

  after(async () => {
    if (!closed) await s.close();
    await hook.close();
  });

  test('go to the webhook with the id, reason, name and count, never what was written', async () => {
    const griefer = await s.newPlayer('Grief_er');
    const [first, second] = [await s.newPlayer('Reporter1'), await s.newPlayer('Reporter2')];
    await s.makeFriends(griefer, first);
    await s.makeFriends(griefer, second);
    const secret = 'my address is 12 Secret Street';
    await send(s, first, griefer, 'what came before');
    const nasty = await send(s, griefer, first, secret);
    const toSecond = await send(s, griefer, second, `${secret} again`);

    assert.equal((await fileReport(s, first, griefer, nasty.body.id)).status, 204);
    const [report] = await s.query<{ id: string }>('select id from chat_reports where reporter_uuid = $1', [first.uuid]);
    const [notice] = await hook.received(1);
    assert.deepEqual(notice, {
      content: `[Kite] Yeni şikâyet #${report!.id}: taciz, şikâyet edilen Grief\\_er, son 24 saatte bu kişiye 1 şikâyet`,
      allowed_mentions: { parse: [] },
    });

    // The same report again files nothing and says nothing; the next one waits for the minute.
    assert.equal((await fileReport(s, first, griefer, nasty.body.id, 'spam')).status, 204);
    assert.equal((await fileReport(s, second, griefer, toSecond.body.id, 'spam')).status, 204);
    await new Promise((resolve) => setTimeout(resolve, 200));
    assert.equal(hook.bodies.length, 1);

    // Closing sends what waits.
    await s.close();
    closed = true;
    const all = await hook.received(2);
    assert.match((all[1] as { content: string }).content, /^\[Kite\] Yeni şikâyet #\d+: spam, şikâyet edilen Grief\\_er, son 24 saatte bu kişiye 2 şikâyet$/);
    const posted = JSON.stringify(all);
    for (const leak of [secret, 'what came before', first.name, second.name, first.uuid, griefer.uuid]) {
      assert.ok(!posted.includes(leak), `the webhook got ${leak}`);
    }
  });
});
