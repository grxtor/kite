import { randomInt } from 'node:crypto';
import { badRequest } from './errors.ts';
import { cleanText } from './status.ts';

// No 0, O, 1, I or L, so a code read aloud or off a screen cannot be typed wrong.
export const shareCodeAlphabet = '23456789ABCDEFGHJKMNPQRSTUVWXYZ';
export const shareCodeLength = 8;
const shareCodePrefix = 'KITE';
const codeBodyPattern = new RegExp(`^[${shareCodeAlphabet}]{${shareCodeLength}}$`);

export const maxContentEntries = 500;

const loaderTypes = ['fabric', 'quilt', 'forge', 'neoforge', 'vanilla'] as const;
const contentKinds = ['mod', 'resourcepack', 'shaderpack', 'datapack'] as const;
const platforms = ['modrinth', 'curseforge'] as const;
// Content points at a platform's project and file by id only, never at a URL or a path.
const entryFields = {
  modrinth: ['kind', 'platform', 'projectId', 'versionId'],
  curseforge: ['kind', 'platform', 'projectId', 'fileId'],
} as const;
const anyEntryField = [...new Set(Object.values(entryFields).flat())];
const maxNameLength = 64;
const minecraftVersionPattern = /^[A-Za-z0-9._+ -]{1,32}$/;
const loaderVersionPattern = /^[A-Za-z0-9._+-]{1,64}$/;
const modrinthIdPattern = /^[A-Za-z0-9]{1,16}$/;
// CurseForge ids are 32-bit.
const maxCurseForgeId = 2_147_483_647;

type LoaderType = (typeof loaderTypes)[number];
type ContentKind = (typeof contentKinds)[number];

export type Loader = { type: 'vanilla' } | { type: Exclude<LoaderType, 'vanilla'>; version: string };

export type ContentEntry =
  | { kind: ContentKind; platform: 'modrinth'; projectId: string; versionId: string }
  | { kind: ContentKind; platform: 'curseforge'; projectId: number; fileId: number };

export interface Manifest {
  name: string;
  minecraftVersion: string;
  loader: Loader;
  content: ContentEntry[];
}

export function newShareCode(): string {
  let code = '';
  for (let i = 0; i < shareCodeLength; i += 1) code += shareCodeAlphabet[randomInt(shareCodeAlphabet.length)];
  return code;
}

/** `KITE-7F3K-9QXM` for the stored `7F3K9QXM`. */
export function displayShareCode(code: string): string {
  return `${shareCodePrefix}-${code.slice(0, 4)}-${code.slice(4)}`;
}

/** The stored form of a code as a person typed it, or null if it cannot be one. */
export function normalizeShareCode(raw: string): string | null {
  const compact = raw.toUpperCase().replace(/[\s-]/g, '');
  // I is not in the alphabet, so no code starts with KITE by itself.
  const body = compact.startsWith(shareCodePrefix) ? compact.slice(shareCodePrefix.length) : compact;
  return codeBodyPattern.test(body) ? body : null;
}

function object(value: unknown, what: string, allowed: readonly string[]): Record<string, unknown> {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) throw badRequest(`${what} must be an object`);
  // Anything not listed (a URL, a path, file bytes) is refused rather than dropped.
  if (Object.keys(value).some((key) => !allowed.includes(key))) throw badRequest(`${what} has a field that is not allowed`);
  return value as Record<string, unknown>;
}

function matching(value: unknown, what: string, pattern: RegExp): string {
  if (typeof value !== 'string' || !pattern.test(value)) throw badRequest(`${what} is not valid`);
  return value;
}

function oneOf<T extends string>(value: unknown, what: string, options: readonly T[]): T {
  if (typeof value !== 'string' || !options.includes(value as T)) throw badRequest(`${what} must be one of ${options.join(', ')}`);
  return value as T;
}

function curseForgeId(value: unknown, what: string): number {
  if (typeof value !== 'number' || !Number.isInteger(value) || value < 1 || value > maxCurseForgeId) {
    throw badRequest(`${what} must be a positive integer`);
  }
  return value;
}

function parseLoader(value: unknown): Loader {
  const raw = object(value, 'loader', ['type', 'version']);
  const type = oneOf(raw.type, 'loader.type', loaderTypes);
  if (type === 'vanilla') {
    if (raw.version !== undefined) throw badRequest('vanilla has no loader version');
    return { type };
  }
  return { type, version: matching(raw.version, 'loader.version', loaderVersionPattern) };
}

function parseEntry(value: unknown, index: number): ContentEntry {
  const what = `content[${index}]`;
  const platform = oneOf(object(value, what, anyEntryField).platform, `${what}.platform`, platforms);
  const raw = object(value, what, entryFields[platform]);
  const kind = oneOf(raw.kind, `${what}.kind`, contentKinds);
  if (platform === 'modrinth') {
    return {
      kind,
      platform,
      projectId: matching(raw.projectId, `${what}.projectId`, modrinthIdPattern),
      versionId: matching(raw.versionId, `${what}.versionId`, modrinthIdPattern),
    };
  }
  return {
    kind,
    platform,
    projectId: curseForgeId(raw.projectId, `${what}.projectId`),
    fileId: curseForgeId(raw.fileId, `${what}.fileId`),
  };
}

/** Checks an uploaded manifest field by field and returns a fresh copy holding only known fields. */
export function parseManifest(body: unknown): Manifest {
  const raw = object(body, 'manifest', ['name', 'minecraftVersion', 'loader', 'content']);
  const name = typeof raw.name === 'string' ? cleanText(raw.name) : '';
  if (name.length === 0 || name.length > maxNameLength) throw badRequest(`name must be 1 to ${maxNameLength} characters`);
  const minecraftVersion = matching(raw.minecraftVersion, 'minecraftVersion', minecraftVersionPattern);
  const loader = parseLoader(raw.loader);

  if (!Array.isArray(raw.content)) throw badRequest('content must be an array');
  if (raw.content.length > maxContentEntries) throw badRequest(`content has more than ${maxContentEntries} entries`);
  const content = raw.content.map(parseEntry);

  if (loader.type === 'vanilla' && content.some((entry) => entry.kind === 'mod')) {
    throw badRequest('vanilla cannot load mods');
  }
  const seen = new Set<string>();
  for (const entry of content) {
    const key = `${entry.platform}:${entry.kind}:${entry.projectId}`;
    if (seen.has(key)) throw badRequest('content lists the same project twice');
    seen.add(key);
  }
  return { name, minecraftVersion, loader, content };
}
