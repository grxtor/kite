import { randomUUID } from 'node:crypto';
import type { FastifyInstance } from 'fastify';
import { isMessageId, markRead, sendMessage } from '../chat.ts';
import { currentUser, minute, type AuthUser, type Context } from '../context.ts';
import { HttpError } from '../errors.ts';
import { uuidPattern } from '../schemas.ts';

/** Largest frame a client may send; `ws` closes the socket with 1009 past it. */
export const maxFrameBytes = 16 * 1024;
export const maxConnectionsPerUser = 5;
// A client that does not read its events is cut off instead of growing the server's memory.
const maxBufferedBytes = 1024 * 1024;

export const closeCodes = {
  textOnly: 1003,
  tooManyConnections: 4001,
  sessionEnded: 4401,
} as const;

const openState = 1;
const uuidRegex = new RegExp(uuidPattern);
const refPattern = /^[A-Za-z0-9_-]{1,64}$/;

/** The part of a `ws` WebSocket this module uses. */
interface Socket {
  readonly readyState: number;
  readonly bufferedAmount: number;
  send(data: string): void;
  ping(): void;
  close(code?: number, reason?: string): void;
  terminate(): void;
  on(event: 'message', listener: (data: Buffer, isBinary: boolean) => void): void;
  on(event: 'pong' | 'close', listener: () => void): void;
}

interface Connection {
  id: string;
  user: AuthUser;
  tokenHash: string;
  socket: Socket;
  alive: boolean;
  /** Set once the session behind the socket is gone; nothing it sends is handled after that. */
  ended: boolean;
  rateLimited: boolean;
  queue: Promise<void>;
  unsubscribe: () => void;
}

type ClientFrame =
  | { type: 'chat.send'; ref?: string; to: string; text: string }
  | { type: 'chat.read'; ref?: string; with: string; upTo: string };

/** A frame that cannot be handled. `ref` is the frame's own when it had a valid one. */
class FrameError extends Error {
  readonly ref: string | undefined;

  constructor(message: string, ref?: string) {
    super(message);
    this.ref = ref;
  }
}

function parseFrame(text: string): ClientFrame {
  let frame: unknown;
  try {
    frame = JSON.parse(text);
  } catch {
    throw new FrameError('Not JSON');
  }
  if (typeof frame !== 'object' || frame === null || Array.isArray(frame)) throw new FrameError('Not an object');
  const raw = frame as Record<string, unknown>;
  if (raw.ref !== undefined && (typeof raw.ref !== 'string' || !refPattern.test(raw.ref))) throw new FrameError('Bad ref');
  const ref = raw.ref as string | undefined;
  const fields =
    raw.type === 'chat.send' ? ['type', 'ref', 'to', 'text'] : raw.type === 'chat.read' ? ['type', 'ref', 'with', 'upTo'] : null;
  if (!fields) throw new FrameError('Unknown type', ref);
  if (Object.keys(raw).some((key) => !fields.includes(key))) throw new FrameError('A field is not allowed', ref);
  if (raw.type === 'chat.send') {
    if (typeof raw.to !== 'string' || !uuidRegex.test(raw.to)) throw new FrameError('to is not a uuid', ref);
    if (typeof raw.text !== 'string') throw new FrameError('text is not a string', ref);
    return { type: 'chat.send', ref, to: raw.to, text: raw.text };
  }
  if (typeof raw.with !== 'string' || !uuidRegex.test(raw.with)) throw new FrameError('with is not a uuid', ref);
  if (!isMessageId(raw.upTo)) throw new FrameError('upTo is not a message id', ref);
  return { type: 'chat.read', ref, with: raw.with, upTo: raw.upTo };
}

