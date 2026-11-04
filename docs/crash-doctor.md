<!--
SPDX-FileCopyrightText: 2026 grxtor
SPDX-License-Identifier: Apache-2.0
-->

# Crash doctor

When a game crashes, Kite reads what the run left behind and says why, in plain words, with the fix
as one button. This page is for whoever edits the rules: what a rule can say, what it can make Kite
do, and why it can do nothing else.

- Code: `launcher/crashdoctor/`. The engine is pure (`CrashEngine`), the fixes are C++
  (`CrashFixer`), the QML is `CrashDoctorPanel.qml` above the Log tab.
- Rules: `launcher/resources/kite/crash-rules.json`, embedded as `:/kite/crash-rules.json` and
  replaced by `meta.kiteclient.com/kite/crash-rules.json` (through `MetaCatalog`, like the bundle
  and server lists) when that copy is valid and its `catalogVersion` is not lower. Asked once per
  run; the cached copy is used offline.
- Tests: `CrashEngine_test` runs every case of `launcher/crashdoctor/testdata/expected.json`
  (real crashes from a player's own instances, scrubbed, and cases written from the loaders' own
  message templates). Add a case with every new rule.

## When it runs

`MinecraftInstance::createLaunchTask()` hands every launch to `CrashDoctorService`. When the launch
ends it analyses the run if the instance crashed: the launch failed and was not a Kill, or a crash
report newer than the launch exists (Forge's loading error screen exits with code 0). The analysis
runs on a worker thread and is kept until the instance launches again. Nothing is sent anywhere
unless the player sends the crash to Kite ([Crash reports](#crash-reports)).

What it reads: the live log (it has the JVM's start errors and Kite's own lines that
`logs/latest.log` never has), the newest `crash-reports/crash-*.txt` and `hs_err_pid*.log` of the
run, and the metadata of the enabled jars in `mods/`.

## On screen

- Right after a crash (a launch that just ended), the window "Why <instance> crashed" opens once by
  itself (`CrashDoctorDialog.qml`): each cause in its own box, the first fix as the button, the rest
  in ⋯. It never opens for an old crash; after a restart there is no report to show.
- Afterwards the instance's Log tab has one line above the log, "The game crashed: <cause>", with the
  main fix, Details and ⋯ (`CrashDoctorPanel.qml`). It stays until the next launch or ⋯ > Hide this.
- Home says "Crashed: <cause>" where it said "Crashed last time" (`InstanceList`'s `crashCause`).
- When nothing fits it says Kite couldn't find the cause and offers to send the crash to Kite (and
  to share the log, after asking).
- When rules that came later know a crash Kite could not explain, the strip says "Fix found:
  <cause>" with its fix, and a desktop notification says so once.

Review snapshots: dev routes `crashwindow=<id>` (as if the crash just happened) and
`crashdoctor=<id>` (as if read again later), plus `review=crashdetails` on the Log tab. For the
crash reports: `crashsend=<id>` sends that crash as Send would, `crashrules=<file>` uses that
`crash-rules.json` as if the meta server sent it.

## A rule

```jsonc
{
  "id": "mods.missing-dependency",       // ^[a-z0-9]+(\.[a-z0-9-]+)+$, stable
  "priority": 82,                        // 0..100
  "key": "mod",                          // captures that tell findings apart (string or list)
  "when": { "loaders": ["fabric"], "minecraft": { "min": "1.17" }, "os": ["macos"] },
  "match": { "any": [pattern], "all": [pattern], "none": [pattern], "each": true },
  "captures": { "mod": "modId", "by": "modId", "range": "text" },
  "hints": { "cls": "classHints" },      // cls -> clsMod, clsModName
  "coalesce": { "mod": ["configMod"] },
  "require": ["mod"],
  "exclude": { "mod": ["minecraft", "java"] },
  "suppresses": ["mods.crashed", { "rule": "mods.missing-dependency", "same": { "mod": "mod" } }],
  "title": { "en": "{byName|by} needs {modName|mod}", "tr": "{byName|by}, {modName|mod} modunu istiyor" },
  "groupTitle": { "en": "{count} mods are missing", "tr": "{count} mod eksik" },
  "explanation": { "en": "…", "tr": "…" },
  "fixes": [{ "action": "installMissingDependency", "params": { "mod": "{mod}" },
              "label": { "en": "Install {modName|mod}", "tr": "{modName|mod} modunu yükle" } }]
}
```

A **pattern** is `{ "re": "...", "in": ["log", "logTail", "crashReport", "hsErr"], "i": false }`:
PCRE2 as `QRegularExpression` runs it, always multi-line, never dot-all, so `.` stays on one line
and a pattern that spans lines spells out `\n`. `logTail` is the last 400 lines of the live log.
Without `in`: crash report, hs_err and the log tail. Loader errors ("Incompatible mods found", "Mod
X requires Y") can use the whole log; a generic exception should not, or a harmless one from early
in the run gets the blame.

**`when`** (all must hold): `loaders`, `minecraft` `{min, max}`, `os`, `arch`, `java`
(`aboveRecommended` / `belowRecommended`), `customJvmArgs`, `memoryRoom`, `exitCode`, `exitStatus`,
and per finding `inSet` (`performanceMods`, `installed`) and `installed`.

**Capture types**: `modId`, `modName`, `text`, `version`, `className` (`/` becomes `.`, needs a
dot), `classFileVersion` (65 becomes 21), `javaMajor` (8..40), `jarFile` (a file name, folders
stripped), `configFile` (relative, no `..`, `.toml`/`.json`/`.json5`, under `config/` only),
`mixinConfig` (adds `configMod` when an installed mod matches). A value that does not fit is
dropped, and a fix that needs it is not offered.

**Placeholders** in texts and parameters: captures, then the facts `mc`, `loader`, `javaNow`,
`javaRecommended`, `memoryMb`, `suggestedMemoryMb`, `lowerMemoryMb`, `systemMemoryMb`, `exitCode`,
and `count` in a group title. `{a|b}` is the first that has a value. Texts are plain text.

Write Turkish the way `translations/GLOSSARY.tr.md` says (log is *günlük*, install is *yükle*), and
avoid a suffix right after a placeholder ("Java {javaMajor} ile aç", not "{javaMajor}'yi").

## How findings become causes

1. Text is normalised: CRLF to LF, no ANSI escapes, no `§x` codes.
2. Per rule: `when` on the facts, `none` vetoes, every `all` must match, then `any` (the first
   matching pattern, or with `each` every match, at most 10).
3. Captures are typed, hinted, coalesced; `exclude`, `require` and the per-finding `when` apply.
4. Findings are de-duplicated by `key`.
5. Suppressions drop other rules' findings: all of them, or only those that agree with one of ours.
6. Score: priority, plus 3 when the evidence is a crash report or hs_err, plus up to 3 for more
   findings of one rule. Best first.
7. A rule with several findings is one card; its fix acts on all of them at once.
8. Nothing found: Kite says it could not tell, and offers the log upload.

## What a fix can do

Rules can name these actions and nothing else; a fix with any other action or parameter is left out
when the file loads.

| Action | Params | Does | Asks first |
|---|---|---|---|
| `useJavaForInstance` | `major` | Next launch: this instance needs at least that Java; Kite picks or downloads it | |
| `useRecommendedJava` | | Next launch: Kite picks the Java for this version again | |
| `useMatchingArchJava` | | Same, with the architecture this version's natives need | |
| `installJava` | `major` | Lets this instance download Java (next launch) | |
| `setMemory` | `maxMb` | Instance memory, clamped to what the computer has | |
| `clearCustomJvmArgs` | | Turns the instance's custom Java arguments off | yes |
| `setMacFirstThread` | | Adds `-XstartOnFirstThread` for this instance | |
| `disableMod` | `target` | Renames the mod to `.disabled` | yes |
| `removeDuplicateMod` | `mod` | Keeps the newest copy, turns the others off | yes |
| `turnOffKiteMod` | | Turns Kite's own mod off for this instance | |
| `resetConfigFile` | `file` | Moves `config/<file>` aside | yes |
| `resetOptions` | | Moves `options.txt` aside | yes |
| `turnOffShaders` | | `enableShaders=false` in Iris / Oculus settings | |
| `setGraphicsBackend` | `backend` | 26.2+ Graphics API (`opengl`, `default`) | |
| `useDedicatedGpu` | | Windows: the faster graphics card for Java | |
| `repairGameFiles` | | Next launch downloads every library again | |
| `installMissingDependency` | `mod` | The Mods tab's install review | the review |
| `updateMod` | `mods` | The install review, replacing the files | the review |
| `updateLoader` | | Opens the Version tab | |
| `rematchPerformanceMods` | | Runs Kite Performance's setup again | |
| `uploadLog` | | Shares the log through paste.ee (the link is copied) | yes |
| `open…` | | Mods / instance / worlds folder, Backups, Content, Java or memory settings, the log | |

Nothing is deleted: files are renamed aside, and the last fix of an instance can be undone, also
after a restart (`CrashDoctorUndo` in the instance settings).

## Threat model

The rules come from the network and the log comes from mods, servers and chat; neither is trusted.

- A rule picks words and actions from the table above, with parameters bound to its captures. It
  cannot run anything, pass a JVM argument or a URL, or name a path: file parameters are names
  resolved inside this instance's `mods/` or `config/`, checked again after links are resolved.
- The confirmations are Kite's, and name the exact files; a rule cannot word them.
- Texts are plain text with a length cap; no links, no formatting.
- Patterns: at most 600 characters, no PCRE verbs or inline flags, a match-step limit Kite sets, and
  inputs capped (2 MB log, 1 MB crash report, 256 KB hs_err). The analysis runs off the GUI thread.
- A broken rule is left out alone; a file with another `formatVersion` is ignored whole; the
  embedded rules always load.
- The only network use is fetching the rules over HTTPS from Kite's own server, the upload the
  player asks for, and the crash report the player sends (below). A signed catalog can come later;
  for now HTTPS and the closed action set are the line.

## Crash reports

A crash no rule knew, or one whose fix did not help (the instance's `CrashDoctorLastFix` names the
rule of the last fix tried, and the game crashed with that rule again after it), can be sent to
Kite's server, so that someone writes the rule and the next `crash-rules.json` fixes it for
everyone. The server side, its storage and the rule-writing workflow are in the friends server's
README (`server/friends`, "Crash reports").

- **Consent.** The crash window and the Log tab's details offer "Send this crash to Kite so we can
  find the fix"; "What is sent" shows the exact payload in words first. Nothing goes until Send is
  pressed. The box "From now on, send without asking" sets `CrashReportsAutoSend` when Send is
  pressed; it is off by default and Settings > Log upload turns it off. Only a fresh crash is sent
  without asking.
- **Payload** (`CrashPayload`, `formatVersion` 1): `signature`, `kite`, `os`, `arch`, `java`
  (major or `null`), `minecraft`, `loader` (`type`, `version`), `rules` (matched rule ids, empty
  unless a fix did not help), `culprit` (optional mod id), `mods` (`{id, version}`, at most 500, each
  id once, none for vanilla) and `lines`. No account, name, instance, world or path. Sent without a
  token to `FriendsServerUrl` + `/v1/crash-reports`.
- **Outbox** (`CrashOutbox`, `<data>/crash-reports/outbox.json`): a report that cannot go (no
  connection, 429, 5xx) waits and is tried again at the next start and every half hour, 8 times or a
  week at most, 20 at most; a 4xx answer drops it. A signature that was sent is not sent again for
  30 days.
- **Read again** (`CrashFollowUp`, `<data>/crash-reports/unresolved.json`): every crash nothing
  knew is kept for 30 days (instance id, its crash report, hs_err and `logs/latest.log`, the time,
  the signature, the rules' `catalogVersion`), 3 per instance and 20 in all. When a catalog with a
  higher `catalogVersion` arrives (download, cache, a launcher update, or `crashrules=`), each kept
  crash is analysed again from its files. If they no longer give the same signature (latest.log was
  written again) or are gone, it is dropped; if a rule knows it now, the instance's report becomes
  that one with "Fix found", and `MMCO_HOOK_CRASH_FIX_FOUND` tells the DesktopNotifier plugin once
  ("Last Tuesday's Skies crash has a fix now."). A launch of the instance clears the fixes found, a
  launch that does not crash clears all its kept crashes. At start, while crashes are kept, Kite asks
  the meta server for newer rules. The server is never asked.

### Lines and signature

The server computes the signature again from the lines, so this is the contract. The same crash on
two computers must give the same lines; nothing personal may survive them (`CrashSignature`,
`CrashScrubber`, tested by `CrashSignature_test` on the real corpus).

1. Text is normalised as above (CRLF, ANSI, `§x`).
2. The lines come from the first source that has them:
   - the crash report: the first stack trace after `Description:`. When the report has Forge's or
     NeoForge's mod sections (`-- MOD <id> --`, `-- Mod loading issue for: <id> --`), the lines are
     the trace's first line and the `Failure message:` and `Exception message:` lines of the first
     3 mods (no frames: the trace is the loader's own);
   - the hs_err file: the signal (`SIGSEGV`, `EXCEPTION_ACCESS_VIOLATION`) or the first error line,
     the problematic frame, and up to 8 lines of `Native frames:`;
   - the last 400 lines of the log: the last stack trace (lines a game printed through
     `[STDERR]:` count too);
   - else the last 5 error lines of the log (`/ERROR]`, `/FATAL]`, `Error`, `Exception in thread`),
     without their `[time] [thread/LEVEL]` prefix. No lines: nothing to send.
3. From a stack trace: the first exception's line and up to 2 message lines before its frames; when
   there are `Caused by:` causes, the last one's line and message lines too (`Suppressed:` blocks
   are skipped); then the top 8 frames of the last cause that has frames.
4. Each frame becomes `at <class>.<method>(<file>)`: no `at` indent, no class loader or module
   prefix (`TRANSFORMER/create@6.0/`, `java.base/`, `knot//`), no line number, nothing after the
   parenthesis (jar names, `{re:mixin,...}`). Generated names lose their numbers:
   `$$Lambda$12/0x...` -> `$$Lambda`, a method's trailing `$12` -> `$<n>` (`lambda$tick$<n>`),
   Mixin's `$zza000$` -> `$`, `GeneratedMethodAccessor12`, `$Proxy12`, `ASMEventHandler_12_` ->
   `<n>`, `$ByteBuddy$...` -> `$ByteBuddy`.
5. Every line, in this order: the home folder -> `~`; URLs -> `<url>`, e-mail addresses ->
   `<email>`; absolute paths (also with spaces in a folder name) -> `<path>`, or `<path>/<file>`
   when the file is a `.jar`, `.json`, `.json5`, `.toml`, `.cfg`, `.properties`, `.txt`, `.class`,
   `.log` or native library; the system user's and the Minecraft accounts' names (3 characters or
   more, whole words, any case) -> `<user>`; UUIDs -> `<uuid>`; `eyJ...` tokens -> `<token>`; IPv4
   (with port) -> `<ip>`; dates and times -> `<time>`; IPv6 -> `<ip>`; `0x...` -> `<hex>`,
   `@1a2b...` -> `@<hex>`, hex runs of 12 or more with a digit and a letter -> `<hex>`.
6. Lines that are not frames also: the instance's name and its worlds' folder names, whole
   phrases as written (so an instance called "Create" leaves the package `create` alone) ->
   `<name>`; `host:port` and host names ending in a common top-level domain -> `<host>`; every
   other stand-alone number (not part of a word, a version or a placeholder) -> `<n>`.
7. Control and bidi characters are removed, runs of white space become one space, the line is
   trimmed and cut to 300 UTF-16 code units (never half a surrogate pair).
8. **Signature** = SHA-256 of the lines joined with `\n` (no trailing newline), UTF-8, lowercase
   hex. The server refuses a report whose signature does not match, or whose lines still hold a
   UUID, an IP address, a home path or an e-mail address.
9. **Culprit**: for a mod loading failure the first mod section's id; else the crash report's
   `Suspected Mods:` mod, else the first frame whose Forge module or whose class (the catalog's class
   hints) is an installed mod. It
   is sent only when it is an installed mod id, and is not part of the signature.
