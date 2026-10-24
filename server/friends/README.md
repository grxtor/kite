# Kite friends server

The service behind Kite's friends list: add a friend by Minecraft name and
see whether they are online, playing, and on which server. There is no chat,
no voice and no world sharing. The launcher talks to it over plain HTTPS and
polls once a minute; there is no WebSocket.

- Production: `https://api.kiteclient.com` (not deployed yet)
- Stack: Node 24+, TypeScript (run directly by Node's type stripping),
  Fastify, PostgreSQL. Package manager: pnpm.

## Running it

```sh
pnpm install
DATABASE_URL=postgres://localhost:5432/kite_friends pnpm start
```

Migrations in `migrations/` run on startup. Settings come from the
environment:

| Variable | Default | Meaning |
|---|---|---|
| `DATABASE_URL` | (required) | Postgres connection string |
| `PORT` / `HOST` | `8080` / `0.0.0.0` | Where to listen |
| `MOJANG_SESSION_URL` | `https://sessionserver.mojang.com` | Session server used for `hasJoined` |
| `CLIENT_IP_HEADER` | (none) | Header that carries the client IP, e.g. `cf-connecting-ip` behind Cloudflare. Without it the socket address is used |
| `STATUS_TTL_SECONDS` | `180` | How long a reported status stays live |
| `SESSION_TTL_DAYS` | `30` | Lifetime of a session token |
| `LOG_LEVEL` | `info` | Pino log level. Requests are not logged, only errors |

Tests need a Postgres the test user may create databases on:

```sh
TEST_DATABASE_URL=postgres://postgres@localhost:5432/postgres pnpm test
pnpm typecheck
```

`docker compose up --build` starts the server with its own Postgres on
port 8080. Deployment is described in [Deploying](#deploying).

## API

All bodies are JSON. Times are ISO 8601 strings in UTC. UUIDs are written
with dashes (`069a79f4-44e9-4726-a5be-fca90e38aaf5`). Minecraft names are 1 to
16 characters of `A-Z a-z 0-9 _`.

Every endpoint except the two `auth` ones needs the session token:

```
Authorization: Bearer kfs_...
```

### Errors

Failures return a status code and a body like this:

```json
{ "error": "not_found", "message": "No such player" }
```

| Status | `error` | When |
|---|---|---|
| 400 | `bad_request` | The body or a parameter is invalid |
| 400 | `challenge_invalid` | Login with an unknown, used or expired challenge |
| 401 | `unauthorized` | Missing, unknown or expired token. Log in again |
| 401 | `not_joined` | Mojang did not confirm the `join` for this challenge |
| 404 | `not_found` | The player, request or friend does not exist, or you may not see it |
| 409 | `already_friends` | Friend request to someone who already is a friend |
| 409 | `blocked_by_you` | Friend request to someone you blocked. Unblock first |
| 409 | `limit_reached` | Too many friends (500), pending outgoing requests (100) or blocks (1000) |
| 429 | `rate_limited` | Too many requests. Wait for the `Retry-After` header (seconds) |
| 502 | `mojang_unavailable` | Mojang's session server failed or timed out. Try again later |

### Signing in

Kite never sees the Microsoft or Minecraft access token. The launcher proves
it owns the account the way a Minecraft server checks a joining player:

1. Ask for a challenge.

   ```http
   POST /v1/auth/challenge
   { "name": "Notch" }
   ```

   ```json
   200
   {
     "challengeId": "6f1c2c1e-6c55-4d0b-9d3b-6bd4f1a3e0a2",
     "serverId": "3f786850e387550fdab836ed7e6dc881de23001b",
     "expiresAt": "2026-10-24T18:02:00.000Z"
   }
   ```

   The challenge lives for two minutes.

2. Tell Mojang the account is joining that `serverId`. This request goes to
   Mojang, not to Kite, and carries the Minecraft access token:

   ```http
   POST https://sessionserver.mojang.com/session/minecraft/join
   {
     "accessToken": "<minecraft access token>",
     "selectedProfile": "069a79f444e94726a5befca90e38aaf5",
     "serverId": "3f786850e387550fdab836ed7e6dc881de23001b"
   }
   ```

   Mojang answers `204`.

3. Log in with the challenge. The server asks Mojang's `hasJoined` whether
   `name` joined `serverId`, and if so, issues a session token.

   ```http
   POST /v1/auth/login
   { "challengeId": "6f1c2c1e-6c55-4d0b-9d3b-6bd4f1a3e0a2" }
   ```

   ```json
   200
   {
     "token": "kfs_Jx2v...",
     "expiresAt": "2026-11-23T18:00:00.000Z",
     "user": { "uuid": "069a79f4-44e9-4726-a5be-fca90e38aaf5", "name": "Notch" }
   }
   ```

   `401 not_joined` means Mojang has no record of step 2 (yet). The challenge
   stays valid until it expires, so the launcher may retry after a second; a
   challenge allows five attempts. A challenge can be used for one login
   only.

The token lasts 30 days. On `401 unauthorized` from any endpoint, sign in
again. A user keeps at most 10 tokens; an eleventh login drops the oldest.

The server learns a player's current name only at sign-in. Someone who
renamed their Minecraft account keeps showing (and being found) under the old
name until their launcher signs in again, so the launcher should sign in
when the account's name differs from the one in `user.name`.

```http
POST /v1/auth/logout
```

`204`. Revokes the token used for the request.

### Me and settings

```http
GET /v1/me
```

```json
200
{
  "uuid": "069a79f4-44e9-4726-a5be-fca90e38aaf5",
  "name": "Notch",
  "settings": { "visibility": "friends", "hideServer": false, "shareIpServers": false }
}
```

- `visibility`
  - `"friends"` (default): friends see your status.
  - `"everyone"`: friends see it, and any signed-in Kite user can look it up by
    your name (see [Looking up a player](#looking-up-a-player)).
  - `"invisible"`: you always appear offline, with no last-seen time. While
    invisible the server stores no status for you at all.
- `hideServer`: friends see that you are playing, but not where. Server and
  single-player are both hidden.
- `shareIpServers`: off by default. When off, a server reached by a raw IP
  (`203.0.113.7`, `1.2.3.4.nip.io`) or a local address (`192.168.1.20`,
  `localhost`, `myserver.local`, `nas.fritz.box`, a single-label name) is
  shown as "a server" without its address. The address is not stored either.

```http
PATCH /v1/me/settings
{ "visibility": "everyone", "hideServer": true }
```

`200` with the full settings object. Any subset of the three fields may be
sent.

```http
DELETE /v1/me
```

`204`. Deletes the account and everything stored about it: tokens, status,
settings, friendships, requests in both directions, and the blocks you
placed. Blocks other players placed on you stay, because they belong to
them. Signing in again later creates a new, empty account.

### Reporting your status

While the launcher runs it sends its status every 60 seconds. A status that is
not refreshed expires after 180 seconds and the user shows as offline.

```http
PUT /v1/me/status
{ "state": "online" }
```

```http
PUT /v1/me/status
{
  "state": "playing",
  "instance": "Survival 1.21",
  "version": "1.21.4",
  "world": { "type": "server", "host": "mc.hypixel.net", "port": 25565 }
}
```

- `state`: `"online"` (launcher open) or `"playing"` (game running).
- `instance`: the instance name, 1 to 64 characters. Only with `playing`.
- `version`: the Minecraft version, 1 to 32 characters of
  `A-Z a-z 0-9 . _ - +` and spaces. Only with `playing`.
- `world`: where the player is, only with `playing`:
  - `{ "type": "server", "host": "...", "port": 25565 }` from the game log's
    `Connecting to <host>, <port>` line. `port` is optional and defaults to
    25565. The host must be a valid DNS name, IPv4 or IPv6 address. A name
    whose last label is a number (`2130706433`, `0x7f.0x1`) is refused,
    because resolvers read it as an IP address.
  - `{ "type": "singleplayer" }` for the integrated server.
  - `null` or left out while in the menus.

```json
200
{
  "expiresAt": "2026-10-24T18:03:00.000Z",
  "status": {
    "state": "playing",
    "instance": "Survival 1.21",
    "version": "1.21.4",
    "world": { "type": "server", "host": "mc.hypixel.net", "port": 25565 }
  }
}
```

`status` is what your friends see after your settings are applied (a raw IP
without `shareIpServers` comes back as `{ "type": "server" }`), so the launcher
can show it. When invisible, it is `{ "state": "offline", "lastSeenAt": null }`.

```http
DELETE /v1/me/status
```

`204`. Go offline right away, e.g. when the launcher quits.

### Status objects

Everywhere a status is shown it has one of these shapes:

```json
{ "state": "offline", "lastSeenAt": "2026-10-24T16:00:00.000Z" }
{ "state": "online" }
{ "state": "playing", "instance": "Survival 1.21", "version": "1.21.4", "world": <world> }
```

`world` is one of:

| `world` | Show as |
|---|---|
| `{ "type": "server", "host": "mc.hypixel.net", "port": 25565 }` | "on mc.hypixel.net" (a Join button can use host and port) |
| `{ "type": "server" }` | "on a server" (the address is a raw IP or local and not shared) |
| `{ "type": "singleplayer" }` | "Single-player" |
| `null` | "Playing" (in the menus, or `hideServer` is on) |

`lastSeenAt` is the last time the player reported a status, or `null` if never
or if the player is invisible.

### Friends

One call returns everything the launcher's friends panel needs, so this and
`PUT /v1/me/status` are the whole 60-second poll:

```http
GET /v1/friends
```

```json
200
{
  "friends": [
    {
      "uuid": "853c80ef-3c37-49fd-aa49-938b674adae6",
      "name": "jeb_",
      "since": "2026-10-20T12:00:00.000Z",
      "status": { "state": "playing", "instance": "Hypixel", "version": "1.8.9",
                  "world": { "type": "server", "host": "mc.hypixel.net", "port": 25565 } }
    }
  ],
  "requests": {
    "incoming": [ { "uuid": "...", "name": "Dinnerbone", "createdAt": "2026-10-24T17:58:00.000Z" } ],
    "outgoing": [ { "uuid": "...", "name": "Grumm", "createdAt": "2026-10-24T17:50:00.000Z" } ]
  }
}
```

Friends are sorted by name. Only players who signed in to Kite at least once
can be added.

```http
POST /v1/friends/requests
{ "name": "jeb_" }
```

| Response | Meaning |
|---|---|
| `201 { "status": "pending", "user": { "uuid", "name" } }` | Request sent |
| `200 { "status": "pending", "user": ... }` | Already sent earlier, nothing changed |
| `200 { "status": "accepted", "user": ... }` | They had already asked you, so you are now friends |
| `404 not_found` | No Kite user has that name, or they blocked you (the two are not told apart) |
| `400 bad_request` | The name is yours |
| `409 already_friends`, `409 blocked_by_you`, `409 limit_reached` | See [Errors](#errors) |
| `429 rate_limited` | More than 30 requests an hour, or 3 a day to the same player |

```http
POST /v1/friends/requests/{uuid}/accept
```

`200 { "user": { "uuid", "name" } }`, or `404` if that player has no pending
request to you.

```http
POST /v1/friends/requests/{uuid}/decline
```

`204`, or `404`. The sender is not told; their request just disappears.

```http
DELETE /v1/friends/requests/{uuid}
```

`204`, or `404`. Cancels your own outgoing request.

```http
DELETE /v1/friends/{uuid}
```

`204`, or `404` if you are not friends.

### Blocking

```http
PUT /v1/blocks/{uuid}
```

`204`, or `404` if no Kite user has that UUID, or `409 limit_reached` past
1000 blocks. Blocking removes the friendship and pending requests in both
directions. A blocked player cannot
send you requests and cannot see your status, even with `visibility:
"everyone"`; you cannot see theirs either.

```http
DELETE /v1/blocks/{uuid}
```

`204`, or `404` if you had not blocked them.

```http
GET /v1/blocks
```

```json
200
{ "blocked": [ { "uuid": "...", "name": "Griefer", "createdAt": "2026-10-24T17:00:00.000Z" } ] }
```

### Looking up a player

```http
GET /v1/players/{name}/status
```

```json
200
{ "uuid": "...", "name": "jeb_", "status": { "state": "online" } }
```

Returns the status if the player is a friend, or has `visibility:
"everyone"`, or is you. Otherwise `404 not_found`, which is also the answer
for an unknown name, an invisible non-friend and a block in either direction.
Limited to 30 lookups a minute.

### Rate limits

| Scope | Limit |
|---|---|
| Any request, per IP | 300 a minute |
| `auth` endpoints, per IP | 20 a minute |
| Signed-in requests, per user | 120 a minute |
| Friend requests sent, per user | 30 an hour, 3 a day to the same player |
| Player lookups, per user | 30 a minute |

The launcher's normal load is two requests a minute.

### Health

```http
GET /healthz
```

`200 { "ok": true }` when the database answers.

## Privacy

What the server stores, all of it deleted by `DELETE /v1/me` (except blocks
other players placed on you):

- Minecraft UUID and name, and when they were last seen
- Settings
- Friendships, pending requests and blocks
- The current status: state, instance name, Minecraft version and world. It
  expires three minutes after the last report and is purged after that.
- Session tokens, stored only as SHA-256 hashes

Raw IP and local server addresses are not stored unless `shareIpServers` is
on. Requests are not logged. Client IPs are only kept in memory for rate
limiting.

## Deploying

The server is meant to run on the Dokploy server as an Application in the
Kite project, next to a Dokploy Postgres database:

1. Create a Postgres database service (e.g. `kite-friends-db`) in the Kite
   project. Keep it internal; it needs no public port.
2. Create an Application from the GitHub repo `grxtor/kite`, build type
   Dockerfile, build path `/server/friends`, port 8080, with watch path
   `server/friends/**`.
3. Environment:

   ```
   DATABASE_URL=postgres://<user>:<password>@<db internal host>:5432/<db>
   CLIENT_IP_HEADER=cf-connecting-ip
   ```

4. Domain `api.kiteclient.com` → port 8080, HTTPS with Let's Encrypt, and a
   proxied Cloudflare `A` record `api` pointing at the Dokploy server.
5. The health check is `GET /healthz`.

`CLIENT_IP_HEADER` matters: without it every request seems to come from
Traefik, and all users share one per-IP limit. `cf-connecting-ip` is only
safe while the server is reached through Cloudflare, because anyone talking
to the origin directly can set the header. Per-user limits still apply to
every signed-in request. Without Cloudflare in front, use `x-real-ip`, which
Traefik sets itself.

`docker-compose.yml` runs the same image with its own Postgres, for a local
run.
