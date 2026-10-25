import assert from 'node:assert/strict';
import type { AddressInfo } from 'node:net';
import { buildApp } from '../src/app.ts';
import type { Config, RateLimits } from '../src/config.ts';
import { createPool, migrate } from '../src/db.ts';
import { createTestDatabase, startMockMojang, type MockMojang } from './infra.ts';

export const NOTCH = { name: 'Notch', uuid: '069a79f4-44e9-4726-a5be-fca90e38aaf5' };
export const JEB = { name: 'jeb_', uuid: '853c80ef-3c37-49fd-aa49-938b674adae6' };
export const START_TIME = Date.parse('2026-10-24T18:00:00Z');

export function makeUuid(n: number): string {
  return `00000000-0000-4000-8000-${n.toString(16).padStart(12, '0')}`;
}

export interface UserRef {
  uuid: string;
  name: string;
}

export interface ChallengeBody {
  challengeId: string;
  serverId: string;
  expiresAt: string;
}

export interface LoginBody {
  token: string;
  expiresAt: string;
  user: UserRef;
}

export interface Settings {
  visibility: string;
  hideServer: boolean;
  shareIpServers: boolean;
  allowJoin: boolean;
}

export interface MeBody extends UserRef {
  settings: Settings;
}

export interface WorldBody {
  type: string;
  host?: string;
  port?: number;
}

export interface StatusBody {
  state: string;
  lastSeenAt?: string | null;
  instance?: string;
  version?: string;
  world?: WorldBody | null;
  joinable?: boolean;
}

export interface FriendEntry extends UserRef {
  since: string;
  status: StatusBody;
}

export interface RequestEntry extends UserRef {
  createdAt: string;
}

export interface FriendsBody {
  friends: FriendEntry[];
  requests: { incoming: RequestEntry[]; outgoing: RequestEntry[] };
}

export interface ErrorBody {
  error: string;
  message: string;
}

export interface Player extends UserRef {
  token: string;
}

export interface ApiOptions {
  token?: string;
  body?: unknown;
  headers?: Record<string, string>;
}

export interface ApiResponse<T> {
  status: number;
  headers: Headers;
  body: T;
}

export interface TestClock {
  now: () => number;
  advance: (seconds: number) => void;
  iso: (offsetSeconds?: number) => string;
}

export interface TestConfigOverrides extends Partial<Omit<Config, 'rateLimits'>> {
  rateLimits?: Partial<RateLimits>;
}

export interface TestServer {
  baseUrl: string;
  clock: TestClock;
  mojang: MockMojang;
  api: <T = unknown>(method: string, path: string, options?: ApiOptions) => Promise<ApiResponse<T>>;
  signIn: (name: string, uuid: string) => Promise<string>;
  newPlayer: (name?: string, uuid?: string) => Promise<Player>;
  makeFriends: (a: Player, b: Player) => Promise<void>;
  friendsOf: (player: Player) => Promise<FriendsBody>;
  close: () => Promise<void>;
}

const RELAXED_LIMIT = 1_000_000;
const relaxedRateLimits: RateLimits = {
  ipPerMinute: RELAXED_LIMIT,
  authPerMinute: RELAXED_LIMIT,
  userPerMinute: RELAXED_LIMIT,
  friendRequestsPerHour: RELAXED_LIMIT,
  friendRequestsPerPairPerDay: RELAXED_LIMIT,
  lookupsPerMinute: RELAXED_LIMIT,
};

export async function startServer(overrides: TestConfigOverrides = {}): Promise<TestServer> {
  const db = await createTestDatabase();
  const mojang = await startMockMojang();
  const pool = createPool(db.url);
  await migrate(pool);

  let now = START_TIME;
  const clock: TestClock = {
    now: () => now,
    advance: (seconds) => {
      now += seconds * 1000;
    },
    iso: (offsetSeconds = 0) => new Date(now + offsetSeconds * 1000).toISOString(),
  };

  const { rateLimits, ...rest } = overrides;
  const config: Config = {
    port: 0,
    host: '127.0.0.1',
    databaseUrl: db.url,
    mojangSessionUrl: mojang.url,
    clientIpHeader: null,
    logLevel: 'silent',
    statusTtlSeconds: 180,
    sessionTtlDays: 30,
    challengeTtlSeconds: 120,
    ...rest,
    rateLimits: { ...relaxedRateLimits, ...rateLimits },
  };

  const app = await buildApp({ config, pool, clock: clock.now });
  await app.listen({ port: 0, host: '127.0.0.1' });
  const baseUrl = `http://127.0.0.1:${(app.server.address() as AddressInfo).port}`;

  async function api<T = unknown>(method: string, path: string, options: ApiOptions = {}): Promise<ApiResponse<T>> {
    const headers: Record<string, string> = { ...options.headers };
    if (options.token !== undefined) headers.authorization = `Bearer ${options.token}`;
    if (options.body !== undefined) headers['content-type'] = 'application/json';
    const res = await fetch(baseUrl + path, {
      method,
      headers,
      body: options.body === undefined ? undefined : JSON.stringify(options.body),
    });
    const text = await res.text();
    return { status: res.status, headers: res.headers, body: (text === '' ? undefined : JSON.parse(text)) as T };
  }

  async function signIn(name: string, uuid: string): Promise<string> {
    const challenge = await api<ChallengeBody>('POST', '/v1/auth/challenge', { body: { name } });
    assert.equal(challenge.status, 200, `challenge for ${name}`);
    mojang.join(name, uuid, challenge.body.serverId);
    const login = await api<LoginBody>('POST', '/v1/auth/login', { body: { challengeId: challenge.body.challengeId } });
    assert.equal(login.status, 200, `login for ${name}`);
    return login.body.token;
  }

  let counter = 0;
  async function newPlayer(name?: string, uuid?: string): Promise<Player> {
    counter += 1;
    const playerName = name ?? `Player${counter}`;
    const playerUuid = uuid ?? makeUuid(counter + 1000);
    return { name: playerName, uuid: playerUuid, token: await signIn(playerName, playerUuid) };
  }

  async function makeFriends(a: Player, b: Player): Promise<void> {
    const sent = await api('POST', '/v1/friends/requests', { token: a.token, body: { name: b.name } });
    assert.equal(sent.status, 201);
    const accepted = await api('POST', `/v1/friends/requests/${a.uuid}/accept`, { token: b.token });
    assert.equal(accepted.status, 200);
  }

  async function friendsOf(player: Player): Promise<FriendsBody> {
    const res = await api<FriendsBody>('GET', '/v1/friends', { token: player.token });
    assert.equal(res.status, 200);
    return res.body;
  }

  return {
    baseUrl,
    clock,
    mojang,
    api,
    signIn,
    newPlayer,
    makeFriends,
    friendsOf,
    close: async () => {
      await app.close();
      await pool.end();
      await mojang.close();
      await db.drop();
    },
  };
}
