import { readdir, readFile } from 'node:fs/promises';
import pg from 'pg';

const migrationsDir = new URL('../migrations/', import.meta.url);
// Any constant works; it only has to be the same for every instance.
const migrationLock = 7_305_001;
const statementTimeoutMs = 10_000;

export function createPool(databaseUrl: string): pg.Pool {
  return new pg.Pool({ connectionString: databaseUrl, max: 10, statement_timeout: statementTimeoutMs });
}

export async function withTransaction<T>(pool: pg.Pool, work: (client: pg.PoolClient) => Promise<T>): Promise<T> {
  const client = await pool.connect();
  try {
    await client.query('begin');
    const result = await work(client);
    await client.query('commit');
    return result;
  } catch (error) {
    await client.query('rollback');
    throw error;
  } finally {
    client.release();
  }
}

export async function migrate(pool: pg.Pool): Promise<void> {
  const files = (await readdir(migrationsDir)).filter((file) => file.endsWith('.sql')).sort();
  await withTransaction(pool, async (client) => {
    await client.query('select pg_advisory_xact_lock($1)', [migrationLock]);
    await client.query(
      'create table if not exists schema_migrations (name text primary key, applied_at timestamptz not null default now())',
    );
    const applied = new Set(
      (await client.query<{ name: string }>('select name from schema_migrations')).rows.map((row) => row.name),
    );
    for (const file of files) {
      if (applied.has(file)) continue;
      await client.query(await readFile(new URL(file, migrationsDir), 'utf8'));
      await client.query('insert into schema_migrations (name) values ($1)', [file]);
    }
  });
}
