import type { FastifyInstance } from 'fastify';
import { currentUser, type Context } from '../context.ts';
import { withTransaction } from '../db.ts';
import { badRequest } from '../errors.ts';
import { settingsBody, statusBody } from '../schemas.ts';
import { normalizeHost } from '../serverAddress.ts';
import { cleanText, defaultMinecraftPort, statusView, visibleWorld, type Visibility, type World } from '../status.ts';

interface SettingsRow {
  visibility: Visibility;
  hide_server: boolean;
  share_ip_servers: boolean;
}

interface SettingsPatch {
  visibility?: Visibility;
  hideServer?: boolean;
  shareIpServers?: boolean;
}

interface StatusBody {
  state: 'online' | 'playing';
  instance?: string;
  version?: string;
  world?: { type: 'server' | 'singleplayer'; host?: string; port?: number } | null;
}

const settingsColumns = 'visibility, hide_server, share_ip_servers';

function settingsJson(row: SettingsRow) {
  return { visibility: row.visibility, hideServer: row.hide_server, shareIpServers: row.share_ip_servers };
}

/** Checks which fields go together and turns the reported world into the one to store. */
function parseWorld(body: StatusBody): World | null {
  if (body.state === 'online') {
    if (body.instance !== undefined || body.version !== undefined || body.world) {
      throw badRequest('instance, version and world only go with state "playing"');
    }
    return null;
  }
  if (body.instance === undefined || body.version === undefined) {
    throw badRequest('state "playing" needs instance and version');
  }
  const world = body.world;
  if (!world) return null;
  if (world.type === 'singleplayer') return { type: 'singleplayer' };
  if (world.host === undefined) throw badRequest('a server world needs a host');
  const host = normalizeHost(world.host);
  if (!host) throw badRequest('host is not a valid server address');
  return { type: 'server', host, port: world.port ?? defaultMinecraftPort };
}

export function registerMeRoutes(app: FastifyInstance, ctx: Context) {
  app.get('/v1/me', async (request) => {
    const user = currentUser(request);
    const { rows } = await ctx.pool.query<SettingsRow>(`select ${settingsColumns} from users where uuid = $1`, [
      user.uuid,
    ]);
    return { uuid: user.uuid, name: user.name, settings: settingsJson(rows[0]!) };
  });

  app.patch<{ Body: SettingsPatch }>('/v1/me/settings', { schema: { body: settingsBody } }, async (request) => {
    const user = currentUser(request);
    const { visibility, hideServer, shareIpServers } = request.body;
    return withTransaction(ctx.pool, async (client) => {
      const { rows } = await client.query<SettingsRow>(
        `update users set visibility = coalesce($2, visibility), hide_server = coalesce($3, hide_server),
           share_ip_servers = coalesce($4, share_ip_servers)
         where uuid = $1 returning ${settingsColumns}`,
        [user.uuid, visibility ?? null, hideServer ?? null, shareIpServers ?? null],
      );
      const settings = rows[0]!;
      // Invisible means nothing about the player's activity is kept.
      if (settings.visibility === 'invisible') {
        await client.query('delete from presence where user_uuid = $1', [user.uuid]);
        await client.query('update users set last_seen_at = null where uuid = $1', [user.uuid]);
      }
      return settingsJson(settings);
    });
  });

  app.delete('/v1/me', async (request, reply) => {
    await ctx.pool.query('delete from users where uuid = $1', [currentUser(request).uuid]);
    return reply.status(204).send();
  });

  app.put<{ Body: StatusBody }>('/v1/me/status', { schema: { body: statusBody } }, async (request) => {
    const user = currentUser(request);
    const body = request.body;
    const reported = parseWorld(body);
    const instance = body.state === 'playing' ? cleanText(body.instance ?? '') : null;
    if (instance === '') throw badRequest('instance is empty');
    const version = body.state === 'playing' ? (body.version ?? null) : null;

    const now = ctx.now();
    const expiresAt = new Date(now.getTime() + ctx.config.statusTtlSeconds * 1000);
    // The row lock keeps a settings change from slipping in between reading and writing.
    const status = await withTransaction(ctx.pool, async (client) => {
      const { rows } = await client.query<SettingsRow>(
        `select ${settingsColumns} from users where uuid = $1 for update`,
        [user.uuid],
      );
      const settings = rows[0]!;
      if (settings.visibility === 'invisible') {
        await client.query('delete from presence where user_uuid = $1', [user.uuid]);
        return { state: 'offline', lastSeenAt: null } as const;
      }

      // Stored already filtered, so a hidden or private address never reaches the database.
      const privacy = { hideServer: settings.hide_server, shareIpServers: settings.share_ip_servers };
      const world = visibleWorld(reported, privacy);
      await client.query(
        `insert into presence (user_uuid, state, instance, version, world, expires_at) values ($1, $2, $3, $4, $5, $6)
         on conflict (user_uuid) do update set state = excluded.state, instance = excluded.instance,
           version = excluded.version, world = excluded.world, expires_at = excluded.expires_at`,
        [user.uuid, body.state, instance, version, world ? JSON.stringify(world) : null, expiresAt],
      );
      await client.query('update users set last_seen_at = $2 where uuid = $1', [user.uuid, now]);
      return statusView({ ...settings, last_seen_at: now, state: body.state, instance, version, world });
    });
    return { expiresAt: expiresAt.toISOString(), status };
  });

  app.delete('/v1/me/status', async (request, reply) => {
    const user = currentUser(request);
    await withTransaction(ctx.pool, async (client) => {
      const removed = await client.query('delete from presence where user_uuid = $1', [user.uuid]);
      if (removed.rowCount) {
        await client.query('update users set last_seen_at = $2 where uuid = $1', [user.uuid, ctx.now()]);
      }
    });
    return reply.status(204).send();
  });
}
