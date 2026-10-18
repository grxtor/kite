# discord_ipc — minimal Discord IPC client (vendored)

A small self-contained Qt-only client for Discord's local IPC socket.
Implements just enough of the Discord RPC protocol to call
`SET_ACTIVITY`, which is all the DiscordRPC Kite plugin needs.

## Why not the upstream discord-rpc library?

The upstream [discord/discord-rpc](https://github.com/discord/discord-rpc) C library
(MIT) carries its own copy of `rapidjson`, has platform-specific
connection code for Linux/macOS/Windows, and is no longer maintained
by Discord. Rather than vendor ~30 files of legacy code, this folder
ships a Qt-only re-implementation that:

* uses `QLocalSocket` (which already abstracts UNIX sockets and Windows
  named pipes on a per-platform basis);
* uses Qt's `QJsonDocument` instead of rapidjson;
* supports only the handful of opcodes / commands the plugin needs.

The wire format and design are derived from the public Discord RPC
documentation (still hosted at
<https://discord.com/developers/docs/topics/rpc>) and from the original
MIT-licensed reference implementation.

## License

`discord_ipc` is licensed under the **MIT License** to match the
upstream protocol implementation it is derived from. See `LICENSE`
in this directory.
