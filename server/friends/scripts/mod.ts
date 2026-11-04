/*
 * Moderation of chat reports (README, "Moderation"). Reads DATABASE_URL.
 *
 *   pnpm mod list [--open] [--limit 50]
 *   pnpm mod show <id>
 *   pnpm mod mute <uuid> <duration>
 *   pnpm mod unmute <uuid>
 *   pnpm mod ban <uuid> [--reason <text>]
 *   pnpm mod unban <uuid>
 *   pnpm mod resolve <id> [--note <text>]
 *   pnpm mod dismiss <id> [--note <text>]
 *
 * Every change is written to moderation_log with actor "cli". A ban's reason is shown to the
 * banned player, so it must not name who reported them.
 */
import { createPool } from '../src/db.ts';
import { runMod } from '../src/modCli.ts';
import { ModError } from '../src/modStore.ts';

const databaseUrl = process.env.DATABASE_URL;
if (!databaseUrl) {
  console.error('DATABASE_URL is required');
  process.exit(2);
}
const pool = createPool(databaseUrl);

try {
  console.log(await runMod(pool, process.argv.slice(2), new Date()));
} catch (error) {
  if (!(error instanceof ModError)) throw error;
  console.error(error.message);
  process.exitCode = 1;
} finally {
  await pool.end();
}
