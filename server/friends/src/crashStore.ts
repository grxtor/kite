import type pg from 'pg';
import { day } from './context.ts';
import { sampleOf, type CrashReport, type CrashSample } from './crashReports.ts';
import { withTransaction } from './db.ts';

/** Reports are kept this long; the signature's counts outlive them. */
export const crashReportRetentionDays = 90;
/** A signature nobody has sent for this long is forgotten. */
export const crashSignatureRetentionDays = 365;
/** The newest reports kept for each signature; older ones only count. */
export const maxSamplesPerSignature = 20;
// The crash doctor's rule ids, as crashReports.ts checks them.
const ruleIdPattern = /^[a-z0-9]+(\.[a-z0-9-]+)+$/;
// Short enough to type, long enough not to be ambiguous among a few thousand.
export const minSignaturePrefix = 8;

type Queryable = Pick<pg.Pool, 'query'>;

export interface SignatureRow {
  signature: string;
  lines: string[];
  firstSeen: Date;
  lastSeen: Date;
  reportCount: number;
  resolvedRuleId: string | null;
}

interface SignatureDbRow {
  signature: string;
  lines: string[];
  first_seen: Date;
  last_seen: Date;
  report_count: string;
  resolved_rule_id: string | null;
}

function signatureRow(row: SignatureDbRow): SignatureRow {
  return {
    signature: row.signature,
    lines: row.lines,
    firstSeen: row.first_seen,
    lastSeen: row.last_seen,
    reportCount: Number(row.report_count),
    resolvedRuleId: row.resolved_rule_id,
  };
}

/** The UTC day of @p now, which is all a report keeps of when it came. */
function dayOf(now: Date): string {
  return now.toISOString().slice(0, 10);
}

/** Counts @p report under its signature and keeps it as one of the signature's newest samples. */
export async function recordCrashReport(pool: pg.Pool, report: CrashReport, now: Date): Promise<void> {
  await withTransaction(pool, async (client) => {
    await client.query(
      `insert into crash_signatures (signature, format_version, lines, first_seen, last_seen)
       values ($1, $2, $3, $4, $4)
       on conflict (signature) do update
       set last_seen = greatest(crash_signatures.last_seen, excluded.last_seen),
           report_count = crash_signatures.report_count + 1`,
      [report.signature, report.formatVersion, JSON.stringify(report.lines), now],
    );
    await client.query('insert into crash_reports (signature, received_on, payload) values ($1, $2, $3)', [
      report.signature,
      dayOf(now),
      JSON.stringify(sampleOf(report)),
    ]);
    await client.query(
      `delete from crash_reports where signature = $1 and id not in
         (select id from crash_reports where signature = $1 order by id desc limit $2)`,
      [report.signature, maxSamplesPerSignature],
    );
  });
}

/** Deletes reports past retention and signatures nobody sent for a year. */
export async function purgeCrashReports(pool: Queryable, now: Date): Promise<void> {
  await pool.query('delete from crash_reports where received_on < $1', [
    dayOf(new Date(now.getTime() - crashReportRetentionDays * day)),
  ]);
  await pool.query('delete from crash_signatures where last_seen < $1', [
    new Date(now.getTime() - crashSignatureRetentionDays * day),
  ]);
}

const signatureColumns = 'signature, lines, first_seen, last_seen, report_count, resolved_rule_id';

/** The most sent signatures, unresolved ones only unless @p all. */
export async function topSignatures(pool: Queryable, limit: number, all = false): Promise<SignatureRow[]> {
  const { rows } = await pool.query<SignatureDbRow>(
    `select ${signatureColumns} from crash_signatures
     where $2 or resolved_rule_id is null
     order by report_count desc, last_seen desc, signature limit $1`,
    [limit, all],
  );
  return rows.map(signatureRow);
}

/** The signature @p prefix stands for: a full one, or the start of exactly one. */
export async function findSignature(pool: Queryable, prefix: string): Promise<SignatureRow> {
  const wanted = prefix.toLowerCase();
  if (!/^[0-9a-f]+$/.test(wanted) || wanted.length < minSignaturePrefix || wanted.length > 64) {
    throw new Error(`A signature is ${minSignaturePrefix} to 64 hex characters`);
  }
  const { rows } = await pool.query<SignatureDbRow>(
    `select ${signatureColumns} from crash_signatures where starts_with(signature, $1) order by signature limit 2`,
    [wanted],
  );
  const [first, second] = rows;
  if (!first) throw new Error(`No signature starts with ${wanted}`);
  if (second) throw new Error(`More than one signature starts with ${wanted}`);
  return signatureRow(first);
}

/** The samples of @p signature, newest first. */
export async function samplesOf(pool: Queryable, signature: string): Promise<CrashSample[]> {
  const { rows } = await pool.query<{ payload: CrashSample }>(
    'select payload from crash_reports where signature = $1 order by id desc',
    [signature],
  );
  return rows.map((row) => row.payload);
}

/** Marks @p signature as recognised by @p ruleId; crash-top lists it no more. */
export async function resolveSignature(pool: Queryable, signature: string, ruleId: string, now: Date): Promise<void> {
  if (!ruleIdPattern.test(ruleId)) throw new Error(`${ruleId} is not a crash rule id`);
  const updated = await pool.query(
    'update crash_signatures set resolved_rule_id = $2, resolved_at = $3 where signature = $1',
    [signature, ruleId, now],
  );
  if (updated.rowCount === 0) throw new Error(`No signature ${signature}`);
}
