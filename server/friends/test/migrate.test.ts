import assert from 'node:assert/strict';
import { after, before, describe, test } from 'node:test';
import type pg from 'pg';
import { createPool, migrate } from '../src/db.ts';
import { createTestDatabase, type TestDatabase } from './infra.ts';

describe('migrations', () => {
  let db: TestDatabase;
  let pool: pg.Pool;

  before(async () => {
    db = await createTestDatabase();
    pool = createPool(db.url);
  });

  after(async () => {
    await pool.end();
    await db.drop();
  });

  test('running them twice changes nothing', async () => {
    await migrate(pool);
    await migrate(pool);
    const { rows } = await pool.query<{ name: string }>('select name from schema_migrations order by name');
    assert.deepEqual(
      rows.map((row) => row.name),
      ['001_init.sql', '002_allow_join.sql'],
    );
  });

  test('002 lets players who existed before it be joined', async () => {
    // Back to the schema before 002, with one player in it.
    await pool.query('alter table users drop column allow_join');
    await pool.query(`delete from schema_migrations where name = '002_allow_join.sql'`);
    await pool.query(
      `insert into users (uuid, name, name_lower, created_at, name_seen_at)
       values ('069a79f4-44e9-4726-a5be-fca90e38aaf5', 'Notch', 'notch', now(), now())`,
    );
    await migrate(pool);
    const { rows } = await pool.query<{ allow_join: boolean }>('select allow_join from users');
    assert.deepEqual(rows, [{ allow_join: true }]);
  });
});
