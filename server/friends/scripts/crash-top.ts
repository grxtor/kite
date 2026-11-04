/*
 * The crash reports players sent, for the one who writes crash rules (README, "Crash reports").
 *
 *   pnpm crash-top [--limit 20] [--all]
 *   pnpm crash-top --signature <sig> [--dump <dir>]
 *   pnpm crash-top --resolve <sig> --rule <rule-id>
 *
 * <sig> is a signature or its first 8 or more characters. Reads DATABASE_URL.
 */
import { parseArgs } from 'node:util';
import { findSignature, resolveSignature, samplesOf, topSignatures } from '../src/crashStore.ts';
import { dumpSignature, signatureText, summarize } from '../src/crashTop.ts';
import { createPool } from '../src/db.ts';

const defaultLimit = 20;

const { values } = parseArgs({
  options: {
    limit: { type: 'string' },
    all: { type: 'boolean', default: false },
    signature: { type: 'string' },
    dump: { type: 'string' },
    resolve: { type: 'string' },
    rule: { type: 'string' },
  },
  strict: true,
});

const databaseUrl = process.env.DATABASE_URL;
if (!databaseUrl) {
  console.error('DATABASE_URL is required');
  process.exit(2);
}
const pool = createPool(databaseUrl);

async function main(): Promise<void> {
  if (values.resolve !== undefined) {
    if (!values.rule) throw new Error('--resolve needs --rule <rule-id>');
    const row = await findSignature(pool, values.resolve);
    await resolveSignature(pool, row.signature, values.rule, new Date());
    console.log(`${row.signature} is resolved by ${values.rule}`);
    return;
  }
  if (values.signature !== undefined) {
    const row = await findSignature(pool, values.signature);
    const samples = await samplesOf(pool, row.signature);
    console.log(row.signature);
    console.log(signatureText(1, row, summarize(samples)));
    if (values.dump) console.log(`Written to ${await dumpSignature(values.dump, row, samples)}`);
    return;
  }
  const limit = values.limit === undefined ? defaultLimit : Number(values.limit);
  if (!Number.isInteger(limit) || limit <= 0) throw new Error('--limit must be a positive integer');
  const rows = await topSignatures(pool, limit, values.all);
  if (rows.length === 0) console.log(values.all ? 'No crash reports yet.' : 'No unresolved crash reports.');
  for (const [index, row] of rows.entries()) {
    console.log(signatureText(index + 1, row, summarize(await samplesOf(pool, row.signature))));
    console.log('');
  }
}

try {
  await main();
} catch (error) {
  console.error(error instanceof Error ? error.message : error);
  process.exitCode = 1;
} finally {
  await pool.end();
}
