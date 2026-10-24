---
layout: ../layouts/LegalLayout.astro
title: "Privacy Policy"
description: "What Kite and kiteclient.com do with your data."
updated: "2026-10-24"
---

Kite is a Minecraft launcher made by grxtor. This policy covers the Kite app and this website, kiteclient.com. If anything here is unclear, write to [contact@kiteclient.com](mailto:contact@kiteclient.com).

## The short version

- Kite has no analytics, no telemetry and no advertising. It does not create an ID for you or your computer.
- Kite has no account system of its own. Your Microsoft sign-in, your settings and your instances stay on your computer.
- Friends is the one feature that uses a server of ours. It is off until you turn it on. When it is on, Kite's server keeps your Minecraft name, your friends and what you are playing, and you can delete all of it from inside Kite.
- Kite talks to the services it needs to work: Microsoft and Mojang to sign you in and download the game, and mod sites such as Modrinth when you browse or update mods. Each of those services sees your IP address and handles your data under its own privacy policy.

## The website

This site is a set of static pages. It sets no cookies, runs no scripts and loads nothing from other companies. Its fonts are served from this site.

The site runs on our own server behind Cloudflare. Cloudflare and our server see your IP address, the page you asked for, the time and your browser's user agent. We use this only to deliver the site and protect it from abuse, and we do not keep separate access logs.

## The Kite app

### What stays on your computer

Kite keeps everything in its data folder on your computer:

- macOS: `~/Library/Application Support/Kite/Kite`
- Windows: `%APPDATA%\Kite\Kite`
- Linux: `~/.local/share/Kite`

That folder holds:

- your settings
- your instances, with their mods, worlds, screenshots and backups
- Kite's own logs
- `accounts.json`, which holds your Minecraft profile (name, UUID, skin and capes) and the sign-in tokens Kite needs to keep you signed in
- while Friends is on: `friends.json`, with the token for Kite's friends server, and your friends' heads in `cache/friendfaces`

The tokens are not encrypted, but `accounts.json` and `friends.json` can only be read by your own user account. Removing an account in Kite deletes its tokens. Uninstalling Kite and deleting the data folder removes everything.

### Signing in with Microsoft

When you sign in, Kite opens Microsoft's own sign-in page in your browser. Kite never sees your password. After you approve it, Microsoft sends Kite a token through a short-lived address on your own computer (`127.0.0.1`), and Kite then uses it with:

- **Xbox Live**, to get the tokens Minecraft needs. Kite also asks Xbox Live for your Xbox profile settings during sign-in. It does not store them or send them anywhere.
- **Minecraft services** (`api.minecraftservices.com`), to check that you own Minecraft: Java Edition and to read your profile, skin and capes.

Kite refreshes these tokens when they expire, while it is running. If you use the skin and cape editor, the skin you choose is uploaded to your Minecraft profile.

Offline accounts are only available after you add a Microsoft account that owns the game. They store just a name.

### Services Kite contacts

Kite contacts these services as part of normal use. Kite does not add any personal data beyond what each request needs.

| Service | When | What it receives |
|---|---|---|
| Mojang (`minecraft.net` hosts) | When an instance needs game files, libraries or assets | Download requests |
| Metadata server | When you create an instance, change versions, or Kite needs a Java runtime | Requests for version lists. Today this is `meta.projecttick.org`, run by Project Tick, the project Kite is forked from. Kite will move to its own server at kiteclient.com. |
| Java vendors: Mojang, Eclipse Adoptium, Azul | When an instance needs a Java version you don't have and automatic Java download is on (the default) | Download requests |
| Modrinth, CurseForge, FTB, ATLauncher, Technic | When you browse, install or update modpacks and mods | Your search terms. When you check for updates, file hashes of your installed mods, so the service can match them. |
| Image hosts chosen by pack authors | When you open a modpack's description | Requests for the images in that description |
| GitHub | About once a day in release builds, to check for a new version of Kite. Also at start, when `git` is installed, to update Kite's offline copy of its wiki. | Download requests |
| Kite's friends server (`api.kiteclient.com`) | Only while Friends is on: when it signs in, then once a minute while Kite runs | See [Friends](#friends) below |
| Mojang's session server (`sessionserver.mojang.com`) and `textures.minecraft.net` | Only while Friends is on: when it signs in, and to show the heads of your friends | At sign-in, a join request with your Minecraft token, the same one the game sends when you join a server. For heads, the UUIDs of the players in your friends list, your requests and your blocks. |
| paste.ee | When you press Upload on a log. If Kite itself crashes, its crash reporter uploads Kite's own log right away and shows you the link. | The log text, which becomes a public link. Kite removes sign-in tokens, UUIDs and your home folder path first. Your player name, instance names and mod list stay in the log. |

