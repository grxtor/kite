import assert from 'node:assert/strict';
import { describe, test } from 'node:test';
import { clientIpReader, isCloudflare } from '../src/clientIp.ts';

const cloudflareEdge = '172.70.1.2';
const attacker = '203.0.113.9';

describe('isCloudflare', () => {
  const cases: Array<[string, boolean]> = [
    ['173.245.48.1', true],
    ['104.16.0.1', true],
    ['172.71.255.255', true],
    ['::ffff:162.158.1.1', true],
    ['2606:4700::1111', true],
    ['2a06:98c1::1', true],
    ['172.15.0.1', false],
    ['203.0.113.9', false],
    ['127.0.0.1', false],
    ['2001:db8::1', false],
    ['not-an-ip', false],
    ['', false],
  ];
  for (const [address, expected] of cases) {
    test(`${address || '(empty)'} is ${expected ? '' : 'not '}Cloudflare`, () => {
      assert.equal(isCloudflare(address), expected);
    });
  }
});

describe('clientIpReader', () => {
  const read = clientIpReader({ clientIpHeader: 'cf-connecting-ip' });

  test('without a client IP header the socket address is used', () => {
    const plain = clientIpReader({ clientIpHeader: null });
    assert.equal(plain({ ip: '10.0.1.5', headers: { 'cf-connecting-ip': '1.1.1.1' } }), '10.0.1.5');
  });

  test('a request through Cloudflare is counted by the header', () => {
    const headers = { 'x-real-ip': cloudflareEdge, 'cf-connecting-ip': '198.51.100.7' };
    assert.equal(read({ ip: '10.0.1.5', headers }), '198.51.100.7');
  });

  test('a request straight to the origin is counted by its own address', () => {
    const headers = { 'x-real-ip': attacker, 'cf-connecting-ip': '198.51.100.7' };
    assert.equal(read({ ip: '10.0.1.5', headers }), attacker);
  });

  test('without X-Real-Ip the socket address decides', () => {
    assert.equal(read({ ip: cloudflareEdge, headers: { 'cf-connecting-ip': '198.51.100.7' } }), '198.51.100.7');
    assert.equal(read({ ip: attacker, headers: { 'cf-connecting-ip': '198.51.100.7' } }), attacker);
  });

  test('Cloudflare without the header falls back to the edge address', () => {
    assert.equal(read({ ip: '10.0.1.5', headers: { 'x-real-ip': cloudflareEdge } }), cloudflareEdge);
  });

  test('only the first address of a list counts', () => {
    const headers = { 'x-real-ip': cloudflareEdge, 'cf-connecting-ip': '198.51.100.7, 192.0.2.1' };
    assert.equal(read({ ip: '10.0.1.5', headers }), '198.51.100.7');
  });
});
