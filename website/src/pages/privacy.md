---
layout: ../layouts/LegalLayout.astro
title: "Privacy Policy"
description: "What Kite and kiteclient.com do with your data."
updated: "2026-10-20"
---

Kite is a Minecraft launcher made by grxtor. This policy covers the Kite app and this website, kiteclient.com. If anything here is unclear, write to [contact@kiteclient.com](mailto:contact@kiteclient.com).

## The short version

- Kite has no analytics, no telemetry and no advertising. It does not create an ID for you or your computer.
- We do not run an account system. Your Microsoft sign-in, your settings and your instances stay on your computer.
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

The tokens are not encrypted, but the file can only be read by your own user account. Removing an account in Kite deletes its tokens. Uninstalling Kite and deleting the data folder removes everything.

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
| paste.ee | When you press Upload on a log. If Kite itself crashes, its crash reporter uploads Kite's own log right away and shows you the link. | The log text, which becomes a public link. Kite removes sign-in tokens, UUIDs and your home folder path first. Your player name, instance names and mod list stay in the log. |

When you play, Minecraft itself connects to Mojang and Microsoft and to the servers you join. That happens inside the game, not in Kite.

### Plugins

Kite can load plugins. Plugins from other people are their own software and can send data anywhere. Only install plugins you trust.

## Children

Kite and this site don't ask for or collect personal data from anyone, including children. Sign-in for younger players is handled by Microsoft's own family settings.

## Your rights

We don't hold an account or profile about you, so in most cases there is nothing for us to look up, correct or delete. Your data is in Kite's folder on your computer, and you can delete it at any time. If you've emailed us, you can ask us to delete those emails. If you are in the EU, UK or Türkiye, you can also contact your data protection authority.

## Changes

If we change this policy, we'll update this page and the date at the top. If a change affects what the app sends, it will also be listed in Kite's release notes.

## Contact

grxtor, [contact@kiteclient.com](mailto:contact@kiteclient.com)