export function registerSocketRoute(app: FastifyInstance, ctx: Context) {
  // Per player, oldest first.
  const byUser = new Map<string, Connection[]>();

  function send(conn: Connection, payload: object): void {
    if (conn.socket.readyState !== openState) return;
    if (conn.socket.bufferedAmount > maxBufferedBytes) {
      conn.socket.terminate();
      return;
    }
    conn.socket.send(JSON.stringify(payload));
  }

  function forget(conn: Connection): void {
    conn.unsubscribe();
    const rest = (byUser.get(conn.user.uuid) ?? []).filter((other) => other !== conn);
    if (rest.length > 0) byUser.set(conn.user.uuid, rest);
    else byUser.delete(conn.user.uuid);
  }

  // A client that ignores the close frame keeps its socket open until `ws` gives up on it.
  function end(conn: Connection): void {
    conn.ended = true;
    conn.socket.close(closeCodes.sessionEnded, 'Session ended');
  }

  async function handleFrame(conn: Connection, text: string): Promise<void> {
    if (conn.ended) return;
    let ref: string | undefined;
    try {
      const frame = parseFrame(text);
      ref = frame.ref;
      const me = conn.user.uuid;
      if (frame.type === 'chat.send') {
        const message = await sendMessage(ctx, me, frame.to, frame.text, conn.id);
        send(conn, { type: 'chat.sent', ref, message });
      } else {
        const upTo = await markRead(ctx, me, frame.with, frame.upTo, conn.id);
        send(conn, { type: 'chat.read', ref, with: frame.with.toLowerCase(), upTo });
      }
    } catch (error) {
      if (error instanceof FrameError) {
        send(conn, { type: 'error', ref: error.ref, error: 'bad_request', message: error.message });
      } else if (error instanceof HttpError) {
        const retryAfter = error.retryAfterSeconds ?? undefined;
        send(conn, { type: 'error', ref, error: error.code, message: error.message, retryAfter });
      } else {
        ctx.log.error(error, 'handling a socket frame failed');
        send(conn, { type: 'error', ref, error: 'internal', message: 'Something went wrong' });
      }
    }
  }

  function open(socket: Socket, user: AuthUser): void {
    const conn: Connection = {
      id: randomUUID(),
      user,
      tokenHash: user.tokenHash.toString('hex'),
      socket,
      alive: true,
      ended: false,
      rateLimited: false,
      queue: Promise.resolve(),
      unsubscribe: () => {},
    };
    const existing = byUser.get(user.uuid) ?? [];
    while (existing.length >= maxConnectionsPerUser) {
      existing.shift()?.socket.close(closeCodes.tooManyConnections, 'Too many connections');
    }
    byUser.set(user.uuid, [...existing, conn]);

    conn.unsubscribe = ctx.bus.subscribe(user.uuid, (message) => {
      if (message.kind === 'end') {
        if (message.tokenHash === undefined || message.tokenHash === conn.tokenHash) end(conn);
      } else if (message.exceptConnection !== conn.id) {
        send(conn, message.event);
      }
    });

    socket.on('pong', () => {
      conn.alive = true;
    });
    socket.on('close', () => forget(conn));
    socket.on('message', (data, isBinary) => {
      if (conn.ended) return;
      if (isBinary) {
        socket.close(closeCodes.textOnly, 'Text frames only');
        return;
      }
      // Per player rather than per socket, so reconnecting does not start a fresh window.
      const hit = ctx.limiter.hit(`socket:${user.uuid}`, ctx.config.rateLimits.socketFramesPerMinute, minute);
      if (!hit.ok) {
        // One error per window; the frames after it are dropped quietly.
        if (!conn.rateLimited) {
          send(conn, { type: 'error', error: 'rate_limited', message: 'Too many messages', retryAfter: hit.retryAfterSeconds });
        }
        conn.rateLimited = true;
        return;
      }
      conn.rateLimited = false;
      const text = data.toString('utf8');
      // One frame at a time, so a player's messages are stored in the order they were sent.
      conn.queue = conn.queue.then(() => handleFrame(conn, text));
    });

    send(conn, { type: 'ready', user: { uuid: user.uuid, name: user.name } });
  }

  // A socket that missed the last ping is dropped; one whose session ran out is closed.
  const heartbeat = setInterval(() => {
    const now = ctx.now();
    for (const conn of [...byUser.values()].flat()) {
      if (now >= conn.user.sessionExpiresAt) {
        end(conn);
      } else if (!conn.alive) {
        conn.socket.terminate();
      } else {
        conn.alive = false;
        conn.socket.ping();
      }
    }
  }, ctx.config.heartbeatMs);
  heartbeat.unref();
  app.addHook('onClose', async () => clearInterval(heartbeat));

  app.route({
    method: 'GET',
    url: '/v1/ws',
    handler: async (_request, reply) =>
      reply.status(426).header('upgrade', 'websocket').send({ error: 'bad_request', message: 'Connect with a WebSocket' }),
    wsHandler: (socket: Socket, request) => open(socket, currentUser(request)),
  });
}
