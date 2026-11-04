import { crashSignature } from '../src/crashReports.ts';

export const crashLines = [
  'java.lang.NullPointerException: Cannot invoke "net.minecraft.world.level.Level.getBlockState(<n>)" because "<local4>" is null',
  'at com.simibubi.create.content.trains.Carriage.tick(Carriage.java)',
  'at net.minecraft.server.level.ServerLevel.tick(ServerLevel.java)',
];

/** A valid report body; @p lines and @p extra change it. */
export function crashBody(lines: string[] = crashLines, extra: Record<string, unknown> = {}): Record<string, unknown> {
  return {
    formatVersion: 1,
    signature: crashSignature(lines),
    kite: '1.2.0',
    os: 'macos',
    arch: 'arm64',
    java: 17,
    minecraft: '1.20.1',
    loader: { type: 'forge', version: '47.3.0' },
    rules: [],
    culprit: 'create',
    mods: [
      { id: 'create', version: '0.5.1.j' },
      { id: 'flywheel', version: '0.6.11-13' },
    ],
    lines,
    ...extra,
  };
}

/** Lines of another crash, different for each @p n. */
export function otherLines(n: number): string[] {
  return [`java.lang.IllegalStateException: Crash number ${'x'.repeat(n + 1)}`, 'at net.minecraft.client.Minecraft.run(Minecraft.java)'];
}
