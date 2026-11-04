import { createHash } from 'node:crypto';
import { badRequest } from './errors.ts';

/*
 * A crash report a player chose to send (POST /v1/crash-reports), checked field by field like a
 * share code manifest: a field that is not listed is refused, not dropped. The lines are the
 * launcher's normalized error lines (docs/crash-doctor.md, "Crash reports"); the signature is
 * their SHA-256 and is computed again here, so two reports with one signature carry one crash.
 */

export const crashReportFormatVersion = 1;
export const maxCrashLines = 24;
/** In UTF-16 code units, the way Qt counts a string's length. */
export const maxCrashLineLength = 300;
export const maxCrashMods = 500;
export const maxCrashRules = 20;

const osNames = ['macos', 'windows', 'linux'] as const;
const archNames = ['arm64', 'x86_64'] as const;
const loaderTypes = ['vanilla', 'fabric', 'quilt', 'forge', 'neoforge'] as const;
const reportFields = [
  'formatVersion', 'signature', 'kite', 'os', 'arch', 'java', 'minecraft', 'loader', 'rules', 'culprit', 'mods', 'lines',
] as const;
const minJava = 5;
const maxJava = 99;

const signaturePattern = /^[0-9a-f]{64}$/;
const kitePattern = /^[A-Za-z0-9._+-]{1,32}$/;
const minecraftPattern = /^[A-Za-z0-9._+ -]{1,32}$/;
const loaderVersionPattern = /^[A-Za-z0-9._+-]{1,64}$/;
const modIdPattern = /^[a-z0-9][a-z0-9_.-]{0,63}$/;
const modVersionPattern = /^[A-Za-z0-9._+-]{0,64}$/;
// The crash doctor's rule ids (crash-rules.json).
const ruleIdPattern = /^[a-z0-9]+(\.[a-z0-9-]+)+$/;
const maxRuleIdLength = 64;
const controlOrBidi = /[\u0000-\u001f\u007f-\u009f‎‏‪-‮⁦-⁩]/;

/*
 * What the launcher's scrubber replaces; a line that still has one was not scrubbed, so the whole
 * report is refused rather than stored.
 */
