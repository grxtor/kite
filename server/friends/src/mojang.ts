import { HttpError } from './errors.ts';

export interface MojangProfile {
  uuid: string;
  name: string;
}

const timeoutMs = 5000;

function dashed(id: string): string {
  return `${id.slice(0, 8)}-${id.slice(8, 12)}-${id.slice(12, 16)}-${id.slice(16, 20)}-${id.slice(20)}`;
}

function unavailable(): HttpError {
  return new HttpError(502, 'mojang_unavailable', "Mojang's session server did not answer");
}

/** Asks Mojang whether `name` announced a join to `serverId`; null when it did not. */
export async function hasJoined(sessionUrl: string, name: string, serverId: string): Promise<MojangProfile | null> {
  const url = new URL(`${sessionUrl}/session/minecraft/hasJoined`);
  url.searchParams.set('username', name);
  url.searchParams.set('serverId', serverId);

  let response: Response;
  try {
    response = await fetch(url, { signal: AbortSignal.timeout(timeoutMs) });
  } catch {
    throw unavailable();
  }
  if (response.status === 204) return null;
  if (response.status !== 200) throw unavailable();

  const body: unknown = await response.json().catch(() => null);
  if (typeof body !== 'object' || body === null) throw unavailable();
  const { id, name: profileName } = body as Record<string, unknown>;
  if (typeof id !== 'string' || !/^[0-9a-f]{32}$/i.test(id) || typeof profileName !== 'string') {
    throw unavailable();
  }
  return { uuid: dashed(id.toLowerCase()), name: profileName };
}