When you play, Minecraft itself connects to Mojang and Microsoft and to the servers you join. That happens inside the game, not in Kite.

### Plugins

Kite can load plugins. Plugins from other people are their own software and can send data anywhere. Only install plugins you trust.

## Friends

Friends shows which server your friends are playing on. There is no chat. It is off until you turn on **Use friends** in Settings → Friends, and while it is off, Kite sends nothing for it.

### Signing in

Friends signs in to Kite's own server, `api.kiteclient.com`, the way a Minecraft server checks a player who joins: Kite tells Mojang's session server that your account is joining, and Kite's server asks Mojang to confirm it. Your Microsoft and Minecraft tokens only go to Mojang, never to Kite's server.

Kite's server then gives Kite its own token, which opens the friends list and nothing else, and lasts 30 days. Friends needs a Microsoft account; offline accounts can't use it.

### What Kite sends

Once a minute while Kite runs, it tells the server:

- that Kite is open, or that the game is running
- while the game runs: the instance name, the Minecraft version, and where you are, which Kite reads from the game's log: the address and port of the server you joined, or single-player

When you quit Kite, it tells the server that you went offline.

### What the server keeps

- your Minecraft UUID and name, and when you were last seen
- your Friends settings
- your friends, your pending requests and the players you blocked
- your current status. It expires three minutes after Kite last reported it, and is then deleted.
- your sign-in tokens, stored only as SHA-256 hashes, so they can't be read back

Like this site, the friends server runs on our own machine behind Cloudflare. It doesn't log requests. Cloudflare and the server see your IP address. The server keeps it only in memory, to limit how many requests one address can make.

### Who sees what

- **Who can see you** is set to Friends by default: only your friends see your status, and when you were last online.
- Everyone lets any signed-in Kite user look up your status by your name, too.
- Invisible always shows you as offline, without a last-seen time, and the server keeps no status for you at all.
- With **Hide which server I am on**, friends see that you are playing, but not where. The server doesn't keep the address either.
- A server you join by a raw IP address, or one on your home network, shows as "a server". Its address is neither shown nor kept unless you turn on **Show servers I join by IP address**.
- Anyone using Kite who knows your Minecraft name can send you a friend request. Blocking someone removes them from your friends, and they can no longer send you requests or see your status.

### Your friends' heads

Kite downloads the heads of your friends straight from Mojang (`sessionserver.mojang.com` and `textures.minecraft.net`), not through Kite's server. It keeps them in its data folder and fetches them again after a day.

### Turning it off and deleting your data

Turning Friends off tells the server that you are offline, signs Kite out, and deletes the token and the heads from your computer. Your friends stay on the server, so you can turn it back on later. Your account stays there until you delete it.

**Delete my friends data** in Settings → Friends deletes your account on Kite's server and everything stored with it: tokens, status, settings, friendships, requests in both directions and the blocks you placed. Blocks that other players placed on you stay, because they belong to them. It also turns Friends off.

## Children

Kite and this site don't ask for personal data from anyone, including children. If a player turns on Friends, Kite's server keeps only what is listed under [Friends](#friends). Sign-in for younger players is handled by Microsoft's own family settings.

## Your rights

Unless you use Friends, we don't hold an account or profile about you, so in most cases there is nothing for us to look up, correct or delete. Your data is in Kite's folder on your computer, and you can delete it at any time. If you use Friends, **Delete my friends data** deletes what Kite's server holds about you. If you've emailed us, you can ask us to delete those emails. If you are in the EU, UK or Türkiye, you can also contact your data protection authority.

## Changes

If we change this policy, we'll update this page and the date at the top. If a change affects what the app sends, it will also be listed in Kite's release notes.

## Contact

grxtor, [contact@kiteclient.com](mailto:contact@kiteclient.com)
