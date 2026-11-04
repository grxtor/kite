-- Mutes and bans, by Minecraft UUID. No foreign key on users: deleting the account and
-- signing in again must not lift them. A row with neither left is deleted.
create table sanctions (
  uuid uuid primary key,
  -- No sending chat messages until then.
  muted_until timestamptz,
  -- No chat, invites or new friend requests while set.
  banned_at timestamptz,
  ban_reason text
);

-- What a moderator decided about a report. Reports filed before this were all still open.
alter table chat_reports
  add column status text not null default 'open' check (status in ('open', 'resolved', 'dismissed')),
  add column closed_at timestamptz,
  add column resolution_note text;
-- The notice counts the reports a player got in the last 24 hours.
create index chat_reports_reported_idx on chat_reports (reported_uuid, created_at);

-- Every moderation action, kept as long as reports are.
create table moderation_log (
  id bigint generated always as identity primary key,
  actor text not null,
  at timestamptz not null,
  action text not null check (action in ('mute', 'unmute', 'ban', 'unban', 'resolve', 'dismiss')),
  -- A player's UUID, or a report's id for resolve and dismiss.
  target text not null,
  note text
);
create index moderation_log_at_idx on moderation_log (at);
