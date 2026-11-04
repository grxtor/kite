## Monocypher

Monocypher 4.0.3 (https://monocypher.org, https://github.com/LoupVaillant/Monocypher), from the
release tarball `monocypher-4.0.3.tar.gz` (SHA-256
`8cc9bc341a66249016db9bd70e9142d8d0aef9945973744b1ac05dbc55d8ee66`): `src/monocypher.c`,
`src/monocypher.h` and `src/optional/monocypher-ed25519.{c,h}`, unmodified, with `LICENCE.md`.

kite-updater uses `crypto_ed25519_check()` to verify the signature on a release's
`kite-update.json` before it installs anything (see `launcher/updater/UpdateManifest.cpp`).

Dual-licensed: 2-clause BSD or CC0 1.0.
