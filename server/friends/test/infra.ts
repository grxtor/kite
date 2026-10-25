import { randomBytes } from 'node:crypto';
import { createServer } from 'node:http';
import type { IncomingMessage, Server, ServerResponse } from 'node:http';
import type { AddressInfo } from 'node:net';
import pg from 'pg';

export interface TestDatabase {
  url: string;
  drop: () => Promise<void>;
}

export async function createTestDatabase(): Promise<TestDatabase> {
  const adminUrl = process.env.TEST_DATABASE_URL ?? 'postgres://postgres@localhost:5432/postgres';
  const dbName = `kite_friends_test_${randomBytes(6).toString('hex')}`;
  const admin = new pg.Client({ connectionString: adminUrl });
  await admin.connect();
  try {
    await admin.query(`CREATE DATABASE ${dbName}`);
  } finally {
    await admin.end();
  }
  const url = new URL(adminUrl);
  url.pathname = `/${dbName}`;
  return {
    url: url.toString(),
    drop: async () => {
      const dropper = new pg.Client({ connectionString: adminUrl });
      await dropper.connect();
      try {
        await dropper.query('SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = $1', [dbName]);
        await dropper.query(`DROP DATABASE IF EXISTS ${dbName} WITH (FORCE)`);
      } finally {
        await dropper.end();
      }
    },
  };
}

export interface MojangRequest {
  username: string | null;
  serverId: string | null;
  url: string;
}

export interface MockMojang {
  url: string;
  requests: MojangRequest[];
  join: (name: string, uuid: string, serverId: string) => void;
  setFailing: (failing: boolean) => void;
  close: () => Promise<void>;
}

export async function startMockMojang(): Promise<MockMojang> {
  const joins = new Map<string, { name: string; uuid: string }>();
  const requests: MojangRequest[] = [];
  let failing = false;

  const handler = (req: IncomingMessage, res: ServerResponse): void => {
    const url = new URL(req.url ?? '/', 'http://mojang.test');
    if (req.method !== 'GET' || url.pathname !== '/session/minecraft/hasJoined') {
      res.writeHead(404).end();
      return;
    }
    const username = url.searchParams.get('username');
    const serverId = url.searchParams.get('serverId');
    requests.push({ username, serverId, url: req.url ?? '' });
    if (failing) {
      res.writeHead(500).end();
      return;
    }
    const joined = serverId === null ? undefined : joins.get(serverId);
    if (joined === undefined || username === null || joined.name.toLowerCase() !== username.toLowerCase()) {
      res.writeHead(204).end();
      return;
    }
    res.writeHead(200, { 'content-type': 'application/json' });
    res.end(JSON.stringify({ id: joined.uuid.replaceAll('-', ''), name: joined.name, properties: [] }));
  };

  const server: Server = createServer(handler);
  await new Promise<void>((resolve) => server.listen(0, '127.0.0.1', resolve));
  const { port } = server.address() as AddressInfo;

  return {
    url: `http://127.0.0.1:${port}`,
    requests,
    join: (name, uuid, serverId) => {
      joins.set(serverId, { name, uuid });
    },
    setFailing: (value) => {
      failing = value;
    },
    close: () =>
      new Promise<void>((resolve) => {
        server.closeAllConnections();
        server.close(() => resolve());
      }),
  };
}
