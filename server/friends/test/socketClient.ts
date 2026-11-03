import assert from 'node:assert/strict';
import { randomBytes } from 'node:crypto';
import { request } from 'node:http';
import { connect, type Socket } from 'node:net';

export interface ServerFrame {
  type: string;
  [key: string]: unknown;
}

export interface CloseInfo {
  code: number;
  reason: string;
}

interface Waiter {
  type: string;
  resolve: (frame: ServerFrame) => void;
  reject: (error: Error) => void;
  timer: NodeJS.Timeout;
}

// Node's WebSocket takes headers, which the DOM typings do not know about.
const NodeWebSocket = WebSocket as unknown as new (url: string, init: { headers: Record<string, string> }) => WebSocket;

/** A WebSocket client that queues what the server sends, so a test can wait for one frame type. */
export class TestSocket {
  readonly closed: Promise<CloseInfo>;
  private readonly ws: WebSocket;
  private readonly queue: ServerFrame[] = [];
  private readonly waiters: Waiter[] = [];

  constructor(url: string, token: string | undefined) {
    const headers: Record<string, string> = token === undefined ? {} : { authorization: `Bearer ${token}` };
    this.ws = new NodeWebSocket(url, { headers });
    this.ws.onmessage = (event) => this.receive(JSON.parse(String(event.data)) as ServerFrame);
    this.ws.onerror = () => {};
    this.closed = new Promise((resolve) => {
      this.ws.onclose = (event) => resolve({ code: event.code, reason: event.reason });
    });
  }

  private receive(frame: ServerFrame): void {
    const index = this.waiters.findIndex((waiter) => waiter.type === frame.type);
    const waiter = index === -1 ? undefined : this.waiters.splice(index, 1)[0];
    if (!waiter) {
      this.queue.push(frame);
      return;
    }
    clearTimeout(waiter.timer);
    waiter.resolve(frame);
  }

  /** The next frame of `type` not yet taken, waiting for it if needed. */
  next(type: string, timeoutMs = 2000): Promise<ServerFrame> {
    const index = this.queue.findIndex((frame) => frame.type === type);
    if (index !== -1) return Promise.resolve(this.queue.splice(index, 1)[0]!);
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.waiters.splice(this.waiters.indexOf(waiter), 1);
        reject(new Error(`no ${type} frame within ${timeoutMs} ms`));
      }, timeoutMs);
      const waiter: Waiter = { type, resolve, reject, timer };
      this.waiters.push(waiter);
    });
  }

  /** Fails if a frame of `type` arrives within `ms`. */
  async none(type: string, ms = 150): Promise<void> {
    await new Promise((resolve) => setTimeout(resolve, ms));
    assert.equal(
      this.queue.find((frame) => frame.type === type),
      undefined,
      `unexpected ${type} frame`,
    );
  }

  send(frame: unknown): void {
    this.ws.send(JSON.stringify(frame));
  }

  sendRaw(data: string | Uint8Array<ArrayBuffer>): void {
    this.ws.send(data);
  }

  close(): void {
    this.ws.close();
  }
}

function upgradeHeaders(extra: Record<string, string>): Record<string, string> {
  return {
    connection: 'Upgrade',
    upgrade: 'websocket',
    'sec-websocket-version': '13',
    'sec-websocket-key': randomBytes(16).toString('base64'),
    ...extra,
  };
}

/** The status the server answers an upgrade request with: 101, or the HTTP error it sent instead. */
export function upgradeStatus(baseUrl: string, headers: Record<string, string>): Promise<number> {
  return new Promise((resolve, reject) => {
    const req = request(`${baseUrl}/v1/ws`, { headers: upgradeHeaders(headers) });
    req.on('response', (res) => {
      res.resume();
      resolve(res.statusCode ?? 0);
    });
    req.on('upgrade', (res, socket: Socket) => {
      socket.destroy();
      resolve(res.statusCode ?? 0);
    });
    req.on('error', reject);
    req.end();
  });
}

