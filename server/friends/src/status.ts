import { classifyHost } from './serverAddress.ts';

export const defaultMinecraftPort = 25565;

export type World = { type: 'server'; host?: string; port?: number } | { type: 'singleplayer' };
export type Visibility = 'everyone' | 'friends' | 'invisible';

export interface PrivacySettings {
  hideServer: boolean;
  shareIpServers: boolean;
}

export type StatusView =
  | { state: 'offline'; lastSeenAt: string | null }
  | { state: 'online' }
  | { state: 'playing'; instance: string; version: string; world: World | null; joinable: boolean };

/** A user joined with their live presence; the presence columns are null when there is none. */
export interface PresenceRow {
  visibility: Visibility;
  hide_server: boolean;
  share_ip_servers: boolean;
  allow_join: boolean;
  last_seen_at: Date | null;
  state: 'online' | 'playing' | null;
  instance: string | null;
  version: string | null;
  world: World | null;
}

/** Selects the columns of PresenceRow for users `u` left-joined to live presence `p`. */
export const presenceColumns = `u.visibility, u.hide_server, u.share_ip_servers, u.allow_join, u.last_seen_at,
  p.state, p.instance, p.version, p.world`;

/** The part of a world others may see under the owner's current settings. */
export function visibleWorld(world: World | null, settings: PrivacySettings): World | null {
  if (!world || settings.hideServer) return null;
  if (world.type === 'singleplayer' || world.host === undefined) return world;
  if (classifyHost(world.host) !== 'domain' && !settings.shareIpServers) return { type: 'server' };
  return { type: 'server', host: world.host, port: world.port ?? defaultMinecraftPort };
}

export function statusView(row: PresenceRow): StatusView {
  if (row.visibility === 'invisible') return { state: 'offline', lastSeenAt: null };
  if (row.state === 'online') return { state: 'online' };
  if (row.state === 'playing') {
    const settings = { hideServer: row.hide_server, shareIpServers: row.share_ip_servers };
    const world = visibleWorld(row.world, settings);
    return {
      state: 'playing',
      instance: row.instance ?? '',
      version: row.version ?? '',
      world,
      // A friend can only join a server whose address they can see.
      joinable: row.allow_join && world?.type === 'server' && world.host !== undefined,
    };
  }
  return { state: 'offline', lastSeenAt: row.last_seen_at?.toISOString() ?? null };
}

// C0/C1 controls and bidi overrides, which could make one name look like another.
const unsafeCharacters = /[\u0000-\u001f\u007f-\u009f\u200e\u200f\u202a-\u202e\u2066-\u2069]/g;

export function cleanText(text: string): string {
  return text.replace(unsafeCharacters, '').trim();
}
