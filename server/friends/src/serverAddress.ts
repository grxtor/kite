import { BlockList, isIPv4, isIPv6 } from 'node:net';

export type AddressKind = 'domain' | 'ip' | 'local';

const maxHostLength = 253;
const labelPattern = /^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$/;
// Reserved and home-network names (RFC 2606, 6761, 8375) and common router domains.
const localSuffixes = [
  'localhost', 'localdomain', 'local', 'lan', 'home', 'internal', 'intranet', 'corp', 'private',
  'home.arpa', 'test', 'invalid', 'example', 'fritz.box', 'mshome.net', 'localtest.me', 'lvh.me',
];
// Wildcard DNS services whose names are an IP address in disguise.
const ipEncodingSuffixes = ['nip.io', 'sslip.io', 'xip.io', 'traefik.me'];

const localRanges = new BlockList();
for (const [network, prefix] of [
  ['0.0.0.0', 8],
  ['10.0.0.0', 8],
  ['100.64.0.0', 10],
  ['127.0.0.0', 8],
  ['169.254.0.0', 16],
  ['172.16.0.0', 12],
  ['192.168.0.0', 16],
] as const) {
  localRanges.addSubnet(network, prefix, 'ipv4');
}
localRanges.addAddress('::', 'ipv6');
localRanges.addAddress('::1', 'ipv6');
localRanges.addSubnet('fc00::', 7, 'ipv6');
localRanges.addSubnet('fe80::', 10, 'ipv6');

/** The canonical form of a host from the game log, or null if it is not a DNS name or IP literal. */
export function normalizeHost(raw: string): string | null {
  let host = raw.trim().toLowerCase();
  if (host.startsWith('[') && host.endsWith(']')) host = host.slice(1, -1);
  if (isIPv4(host) || isIPv6(host)) return host;
  if (host.endsWith('.')) host = host.slice(0, -1);
  if (host.length === 0 || host.length > maxHostLength) return null;
  const labels = host.split('.');
  if (!labels.every((label) => labelPattern.test(label))) return null;
  // A numeric last label makes resolvers read the whole name as an IPv4 in some
  // other notation (2130706433, 0x7f.0x1, 127.0.0.0x1), or it is malformed (1.2.3).
  if (/^(?:\d+|0x[0-9a-f]*)$/.test(labels.at(-1) ?? '')) return null;
  return host;
}

function mappedIPv4(host: string): string | null {
  const dotted = /^::ffff:(\d+\.\d+\.\d+\.\d+)$/.exec(host);
  if (dotted?.[1] && isIPv4(dotted[1])) return dotted[1];
  const hex = /^::ffff:([0-9a-f]{1,4}):([0-9a-f]{1,4})$/.exec(host);
  if (!hex?.[1] || !hex[2]) return null;
  const high = parseInt(hex[1], 16);
  const low = parseInt(hex[2], 16);
  return [high >> 8, high & 0xff, low >> 8, low & 0xff].join('.');
}

function embedsIPv4(host: string): boolean {
  const parts = host.split(/[.-]/);
  let run = 0;
  for (const part of parts) {
    run = /^\d{1,3}$/.test(part) && Number(part) <= 255 ? run + 1 : 0;
    if (run === 4) return true;
  }
  return false;
}

/** How private a normalized host is: a public name, a raw IP, or something on a home network. */
export function classifyHost(host: string): AddressKind {
  if (isIPv4(host)) return localRanges.check(host, 'ipv4') ? 'local' : 'ip';
  if (isIPv6(host)) {
    const v4 = mappedIPv4(host);
    if (v4) return localRanges.check(v4, 'ipv4') ? 'local' : 'ip';
    return localRanges.check(host, 'ipv6') ? 'local' : 'ip';
  }
  if (!host.includes('.')) return 'local';
  const under = (suffix: string) => host === suffix || host.endsWith(`.${suffix}`);
  if (localSuffixes.some(under)) return 'local';
  if (ipEncodingSuffixes.some(under) || embedsIPv4(host)) return 'ip';
  return 'domain';
}