/** Opens a socket that never answers pings, and resolves `closed` when the server drops it. */
export function silentSocket(baseUrl: string, token: string): Promise<{ closed: Promise<void> }> {
  return new Promise((resolve, reject) => {
    const req = request(`${baseUrl}/v1/ws`, { headers: upgradeHeaders({ authorization: `Bearer ${token}` }) });
    req.on('upgrade', (_res, socket: Socket) => {
      socket.on('data', () => {});
      resolve({ closed: new Promise((done) => socket.on('close', () => done())) });
    });
    req.on('response', (res) => reject(new Error(`upgrade refused with ${res.statusCode}`)));
    req.on('error', reject);
    req.end();
  });
}

/**
 * Sends an upgrade request over a bare TCP socket and resolves with the status line's code once
 * the server has closed the connection, so a refused upgrade that is left open times out.
 */
export function refusedUpgradeCloses(baseUrl: string, headers: Record<string, string>): Promise<number> {
  const url = new URL(baseUrl);
  return new Promise((resolve, reject) => {
    const socket = connect(Number(url.port), url.hostname);
    let received = '';
    socket.on('data', (chunk) => {
      received += chunk.toString('latin1');
    });
    socket.on('close', () => resolve(Number(/^HTTP\/1\.1 (\d+)/.exec(received)?.[1] ?? 0)));
    socket.on('error', reject);
    const lines = Object.entries({ host: url.host, ...upgradeHeaders(headers) }).map(([key, value]) => `${key}: ${value}`);
    socket.write(`GET /v1/ws HTTP/1.1\r\n${lines.join('\r\n')}\r\n\r\n`);
  });
}

function maskedTextFrame(text: string): Buffer {
  const payload = Buffer.from(text, 'utf8');
  assert.ok(payload.length < 65_536);
  const header =
    payload.length < 126
      ? Buffer.from([0x81, 0x80 | payload.length])
      : Buffer.from([0x81, 0x80 | 126, payload.length >> 8, payload.length & 0xff]);
  const mask = randomBytes(4);
  const masked = Buffer.from(payload.map((byte, i) => byte ^ mask[i % 4]!));
  return Buffer.concat([header, mask, masked]);
}

export interface RawSocket {
  /** The code of the first close frame the server sends. */
  closeCode: Promise<number>;
  sendText: (text: string) => void;
  destroy: () => void;
}

/** A socket that speaks frames by hand and never answers pings or the server's close frame. */
export function rawSocket(baseUrl: string, token: string): Promise<RawSocket> {
  return new Promise((resolve, reject) => {
    const req = request(`${baseUrl}/v1/ws`, { headers: upgradeHeaders({ authorization: `Bearer ${token}` }) });
    req.on('upgrade', (_res, socket: Socket, head: Buffer) => {
      let buffered = head;
      let onClose: (code: number) => void = () => {};
      const closeCode = new Promise<number>((done) => {
        onClose = done;
      });
      // Server frames are unmasked and small here: two header bytes, or four with a 16-bit length.
      const drain = () => {
        while (buffered.length >= 2) {
          const opcode = buffered[0]! & 0x0f;
          const short = buffered[1]! & 0x7f;
          const start = short === 126 ? 4 : 2;
          if (buffered.length < start) return;
          const length = short === 126 ? buffered.readUInt16BE(2) : short;
          if (buffered.length < start + length) return;
          if (opcode === 0x8) onClose(buffered.readUInt16BE(start));
          buffered = buffered.subarray(start + length);
        }
      };
      socket.on('data', (chunk: Buffer) => {
        buffered = Buffer.concat([buffered, chunk]);
        drain();
      });
      drain();
      resolve({ closeCode, sendText: (text) => socket.write(maskedTextFrame(text)), destroy: () => socket.destroy() });
    });
    req.on('response', (res) => reject(new Error(`upgrade refused with ${res.statusCode}`)));
    req.on('error', reject);
    req.end();
  });
}
