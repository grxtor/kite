import { BlockList, isIPv4, isIPv6 } from 'node:net';
import type { FastifyRequest } from 'fastify';
import type { Config } from './config.ts';

// Cloudflare's edge addresses (https://www.cloudflare.com/ips/, rarely changes).
// Only a request that came through one of them can carry a client IP header worth believing.
const cloudflareRanges = new BlockList();
for (const range of [
  '173.245.48.0/20', '103.21.244.0/22', '103.22.200.0/22', '103.31.4.0/22', '141.101.64.0/18',
  '108.162.192.0/18', '190.93.240.0/20', '188.114.96.0/20', '197.234.240.0/22', '198.41.128.0/17',
  '162.158.0.0/15', '104.16.0.0/13', '104.24.0.0/14', '172.64.0.0/13', '131.0.72.0/22',
]) {
  const [network = '', prefix] = range.split('/');
  cloudflareRanges.addSubnet(network, Number(prefix), 'ipv4');
}
for (const range of [
  '2400:cb00::/32', '2606:4700::/32', '2803:f800::/32', '2405:b500::/32', '2405:8100::/32',
  '2a06:98c0::/29', '2c0f:f248::/32',
]) {
  const [network = '', prefix] = range.split('/');
  cloudflareRanges.addSubnet(network, Number(prefix), 'ipv6');
}

// Traefik drops a client-sent X-Real-Ip and writes the address that connected to it.
const proxyPeerHeader = 'x-real-ip';

type HeaderValue = string | string[] | undefined;

function firstValue(header: HeaderValue): string | undefined {
  return (Array.isArray(header) ? header[0] : header)?.split(',')[0]?.trim() || undefined;
}

export function isCloudflare(address: string): boolean {
  const mapped = /^::ffff:(\d+\.\d+\.\d+\.\d+)$/i.exec(address)?.[1];
  if (mapped) return cloudflareRanges.check(mapped, 'ipv4');
  if (isIPv4(address)) return cloudflareRanges.check(address, 'ipv4');
  if (isIPv6(address)) return cloudflareRanges.check(address, 'ipv6');
  return false;
}

/**
 * The address rate limits are counted against. With `clientIpHeader` set, the header is
 * believed only when the request reached the proxy from Cloudflare; anyone talking to the
 * origin directly is counted by the address they connected from, whatever they claim.
 */
export function clientIpReader(config: Pick<Config, 'clientIpHeader'>) {
  return (request: Pick<FastifyRequest, 'headers' | 'ip'>): string => {
    if (!config.clientIpHeader) return request.ip;
    const edge = firstValue(request.headers[proxyPeerHeader]) ?? request.ip;
    if (!isCloudflare(edge)) return edge;
    return firstValue(request.headers[config.clientIpHeader]) ?? edge;
  };
}
