# Kite friends server

The service behind Kite's friends list: add a friend by Minecraft name, see
whether they are online, playing, and on which server, and invite them to a
server, and chat with them one to one. It also keeps share codes that hand
an instance's Minecraft version, loader and mods to someone else. There is
no group chat, no voice and no world sharing. The launcher talks to it over
HTTPS and keeps a [WebSocket](#realtime-socket) open for pushed events and
chat; polling once a minute without the socket keeps working.

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
| `CLIENT_IP_HEADER` | (none) | Header that carries the client IP, e.g. `cf-connecting-ip` behind Cloudflare. Believed only for requests that reached the proxy from a Cloudflare address. Without it the socket address is used |
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

Every endpoint except the two `auth` ones needs the session token, the
WebSocket upgrade included:

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
| 404 | `not_found` | The player, request, friend, invite, message or share code does not exist, or you may not see it |
| 409 | `already_friends` | Friend request to someone who already is a friend |
| 409 | `blocked_by_you` | Friend request to someone you blocked. Unblock first |
| 409 | `limit_reached` | Too many friends (500), pending outgoing requests (100), blocks (1000) or share codes (50) |
| 409 | `address_not_shared` | Invite to a raw IP or local address while your `shareIpServers` is off |
| 409 | `invites_off` | Invite to a friend whose `allowInvites` is off |
| 413 | `bad_request` | The body is larger than 8 KB (64 KB for a share code manifest) |
| 426 | `bad_request` | `GET /v1/ws` without a WebSocket upgrade |
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
  "settings": {
    "visibility": "friends",
    "hideServer": false,
    "shareIpServers": false,
    "allowJoin": true,
    "allowInvites": true
  }
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
- `allowJoin`: on by default. Friends may join the server you are on. It
  only takes effect while they can see the server's address; see `joinable`
  in [Status objects](#status-objects).
- `allowInvites`: on by default. Friends may [invite](#invites) you to a
  server. Turning it off also drops the invites you have already received.

```http
PATCH /v1/me/settings
{ "visibility": "everyone", "hideServer": true }
```

`200` with the full settings object. Any subset of the five fields may be
sent.

```http
DELETE /v1/me
```

`204`. Deletes the account and everything stored about it: tokens, status,
settings, friendships, requests and invites in both directions, your share
codes, the blocks you placed, your read markers and the reports you filed.
The text of every chat message you sent is deleted; the people you wrote to
keep a placeholder (`"text": null, "deleted": true`) until it ages out, and
are told with a [`chat.deleted`](#events) event. Messages you received are
hidden from you for good. Blocks other players placed on you stay, because
they belong to them, and so do reports others filed about your messages (see
[Reports](#reports)). Your open sockets close with `4401`. Signing in again
later creates a new, empty account with no chat history.

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
    "world": { "type": "server", "host": "mc.hypixel.net", "port": 25565 },
    "joinable": true
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
{ "state": "playing", "instance": "Survival 1.21", "version": "1.21.4", "world": <world>, "joinable": true }
```

`world` is one of:

| `world` | Show as |
|---|---|
| `{ "type": "server", "host": "mc.hypixel.net", "port": 25565 }` | "on mc.hypixel.net" (a Join button uses host and port when `joinable`) |
| `{ "type": "server" }` | "on a server" (the address is a raw IP or local and not shared) |
| `{ "type": "singleplayer" }` | "Single-player" |
| `null` | "Playing" (in the menus, or `hideServer` is on) |

`joinable` says whether the launcher may offer to join the player: it is
`true` only when the player's `allowJoin` is on and `world` is a server with a
`host`. Single-player, the menus, `hideServer` and an address the player does
not share all make it `false`. It follows the player's current settings, so
turning `allowJoin` off takes effect on the next poll.

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
                  "world": { "type": "server", "host": "mc.hypixel.net", "port": 25565 },
                  "joinable": true },
      "unread": 2
    }
  ],
  "requests": {
    "incoming": [ { "uuid": "...", "name": "Dinnerbone", "createdAt": "2026-10-24T17:58:00.000Z" } ],
    "outgoing": [ { "uuid": "...", "name": "Grumm", "createdAt": "2026-10-24T17:50:00.000Z" } ]
  },
  "invites": {
    "incoming": [ { "uuid": "...", "name": "jeb_", "host": "mc.hypixel.net", "port": 25565,
                    "createdAt": "2026-10-24T17:59:00.000Z", "expiresAt": "2026-10-24T18:09:00.000Z" } ],
    "outgoing": []
  }
}
```

`invites` are the live [invites](#invites) to and from you. `unread` is the
number of [chat](#chat) messages from that friend after your read marker.
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

`204`, or `404` if you are not friends. Invites between the two of you go
too.

### Invites

"jeb_ invites you to mc.hypixel.net". An invite goes from one friend to
another and names a server. It lives for 10 minutes and reaches the other
side through the `invites` part of `GET /v1/friends`, so the launcher's
regular poll is enough.

```http
PUT /v1/invites/{uuid}
{ "host": "mc.hypixel.net", "port": 25565 }
```

```json
200
{
  "uuid": "853c80ef-3c37-49fd-aa49-938b674adae6",
  "name": "jeb_",
  "host": "mc.hypixel.net",
  "port": 25565,
  "createdAt": "2026-10-24T18:00:00.000Z",
  "expiresAt": "2026-10-24T18:10:00.000Z"
}
```

`{uuid}` is the friend you invite; the answer is the invite as it shows in
your `outgoing` list. `host` follows the rules of a status `world` host and
is stored normalized (lower case, no trailing dot); `port` is optional and
defaults to 25565. There is one invite per sender and friend: inviting the
same friend again replaces the old one and starts a new 10 minutes.

| Response | Meaning |
|---|---|
| `200` | Invite sent or replaced |
| `400 bad_request` | The host is not a valid server address, or the uuid is yours |
| `404 not_found` | Not friends with this player (a block either way, an unknown player and a pending request look the same) |
| `409 address_not_shared` | The host is a raw IP or local address and your `shareIpServers` is off. Such an address is never stored |
| `409 invites_off` | The friend's `allowInvites` is off |
| `429 rate_limited` | More than 30 invites an hour, or 5 an hour to the same friend |

```http
GET /v1/invites
```

`200 { "incoming": [...], "outgoing": [...] }`, the same lists as in
`GET /v1/friends`, newest first. `uuid` and `name` are the other player.

```http
POST /v1/invites/{uuid}/dismiss
```

`204`, or `404` if that player has no live invite to you. The sender is not
told; the invite just disappears from their `outgoing` list.

```http
DELETE /v1/invites/{uuid}
```

`204`, or `404`. Cancels your own invite to that player.

An invite also goes away when either player blocks the other, when they stop
being friends, when the recipient turns `allowInvites` off, and, for a raw IP
or local address, when the sender turns `shareIpServers` off.

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

### Share codes

A share code (`KITE-7F3K-9QXM`) gives someone else an instance's Minecraft
version, loader and content, so their launcher can build the same instance.
The manifest names projects on Modrinth and CurseForge by id and nothing
else: no URLs, no file contents, no paths. The launcher downloads files from
the platform, so a code can never carry a jar from anywhere.

```http
POST /v1/share-codes
{
  "name": "Survival 1.21",
  "minecraftVersion": "1.21.4",
  "loader": { "type": "fabric", "version": "0.16.10" },
  "content": [
    { "kind": "mod", "platform": "modrinth", "projectId": "AANobbMI", "versionId": "IZskON6d" },
    { "kind": "resourcepack", "platform": "curseforge", "projectId": 238222, "fileId": 5432100 }
  ]
}
```

```json
201
{ "code": "KITE-7F3K-9QXM", "createdAt": "2026-10-24T18:00:00.000Z", "expiresAt": "2027-01-22T18:00:00.000Z" }
```

The manifest is checked strictly; a field that is not listed here makes it
`400`, it is not silently dropped.

- `name`: 1 to 64 characters, control and bidi characters removed.
- `minecraftVersion`: 1 to 32 characters of `A-Z a-z 0-9 . _ - +` and spaces.
- `loader.type`: `fabric`, `quilt`, `forge`, `neoforge` or `vanilla`.
  `loader.version` is 1 to 64 characters of `A-Z a-z 0-9 . _ - +`, required
  for every loader but `vanilla`, which has none.
- `content`: at most 500 entries, each with `kind` (`mod`, `resourcepack`,
  `shaderpack` or `datapack`) and `platform`:
  - `modrinth`: `projectId` and `versionId`, each 1 to 16 letters and digits.
  - `curseforge`: `projectId` and `fileId`, positive 32-bit integers.

  `vanilla` takes no `mod` entries, and the same project may appear only once
  per kind.

The body may be up to 64 KB (every other endpoint takes 8 KB). A player can
keep 50 codes live and make 20 an hour.

```http
GET /v1/share-codes/{code}
```

```json
200
{
  "code": "KITE-7F3K-9QXM",
  "manifest": { "name": "Survival 1.21", "minecraftVersion": "1.21.4", "loader": { ... }, "content": [ ... ] },
  "createdAt": "2026-10-24T18:00:00.000Z",
  "expiresAt": "2027-01-22T18:00:00.000Z"
}
```

Any signed-in player may fetch a code; the answer does not say who made it.
Case, dashes, spaces and the `KITE-` prefix do not matter, so
`kite-7f3k-9qxm` and `7F3K9QXM` find the same code. `400` if it cannot be a
code, `404` if there is no such code or it expired. Limited to 30 fetches an
hour, counted before the lookup.

Codes are 8 characters from `23456789ABCDEFGHJKMNPQRSTUVWXYZ` (no `0`, `O`,
`1`, `I` or `L`), about 8.5 × 10^11 of them. A code lives for 90 days after it
was made or last fetched; every fetch starts the 90 days again.

```http
GET /v1/share-codes
```

```json
200
{
  "codes": [
    { "code": "KITE-7F3K-9QXM", "name": "Survival 1.21", "minecraftVersion": "1.21.4",
      "loader": { "type": "fabric", "version": "0.16.10" },
      "createdAt": "2026-10-24T18:00:00.000Z", "expiresAt": "2027-01-22T18:00:00.000Z" }
  ]
}
```

Your live codes, newest first.

```http
DELETE /v1/share-codes/{code}
```

`204`, or `404` if you have no such code. Someone else's code looks unknown.

### Realtime socket

```http
GET /v1/ws
Upgrade: websocket
Authorization: Bearer kfs_...
```

The launcher and the game mod open one WebSocket each and keep it open. It
is signed in like every other request, with the token in the
`Authorization` header of the upgrade (Qt's `QWebSocket` takes it through
the `QNetworkRequest`, Java's `WebSocket.Builder` through `header`); a
missing or bad token is a plain `401` before any upgrade, and a token in the
URL is not accepted. Once open, the server sends:

```json
{ "type": "ready", "user": { "uuid": "069a79f4-44e9-4726-a5be-fca90e38aaf5", "name": "Notch" } }
```

Every frame either way is a JSON object in a text frame, with a `type`.

- The server pings every 30 seconds. A socket that has not answered the
  previous ping with a pong is dropped, so a dead connection goes within a
  minute. WebSocket libraries answer pings on their own. A client that hears
  nothing (no ping, no event) for 75 seconds should reconnect.
- At most 5 sockets per player; a sixth closes the oldest with `4001`.
- A frame is at most 16 KB (close `1009`). Binary frames close the socket
  with `1003`. A client that does not read what it is sent, so that more
  than 1 MB waits for it, is dropped.
- A socket may send 120 frames a minute. Past that it gets one
  `rate_limited` error, and frames are dropped until the minute is over.
- `4401` means the session ended: logged out, dropped by an eleventh
  sign-in, expired, or the account was deleted. Sign in again before
  reconnecting. For any other close, reconnect with a growing delay (1 s,
  2 s, 4 s, up to a minute); Cloudflare also drops sockets when it updates
  its servers.

After every (re)connect, call `GET /v1/friends` and, for each conversation
the client has open, `GET /v1/chats/{uuid}/messages?after=<newest id it
has>`: events sent while the socket was down are not replayed. The same
message may then arrive both ways; message ids tell them apart.

#### Events

What the server pushes. Statuses, users and invites look exactly as they do
in `GET /v1/friends`.

| `type` | Fields | When |
|---|---|---|
| `friend.status` | `uuid`, `status` | A friend's status as you see it, when a report differs from the last one, when they go offline or their status runs out, and when they change their settings |
| `friend.request` | `user`, `createdAt` | Someone sent you a friend request |
| `friend.accepted` | `user` | Your request was accepted. Fetch `GET /v1/friends` for their status |
| `friend.removed` | `uuid` | You are no longer friends: either of you unfriended or blocked the other, or they deleted their account. Drop their invites with them |
| `invite` | `invite` | A friend invited you; `invite` is an entry of `invites.incoming` |
| `invite.cancelled` | `uuid` | The friend cancelled their invite to you |
| `chat.message` | `message` | A message to you, or one you sent from another socket or over HTTP |
| `chat.read` | `with`, `upTo` | You moved your read marker from another socket or over HTTP |
| `chat.deleted` | `uuid` | That player deleted their account; show their messages as deleted |

Declined requests, dismissed invites and read markers are not pushed to
the other player, as they are not shown to them anywhere else either.

#### Frames the client sends

```json
{ "type": "chat.send", "ref": "c1", "to": "853c80ef-3c37-49fd-aa49-938b674adae6", "text": "gg" }
{ "type": "chat.read", "ref": "c2", "with": "853c80ef-3c37-49fd-aa49-938b674adae6", "upTo": "1042" }
```

They do what `POST /v1/chats/{uuid}/messages` and `POST /v1/chats/{uuid}/read`
do, with the same rules and limits. `ref` is optional, 1 to 64 characters
of `A-Z a-z 0-9 _ -`, and comes back in the answer:

```json
{ "type": "chat.sent", "ref": "c1", "message": { ... } }
{ "type": "chat.read", "ref": "c2", "with": "853c80ef-...", "upTo": "1042" }
{ "type": "error", "ref": "c1", "error": "not_found", "message": "Not friends with this player" }
```

`error` uses the codes in [Errors](#errors); `rate_limited` comes with
`retryAfter` in seconds. A frame that is not JSON, has an unknown `type` or
a field that does not belong is a `bad_request` error, and the socket stays
open. Frames from one socket are handled in order.

### Chat

One-to-one messages between friends. There are no group chats and no
attachments.

```http
POST /v1/chats/{uuid}/messages
{ "text": "gg" }
```

```json
201
{
  "id": "1042",
  "from": "069a79f4-44e9-4726-a5be-fca90e38aaf5",
  "to": "853c80ef-3c37-49fd-aa49-938b674adae6",
  "text": "gg",
  "createdAt": "2026-10-24T18:00:00.000Z",
  "deleted": false
}
```

`{uuid}` is the friend. The same can be sent over the
[socket](#frames-the-client-sends), which is what the launcher should use
while it has one.

- `text`: 1 to 1000 characters, counted in UTF-16 code units the way Java
  and Qt count a string's length (an emoji is usually two). Control and bidi
  characters, line breaks included, are removed and the ends trimmed;
  nothing else is changed, so clients must show `text` as plain text. The
  game mod must not let `§` codes in it format anything.
- `id` is a string of digits; ids only grow.

| Response | Meaning |
|---|---|
| `201` | Sent. It also reaches the friend's sockets and your other ones |
| `400 bad_request` | The text is empty after cleaning or too long, or the uuid is yours |
| `404 not_found` | Not friends with this player (a block either way, an unknown player and a pending request look the same) |
| `429 rate_limited` | More than 30 messages a minute or 2000 a day |

```http
GET /v1/chats/{uuid}/messages
GET /v1/chats/{uuid}/messages?before=1042&limit=50
GET /v1/chats/{uuid}/messages?after=1042
```

```json
200
{ "messages": [ { "id": "1041", ... }, { "id": "1042", ... } ], "more": true }
```

The conversation with that player, oldest first. Without `before` or
`after` it is the newest page; `before` pages back to older messages and
`after` gets the ones after a message you have (the catch-up after a
reconnect). `more` says whether there are further messages past the page
in that direction. `limit` is 1 to 100, 50 by default; `before` and `after`
cannot be used together.

History stays readable after you stop being friends, until it ages out.
While either of you blocks the other, it is empty for both of you. A
message whose sender deleted their account has `"text": null` and
`"deleted": true`.

```http
POST /v1/chats/{uuid}/read
{ "upTo": "1042" }
```

`200 { "upTo": "1042" }`. Marks the conversation read up to that message,
which must be one you can see in it. The marker never moves back; the
answer is where it is now. It drives `unread` in `GET /v1/friends` and is
pushed to your other sockets, not to the friend: there are no read
receipts. Send it when the conversation is on screen, not for every
message.

#### Reports

```http
POST /v1/chats/{uuid}/messages/{id}/report
{ "reason": "harassment" }
```

`204`. Reports a message that player sent you; `reason` is `spam`,
`harassment` or `other`. It works after blocking them too, which is the
usual order. The server keeps a copy of the message, the 20 messages before
it in the conversation (from both of you) and the reported player's name,
for moderation; reporting the same message again changes nothing. `404` if
no such message from that player to you is visible. 20 reports a day.

There is no moderation screen yet. Reports are read in the database:

```sql
select created_at, reason, reported_uuid, snapshot from chat_reports order by created_at desc;
```

### Rate limits

| Scope | Limit |
|---|---|
| Any request, per IP | 300 a minute |
| `auth` endpoints, per IP | 20 a minute |
| Signed-in requests, per user | 120 a minute |
| Friend requests sent, per user | 30 an hour, 3 a day to the same player |
| Player lookups, per user | 30 a minute |
| Invites sent, per user | 30 an hour, 5 an hour to the same friend |
| Share codes made, per user | 20 an hour |
| Share code fetches, per user | 30 an hour |
| Chat messages sent, per user | 30 a minute, 2000 a day |
| Chat reports, per user | 20 a day |
| Socket frames, per socket | 120 a minute |
| Open sockets, per user | 5 |

A refused message or report still counts. The launcher's normal load is
two requests a minute; the socket upgrade counts as one request.

### Health

```http
GET /healthz
```

`200 { "ok": true }` when the database answers.

## Privacy

What the server stores, all of it deleted by `DELETE /v1/me` (except blocks
other players placed on you and reports they filed about your messages):

- Minecraft UUID and name, and when they were last seen
- Settings
- Friendships, pending requests and blocks
- The current status: state, instance name, Minecraft version and world. It
  expires three minutes after the last report and is purged after that.
- Invites: sender, recipient, server address and port. They expire after 10
  minutes and are purged after that; dismissing or cancelling one deletes it
  at once.
- Share codes: the code, who made it and the manifest (name, Minecraft
  version, loader and platform project ids). Kept until the owner deletes it
  or 90 days pass without a fetch; who fetched a code is not recorded.
- Chat messages: sender, recipient, text and time. Deleted 30 days after
  they were sent. Deleting your account deletes the text of what you sent
  at once (the recipient keeps an empty placeholder until the 30 days are
  up) and hides what you received from you.
- Read markers: how far you have read each conversation. They go with the
  messages they point at.
- Chat reports: who reported whom, the reason, and a copy of the reported
  message with up to 20 messages before it and the reported player's name.
  Kept 180 days so a moderator can act on them, even after the messages age
  out or the reported player deletes their account. A report you filed is
  deleted with your account.
- Session tokens, stored only as SHA-256 hashes

Raw IP and local server addresses are not stored unless `shareIpServers` is
on, in statuses and invites alike. Requests and socket frames are not
logged. Client IPs are only kept in memory for rate limiting.

Chat is meant to be safe for younger players: only friends, who both
accepted each other, can write to each other; a block stops messages both
ways and hides the history; any message can be reported.

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
Traefik, and all users share one per-IP limit. Anyone talking to the origin
directly could set `cf-connecting-ip` themselves, so the server believes it
only when `X-Real-Ip`, which Traefik overwrites with the address that
connected to it, is one of Cloudflare's ranges (listed in
`src/clientIp.ts`). A direct request is counted by its real address
whatever it claims. On the Dokploy server Traefik also turns direct
requests for `api.kiteclient.com` away with 418 (the edge guard in
`/etc/dokploy/traefik/dynamic/00-edge-guard.yml`). Without Cloudflare in
front, use `x-real-ip`.

### WebSockets through Cloudflare and Traefik

- Cloudflare proxies WebSockets on every plan, as long as the zone's
  **Network → WebSockets** setting is on (the default). It closes a socket
  that carries no traffic either way for about 100 seconds; the server's
  ping every 30 seconds is traffic, so open sockets stay up. Cloudflare also
  drops sockets when it restarts its own servers, which is one reason
  clients reconnect.
- Traefik passes the upgrade through by itself; the Dokploy domain needs no
  extra middleware. After the first deploy, keep a socket open for five
  minutes (`websocat -H 'Authorization: Bearer kfs_...'
  wss://api.kiteclient.com/v1/ws`). If it drops at a fixed interval, an
  entry point timeout (`respondingTimeouts`) is shorter than the 30-second
  heartbeat allows.
- One container is all it takes: sockets and their events live in that
  process. Running a second instance needs the in-memory event bus
  (`src/bus.ts`) replaced, e.g. by Postgres `LISTEN/NOTIFY`, and the rate
  limits moved out of memory too.
- A deploy restarts the container, which closes every socket; clients
  reconnect and catch up through the history.

`docker-compose.yml` runs the same image with its own Postgres, for a local
run.
