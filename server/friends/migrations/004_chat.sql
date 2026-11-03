-- No foreign keys on the players: when one deletes their account, the other keeps a
-- placeholder for each message until retention removes it.
create table messages (
  id bigint generated always as identity primary key,
  sender_uuid uuid not null,
  recipient_uuid uuid not null,
  -- Null once the sender deleted their account.
  text text,
  created_at timestamptz not null,
  -- Set for the side that deleted their account, so signing in again starts with no history.
  hidden_for_sender boolean not null default false,
  hidden_for_recipient boolean not null default false,
  check (sender_uuid <> recipient_uuid)
);
create index messages_pair_idx on messages (least(sender_uuid, recipient_uuid), greatest(sender_uuid, recipient_uuid), id);
create index messages_unread_idx on messages (recipient_uuid, sender_uuid, id);
create index messages_sender_idx on messages (sender_uuid);
create index messages_created_idx on messages (created_at);

-- How far each player has read each conversation.
create table chat_reads (
  reader_uuid uuid not null references users on delete cascade,
  other_uuid uuid not null,
  last_read_id bigint not null,
  primary key (reader_uuid, other_uuid)
);
create index chat_reads_other_idx on chat_reads (other_uuid);

-- A report keeps a copy of the message and what came before it, so it outlives retention
-- and the reported player deleting their account. When the reporter deletes theirs, the
-- report stays as evidence and only loses who filed it.
create table chat_reports (
  id bigint generated always as identity primary key,
  reporter_uuid uuid references users on delete set null,
  reported_uuid uuid not null,
  message_id bigint not null,
  reason text not null check (reason in ('spam', 'harassment', 'other')),
  snapshot jsonb not null,
  created_at timestamptz not null,
  unique (reporter_uuid, message_id)
);
create index chat_reports_created_idx on chat_reports (created_at);