const unscrubbed: readonly RegExp[] = [
  /[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}/i,
  /(?<![\w.])\d{1,3}(?:\.\d{1,3}){3}(?![\w.])/,
  /\b(?:[0-9a-f]{1,4}:){5,7}[0-9a-f]{1,4}\b/i,
  /(?:^|[\s"'(=:[/])(?:\/Users\/|\/home\/|[A-Za-z]:[\\/](?:Users|Documents and Settings)[\\/])/i,
  /[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}(?![\w/(.-])/,
];

type OsName = (typeof osNames)[number];
type ArchName = (typeof archNames)[number];
type LoaderType = (typeof loaderTypes)[number];

export type CrashLoader = { type: 'vanilla' } | { type: Exclude<LoaderType, 'vanilla'>; version?: string };

export interface CrashMod {
  id: string;
  version: string;
}

export interface CrashReport {
  formatVersion: number;
  signature: string;
  kite: string;
  os: OsName;
  arch: ArchName;
  java: number | null;
  minecraft: string;
  loader: CrashLoader;
  rules: string[];
  culprit?: string;
  mods: CrashMod[];
  lines: string[];
}

/** What is kept of each report beside its signature's lines: everything but the lines. */
export type CrashSample = Omit<CrashReport, 'signature' | 'lines'>;

/** The SHA-256, in lowercase hex, of the UTF-8 lines joined by "\n". */
export function crashSignature(lines: readonly string[]): string {
  return createHash('sha256').update(lines.join('\n'), 'utf8').digest('hex');
}

function object(value: unknown, what: string, allowed: readonly string[]): Record<string, unknown> {
  if (typeof value !== 'object' || value === null || Array.isArray(value)) throw badRequest(`${what} must be an object`);
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

function list(value: unknown, what: string, max: number): unknown[] {
  if (!Array.isArray(value)) throw badRequest(`${what} must be an array`);
  if (value.length > max) throw badRequest(`${what} has more than ${max} entries`);
  return value;
}

function unique(values: readonly string[], what: string): void {
  if (new Set(values).size !== values.length) throw badRequest(`${what} lists the same entry twice`);
}

function parseJava(value: unknown): number | null {
  if (value === null) return null;
  if (typeof value !== 'number' || !Number.isInteger(value) || value < minJava || value > maxJava) {
    throw badRequest(`java must be null or an integer from ${minJava} to ${maxJava}`);
  }
  return value;
}

function parseLoader(value: unknown): CrashLoader {
  const raw = object(value, 'loader', ['type', 'version']);
  const type = oneOf(raw.type, 'loader.type', loaderTypes);
  if (type === 'vanilla') {
    if (raw.version !== undefined) throw badRequest('vanilla has no loader version');
    return { type };
  }
  if (raw.version === undefined) return { type };
  return { type, version: matching(raw.version, 'loader.version', loaderVersionPattern) };
}

function parseMod(value: unknown, index: number): CrashMod {
  const what = `mods[${index}]`;
  const raw = object(value, what, ['id', 'version']);
  return { id: matching(raw.id, `${what}.id`, modIdPattern), version: matching(raw.version, `${what}.version`, modVersionPattern) };
}

function parseRule(value: unknown, index: number): string {
  const rule = matching(value, `rules[${index}]`, ruleIdPattern);
  if (rule.length > maxRuleIdLength) throw badRequest(`rules[${index}] is too long`);
  return rule;
}

function parseLine(value: unknown, index: number): string {
  const what = `lines[${index}]`;
  if (typeof value !== 'string' || value.length === 0 || value.length > maxCrashLineLength) {
    throw badRequest(`${what} must be 1 to ${maxCrashLineLength} characters`);
  }
  if (controlOrBidi.test(value)) throw badRequest(`${what} has a control character`);
  if (unscrubbed.some((pattern) => pattern.test(value))) throw badRequest(`${what} still holds something personal`);
  return value;
}

/** Checks a sent report field by field and returns a fresh copy holding only known fields. */
export function parseCrashReport(body: unknown): CrashReport {
  const raw = object(body, 'report', reportFields);
  if (raw.formatVersion !== crashReportFormatVersion) {
    throw badRequest(`formatVersion must be ${crashReportFormatVersion}`);
  }
  const lines = list(raw.lines, 'lines', maxCrashLines).map(parseLine);
  if (lines.length === 0) throw badRequest('lines is empty');
  const signature = matching(raw.signature, 'signature', signaturePattern);
  if (signature !== crashSignature(lines)) throw badRequest('signature does not match the lines');

  const loader = parseLoader(raw.loader);
  const mods = list(raw.mods, 'mods', maxCrashMods).map(parseMod);
  unique(mods.map((mod) => mod.id), 'mods');
  if (loader.type === 'vanilla' && mods.length > 0) throw badRequest('vanilla loads no mods');
  const rules = list(raw.rules, 'rules', maxCrashRules).map(parseRule);
  unique(rules, 'rules');

  const report: CrashReport = {
    formatVersion: crashReportFormatVersion,
    signature,
    kite: matching(raw.kite, 'kite', kitePattern),
    os: oneOf(raw.os, 'os', osNames),
    arch: oneOf(raw.arch, 'arch', archNames),
    java: parseJava(raw.java),
    minecraft: matching(raw.minecraft, 'minecraft', minecraftPattern),
    loader,
    rules,
    mods,
    lines,
  };
  if (raw.culprit !== undefined) report.culprit = matching(raw.culprit, 'culprit', modIdPattern);
  return report;
}

export function sampleOf(report: CrashReport): CrashSample {
  const { formatVersion, kite, os, arch, java, minecraft, loader, rules, culprit, mods } = report;
  return { formatVersion, kite, os, arch, java, minecraft, loader, rules, ...(culprit === undefined ? {} : { culprit }), mods };
}
