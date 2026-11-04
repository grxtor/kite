export interface RateLimits {
  ipPerMinute: number;
  authPerMinute: number;
  userPerMinute: number;
  friendRequestsPerHour: number;
  friendRequestsPerPairPerDay: number;
  lookupsPerMinute: number;
  invitesPerHour: number;
  invitesPerPairPerHour: number;
  shareCodesPerHour: number;
  shareCodeFetchesPerHour: number;
  chatMessagesPerMinute: number;
  chatMessagesPerDay: number;
  chatReportsPerDay: number;
  socketFramesPerMinute: number;
  crashReportsPerHour: number;
}

export interface Config {
  port: number;
  host: string;
  databaseUrl: string;
  mojangSessionUrl: string;
  clientIpHeader: string | null;
  logLevel: string;
  statusTtlSeconds: number;
  sessionTtlDays: number;
  challengeTtlSeconds: number;
  /** How often the server pings each WebSocket; one missed pong closes it. */
  heartbeatMs: number;
  /** Discord webhook told about each new chat report; null sends nothing. A secret. */
  modWebhookUrl: string | null;
  rateLimits: RateLimits;
}

export const defaultRateLimits: RateLimits = {
  ipPerMinute: 300,
  authPerMinute: 20,
  userPerMinute: 120,
  friendRequestsPerHour: 30,
  friendRequestsPerPairPerDay: 3,
  lookupsPerMinute: 30,
  invitesPerHour: 30,
  invitesPerPairPerHour: 5,
  shareCodesPerHour: 20,
  shareCodeFetchesPerHour: 30,
  chatMessagesPerMinute: 30,
  chatMessagesPerDay: 2000,
  chatReportsPerDay: 20,
  socketFramesPerMinute: 120,
  crashReportsPerHour: 20,
};

function positiveInt(env: Record<string, string | undefined>, key: string, fallback: number): number {
  const raw = env[key];
  if (raw === undefined || raw === '') return fallback;
  const value = Number(raw);
  if (!Number.isInteger(value) || value <= 0) throw new Error(`${key} must be a positive integer`);
  return value;
}

function webhookUrl(raw: string | undefined): string | null {
  if (raw === undefined || raw === '') return null;
  let url: URL;
  try {
    url = new URL(raw);
  } catch {
    // Not the value itself: it is a secret.
    throw new Error('MOD_WEBHOOK_URL is not a URL');
  }
  if (url.protocol !== 'https:' && url.protocol !== 'http:') throw new Error('MOD_WEBHOOK_URL must be http(s)');
  // fetch would refuse it with an error that repeats the whole URL.
  if (url.username || url.password) throw new Error('MOD_WEBHOOK_URL must not contain credentials');
  return url.toString();
}

export function loadConfig(env: Record<string, string | undefined>): Config {
  const databaseUrl = env.DATABASE_URL;
  if (!databaseUrl) throw new Error('DATABASE_URL is required');
  return {
    port: positiveInt(env, 'PORT', 8080),
    host: env.HOST || '0.0.0.0',
    databaseUrl,
    mojangSessionUrl: (env.MOJANG_SESSION_URL || 'https://sessionserver.mojang.com').replace(/\/+$/, ''),
    clientIpHeader: env.CLIENT_IP_HEADER ? env.CLIENT_IP_HEADER.toLowerCase() : null,
    logLevel: env.LOG_LEVEL || 'info',
    statusTtlSeconds: positiveInt(env, 'STATUS_TTL_SECONDS', 180),
    sessionTtlDays: positiveInt(env, 'SESSION_TTL_DAYS', 30),
    challengeTtlSeconds: 120,
    heartbeatMs: 30_000,
    modWebhookUrl: webhookUrl(env.MOD_WEBHOOK_URL),
    rateLimits: { ...defaultRateLimits },
  };
}
