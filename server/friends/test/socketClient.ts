import assert from 'node:assert/strict';
import { randomBytes } from 'node:crypto';
import { request } from 'node:http';
import type { Socket } from 'node:net';

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
