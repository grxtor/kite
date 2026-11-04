import { mkdir, writeFile } from 'node:fs/promises';
import { join } from 'node:path';
import type { CrashSample } from './crashReports.ts';
import type { SignatureRow } from './crashStore.ts';

/*
 * What scripts/crash-top.ts prints and dumps, kept apart from the database so it can be tested.
 * The lead reads the top unknown crashes, writes a rule for one, and resolves its signature.
 */

/** A mod is listed in the overlap when at least this share of the samples has it. */
export const overlapShare = 0.5;
export const maxOverlapMods = 15;
const maxCounted = 5;
const shortSignature = 12;
const percent = 100;

export interface Count {
  value: string;
  count: number;
}

export interface SampleSummary {
  samples: number;
  minecraft: Count[];
  loaders: Count[];
  kite: Count[];
  os: Count[];
  culprits: Count[];
  rules: Count[];
  /** Mods most samples have, with the share of samples that have them. */
  mods: { id: string; share: number; versions: Count[] }[];
}

function counted(values: readonly string[], max = maxCounted): Count[] {
  const counts = new Map<string, number>();
  for (const value of values) counts.set(value, (counts.get(value) ?? 0) + 1);
  return [...counts]
    .map(([value, count]) => ({ value, count }))
    .sort((a, b) => b.count - a.count || a.value.localeCompare(b.value))
    .slice(0, max);
}

function loaderText(sample: CrashSample): string {
  return sample.loader.type === 'vanilla' || !sample.loader.version
    ? sample.loader.type
    : `${sample.loader.type} ${sample.loader.version}`;
}

export function summarize(samples: readonly CrashSample[]): SampleSummary {
  const versionsOf = new Map<string, string[]>();
  for (const sample of samples) {
    for (const mod of sample.mods) versionsOf.set(mod.id, [...(versionsOf.get(mod.id) ?? []), mod.version]);
  }
  const mods = [...versionsOf]
    .map(([id, versions]) => ({ id, share: versions.length / Math.max(1, samples.length), versions: counted(versions, 3) }))
    .filter((mod) => mod.share >= overlapShare)
    .sort((a, b) => b.share - a.share || a.id.localeCompare(b.id))
    .slice(0, maxOverlapMods);
  return {
    samples: samples.length,
    minecraft: counted(samples.map((sample) => sample.minecraft)),
    loaders: counted(samples.map(loaderText)),
    kite: counted(samples.map((sample) => sample.kite)),
    os: counted(samples.map((sample) => `${sample.os} ${sample.arch}`)),
    culprits: counted(samples.flatMap((sample) => (sample.culprit ? [sample.culprit] : []))),
    rules: counted(samples.flatMap((sample) => sample.rules)),
    mods,
  };
}

const countsText = (counts: readonly Count[]) =>
  counts.length === 0 ? '-' : counts.map((one) => `${one.value} ×${one.count}`).join(', ');
const dateText = (date: Date) => date.toISOString().slice(0, 10);

/** One signature as crash-top prints it. */
export function signatureText(rank: number, row: SignatureRow, summary: SampleSummary): string {
  const resolved = row.resolvedRuleId ? `  resolved by ${row.resolvedRuleId}` : '';
  const out = [
    `#${rank}  ${row.signature.slice(0, shortSignature)}  ${row.reportCount} reports  ` +
      `first ${dateText(row.firstSeen)}  last ${dateText(row.lastSeen)}${resolved}`,
    ...row.lines.map((line) => `    ${line}`),
    `    Minecraft: ${countsText(summary.minecraft)}`,
    `    Loader: ${countsText(summary.loaders)}`,
    `    Culprit: ${countsText(summary.culprits)}    Rules matched: ${countsText(summary.rules)}`,
    `    Mods in most of the ${summary.samples} samples: ` +
      (summary.mods.length === 0
        ? '-'
        : summary.mods.map((mod) => `${mod.id} ${Math.round(mod.share * percent)}%`).join(', ')),
  ];
  return out.join('\n');
}

/**
 * The lines as the crash report would show them (frames indented with a tab), for a rule's
 * patterns. The real file also has line numbers and jar names that the lines left out.
 */
export function crashReportText(lines: readonly string[]): string {
  return lines.map((line) => (line.startsWith('at ') ? `\t${line}` : line)).join('\n') + '\n';
}

/**
 * Writes @p row and its samples under @p dir/<signature start>/: lines.txt, summary.json,
 * samples/<n>.json, and case.json, a synthetic crash doctor case (testdata/synthetic) to start the
 * rule's test from. Returns the folder.
 */
export async function dumpSignature(
  dir: string,
  row: SignatureRow,
  samples: readonly CrashSample[],
): Promise<string> {
  const folder = join(dir, row.signature.slice(0, shortSignature));
  await mkdir(join(folder, 'samples'), { recursive: true });
  const text = crashReportText(row.lines);
  await writeFile(join(folder, 'lines.txt'), text);
  await writeFile(join(folder, 'summary.json'), JSON.stringify({ ...row, summary: summarize(samples) }, null, 1) + '\n');
  await Promise.all(
    samples.map((sample, index) =>
      writeFile(join(folder, 'samples', `${index + 1}.json`), JSON.stringify(sample, null, 1) + '\n'),
    ),
  );
  const note = `crash-top ${row.signature.slice(0, shortSignature)}, ${row.reportCount} reports`;
  await writeFile(join(folder, 'case.json'), JSON.stringify({ note, verify: false, docs: { crashReport: text } }, null, 1) + '\n');
  return folder;
}
