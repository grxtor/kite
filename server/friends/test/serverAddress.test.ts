import assert from 'node:assert/strict';
import { describe, test } from 'node:test';
import { classifyHost, normalizeHost } from '../src/serverAddress.ts';
import type { AddressKind } from '../src/serverAddress.ts';

const label63 = 'a'.repeat(63);
const name253 = [label63, label63, label63, 'a'.repeat(61)].join('.');
const name254 = [label63, label63, label63, 'a'.repeat(62)].join('.');

describe('normalizeHost', () => {
  const valid: Array<[string, string, string]> = [
    ['plain domain', 'mc.hypixel.net', 'mc.hypixel.net'],
    ['uppercase', 'MC.Hypixel.NET', 'mc.hypixel.net'],
    ['surrounding spaces', '  mc.hypixel.net \n', 'mc.hypixel.net'],
    ['trailing dot', 'mc.hypixel.net.', 'mc.hypixel.net'],
    ['single label', 'survival', 'survival'],
    ['hyphen inside a label', 'my-server.example.com', 'my-server.example.com'],
    ['punycode label', 'xn--bcher-kva.example', 'xn--bcher-kva.example'],
    ['digits in labels', '2b2t.org', '2b2t.org'],
    ['IPv4', '203.0.113.7', '203.0.113.7'],
    ['bracketed IPv6', '[::1]', '::1'],
    ['bracketed IPv6 uppercase', '[2001:DB8::A]', '2001:db8::a'],
    ['bare IPv6', 'fe80::1', 'fe80::1'],
    ['IPv4-mapped IPv6', '::ffff:192.168.1.1', '::ffff:192.168.1.1'],
    ['63 character label', `${label63}.com`, `${label63}.com`],
    ['253 character name', name253, name253],
  ];
  for (const [title, input, expected] of valid) {
    test(`accepts ${title}`, () => {
      assert.equal(normalizeHost(input), expected);
    });
  }

  const invalid: Array<[string, string]> = [
    ['empty string', ''],
    ['only spaces', '   '],
    ['underscore', 'bad_host'],
    ['leading hyphen', '-x.com'],
    ['trailing hyphen', 'x-.com'],
    ['empty label', 'a..b'],
    ['leading dot', '.example.com'],
    ['only a dot', '.'],
    ['two trailing dots', 'example.com..'],
    ['inner space', 'my server.com'],
    ['port included', 'example.com:25565'],
    ['scheme included', 'https://example.com'],
    ['path included', 'example.com/path'],
    ['non-ASCII', 'bücher.example'],
    ['64 character label', `${label63}a.com`],
    ['254 character name', name254],
    ['empty brackets', '[]'],
  ];
  for (const [title, input] of invalid) {
    test(`rejects ${title}`, () => {
      assert.equal(normalizeHost(input), null);
    });
  }
});

describe('classifyHost', () => {
  const cases: Array<[string, AddressKind]> = [
    ['mc.hypixel.net', 'domain'],
    ['play.example.org', 'domain'],
    ['local.example.com', 'domain'],
    ['example.lan.com', 'domain'],
    ['corp.com', 'domain'],
    ['internal.com', 'domain'],
    ['notlocalhost.com', 'domain'],
    ['256.1.1.1.nip.io', 'domain'],
    ['mc-1-2-3.example.com', 'domain'],

    ['0.0.0.0', 'local'],
    ['0.1.2.3', 'local'],
    ['10.0.0.1', 'local'],
    ['10.255.255.255', 'local'],
    ['100.64.0.1', 'local'],
    ['100.127.255.255', 'local'],
    ['127.0.0.1', 'local'],
    ['127.255.0.1', 'local'],
    ['169.254.10.10', 'local'],
    ['172.16.0.1', 'local'],
    ['172.31.255.255', 'local'],
    ['192.168.1.20', 'local'],
    ['192.168.255.255', 'local'],

    ['1.1.1.1', 'ip'],
    ['8.8.8.8', 'ip'],
    ['203.0.113.7', 'ip'],
    ['9.255.255.255', 'ip'],
    ['11.0.0.1', 'ip'],
    ['100.63.255.255', 'ip'],
    ['100.128.0.1', 'ip'],
    ['126.255.255.255', 'ip'],
    ['169.253.1.1', 'ip'],
    ['169.255.1.1', 'ip'],
    ['172.15.255.255', 'ip'],
    ['172.32.0.1', 'ip'],
    ['192.167.1.1', 'ip'],
    ['192.169.1.1', 'ip'],

    ['::', 'local'],
    ['::1', 'local'],
    ['fc00::1', 'local'],
    ['fd12:3456:789a::1', 'local'],
    ['fe80::1', 'local'],
    ['febf::1', 'local'],
    ['::ffff:192.168.1.1', 'local'],
    ['::ffff:127.0.0.1', 'local'],
    ['::ffff:10.1.2.3', 'local'],
    ['2001:db8::1', 'ip'],
    ['2606:4700::1111', 'ip'],
    ['fec0::1', 'ip'],
    ['fbff::1', 'ip'],
    ['::2', 'ip'],
    ['::ffff:8.8.8.8', 'ip'],

    ['localhost', 'local'],
    ['survival', 'local'],
    ['foo.localhost', 'local'],
    ['printer.local', 'local'],
    ['nas.lan', 'local'],
    ['box.home', 'local'],
    ['db.internal', 'local'],
    ['wiki.intranet', 'local'],
    ['files.corp', 'local'],
    ['router.home.arpa', 'local'],

    ['1.2.3.4.nip.io', 'ip'],
    ['84-12-3-4.sslip.io', 'ip'],
    ['mc.84-12-3-4.sslip.io', 'ip'],
    ['a.1.2.3.4.example.com', 'ip'],
    ['255.255.255.255.nip.io', 'ip'],
  ];
  for (const [host, kind] of cases) {
    test(`${host} is ${kind}`, () => {
      assert.equal(classifyHost(host), kind);
    });
  }
});
