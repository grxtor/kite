-- Whether friends may invite the player to a server. On by default, existing players included.
alter table users add column allow_invites boolean not null default true;

-- One live invite per sender and recipient; a new one replaces it.
create table invites (
  from_uuid uuid not null references users on delete cascade,
  to_uuid uuid not null references users on delete cascade,
  host text not null,
  port integer not null check (port between 1 and 65535),
  created_at timestamptz not null,
  expires_at timestamptz not null,
  primary key (from_uuid, to_uuid),
  check (from_uuid <> to_uuid)
);
create index invites_to_idx on invites (to_uuid);
create index invites_expires_idx on invites (expires_at);

-- `code` is the eight characters after KITE-, upper case and without dashes.
create table share_codes (
  code text primary key,
  owner_uuid uuid not null references users on delete cascade,
  manifest jsonb not null,
  created_at timestamptz not null,
  expires_at timestamptz not null
);
create index share_codes_owner_idx on share_codes (owner_uuid, created_at desc);
create index share_codes_expires_idx on share_codes (expires_at);
