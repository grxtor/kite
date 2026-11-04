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
  replaced by `meta.kiteclient.com/kite/crash-rules.json` when that copy is valid and its
  `catalogVersion` is not lower. Asked once per run; the cached copy is used offline.
- Tests: `CrashEngine_test` runs every case of `launcher/crashdoctor/testdata/expected.json`
  (real crashes from a player's own instances, scrubbed, and cases written from the loaders' own
  message templates). Add a case with every new rule.

## When it runs

`MinecraftInstance::createLaunchTask()` hands every launch to `CrashDoctorService`. When the launch
ends it analyses the run if the instance crashed: the launch failed and was not a Kill, or a crash
report newer than the launch exists (Forge's loading error screen exits with code 0). The analysis
runs on a worker thread and is kept until the instance launches again. Nothing is sent anywhere.

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
- When nothing fits it says Kite couldn't find the cause and offers to share the log, after asking.

Review snapshots: dev routes `crashwindow=<id>` (as if the crash just happened) and
`crashdoctor=<id>` (as if read again later), plus `review=crashdetails` on the Log tab.

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
- The only network use is fetching the rules over HTTPS from Kite's own server, and the upload the
  player asks for. A signed catalog can come later; for now HTTPS and the closed action set are the
  line.
