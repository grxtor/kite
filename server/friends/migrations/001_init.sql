create table users (
  uuid uuid primary key,
  name text not null,
  name_lower text not null,
  visibility text not null default 'friends'
    check (visibility in ('everyone', 'friends', 'invisible')),
  hide_server boolean not null default false,
  share_ip_servers boolean not null default false,
  created_at timestamptz not null,
  -- Names can move between accounts; lookups take the most recent holder.
  name_seen_at timestamptz not null,
  last_seen_at timestamptz
);
create index users_name_lower_idx on users (name_lower, name_seen_at desc);

create table sessions (
  token_hash bytea primary key,
  -- Login order, so the oldest session can be dropped even within one clock tick.
  seq bigint generated always as identity,
  user_uuid uuid not null references users on delete cascade,
  created_at timestamptz not null,
  expires_at timestamptz not null
);
create index sessions_user_idx on sessions (user_uuid, seq);
create index sessions_expires_idx on sessions (expires_at);

create table challenges (
  id uuid primary key,
  name text not null,
  server_id text not null,
  expires_at timestamptz not null
);
create index challenges_expires_idx on challenges (expires_at);

create table friend_requests (
  from_uuid uuid not null references users on delete cascade,
  to_uuid uuid not null references users on delete cascade,
  created_at timestamptz not null,
  primary key (from_uuid, to_uuid),
  check (from_uuid <> to_uuid)
);
create index friend_requests_to_idx on friend_requests (to_uuid);

-- One row per pair, the smaller uuid first.
create table friendships (
  user_a uuid not null references users on delete cascade,
  user_b uuid not null references users on delete cascade,
  created_at timestamptz not null,
  primary key (user_a, user_b),
  check (user_a < user_b)
);
create index friendships_b_idx on friendships (user_b);

create table blocks (
  blocker uuid not null references users on delete cascade,
  blocked uuid not null references users on delete cascade,
  created_at timestamptz not null,
  primary key (blocker, blocked),
  check (blocker <> blocked)
);
create index blocks_blocked_idx on blocks (blocked);

create table presence (
  user_uuid uuid primary key references users on delete cascade,
  state text not null check (state in ('online', 'playing')),
  instance text,
  version text,
  world jsonb,
  expires_at timestamptz not null
);
create index presence_expires_idx on presence (expires_at);
