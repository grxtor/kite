-- Crash reports players chose to send. Nothing here names a player: no account, no IP,
-- no instance name; the error lines are scrubbed by the launcher and checked again here.

-- One row per kind of crash. The signature is the SHA-256 of the normalized error lines,
-- so every report of a signature has the same lines, kept once here.
create table crash_signatures (
  signature text primary key check (signature ~ '^[0-9a-f]{64}$'),
  format_version integer not null,
  lines jsonb not null,
  first_seen timestamptz not null,
  last_seen timestamptz not null,
  report_count bigint not null default 1,
  -- Set once a rule in crash-rules.json recognises it (crash-top --resolve).
  resolved_rule_id text,
  resolved_at timestamptz
);
create index crash_signatures_unresolved_idx on crash_signatures (report_count desc) where resolved_rule_id is null;
create index crash_signatures_last_seen_idx on crash_signatures (last_seen);

-- The newest reports of each signature (versions, loader, mods), a few per signature and
-- only for a while. The day it came in, not the time.
create table crash_reports (
  id bigint generated always as identity primary key,
  signature text not null references crash_signatures on delete cascade,
  received_on date not null,
  payload jsonb not null
);
create index crash_reports_signature_idx on crash_reports (signature, id);
create index crash_reports_received_idx on crash_reports (received_on);
