-- Whether friends may join the server the player is on. On by default, existing players included.
alter table users add column allow_join boolean not null default true;
