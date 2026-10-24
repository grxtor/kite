export interface RateLimits {
  ipPerMinute: number;
  authPerMinute: number;
  userPerMinute: number;
  friendRequestsPerHour: number;
  friendRequestsPerPairPerDay: number;
  lookupsPerMinute: number;
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
  rateLimits: RateLimits;
}

export const defaultRateLimits: RateLimits = {
  ipPerMinute: 300,
  authPerMinute: 20,
  userPerMinute: 120,
  friendRequestsPerHour: 30,
  friendRequestsPerPairPerDay: 3,
  lookupsPerMinute: 30,
};

function positiveInt(env: Record<string, string | undefined>, key: string, fallback: number): number {
  const raw = env[key];
  if (raw === undefined || raw === '') return fallback;
  const value = Number(raw);
  if (!Number.isInteger(value) || value <= 0) throw new Error(`${key} must be a positive integer`);
  return value;
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
    rateLimits: { ...defaultRateLimits },
  };
}
