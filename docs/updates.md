# How Kite updates itself

Kite checks for a new release when it starts and then once a day (Settings → Updates turns
that off, and has "Check now"). It never installs anything without asking, and never replaces
itself while a game started from it is running: then the only offer is "Update when I close
Kite".

Every update is signed. A copy of Kite installs a release only when its files carry an Ed25519
signature by the key built into that copy. The private keys live in two GitHub Actions secrets;
nothing on a developer's machine is needed to make a release.

## Per platform

| Installed as | Who installs the update | How |
|---|---|---|
| macOS app (`.dmg` / `.zip`) | Sparkle 2.10, inside Kite | Downloads the build's `.zip`, checks its EdDSA signature, swaps the app and restarts. An update downloaded but not installed yet goes in when Kite quits. |
| Windows installer (`Setup.exe`) | `kite-updater.exe` | Downloads the new `Setup.exe`, checks it, then runs it silently (`/S /Update /Relaunch`); the installer keeps the shortcuts and file association the installation had and starts Kite again. |
| Windows portable / plain `.zip`, Linux portable `.tar.gz` | `kite-updater` | Downloads the archive, checks it, unpacks it beside the data folder, backs up what it replaces and copies it in from a second copy of itself (a running program cannot overwrite itself on Windows), then starts Kite. |
| Linux AppImage | `kite-updater` (from inside the AppImage) | Downloads the new AppImage, checks it and renames it over the old file (atomic, same folder). Kite stays open meanwhile, because the updater lives in the AppImage's mount, which goes when Kite exits; then Kite starts the new file. The `.zsync` files are still published for AppImageUpdate and AppImageLauncher users. |
| Scoop, AUR (`kite-bin`), Flatpak | the package manager | Kite only says a release is out and what to run (`scoop update kite`, …). The packages write `updates-managed-by.txt` into the installation; `kite-updater` refuses to install over them. |
| Homebrew cask | Sparkle | The cask says `auto_updates true`, so `brew upgrade` leaves Kite to update itself. |

On Windows and Linux the check and the install are done by `kite-updater`, a separate program
next to `kite`: it asks the GitHub API for the releases, compares the newest tag with the
installed version (`kite --version`), and picks the release file that matches the build
(`Kite_BUILD_ARTIFACT`, portable or not, AppImage or not, CPU). On Windows it touches nothing
while Kite or a game started from it still holds its files; it asks the user to close them.
A failed automatic check stays in the log and is tried again an hour later; only "Check now"
shows errors, and "Last checked" counts only checks that got an answer. The prompt itself is Kite's own
window; its release notes are the GitHub release's text, shown through the same sanitizer as
mod descriptions (no raw HTML, links ask before they open).

## The feeds

All of them are assets of the GitHub release, so `releases/latest/download/<name>` always
serves the newest published (non-draft, non-pre-release) release's copy. No server of ours is
involved, and a draft release is invisible until it is published.

- `appcast-macOS-ARM-Qt6.xml`, `appcast-macOS-Intel-Qt6.xml`, `appcast-macOS-Intel-Legacy-Qt6.xml`:
  Sparkle's feed, one per macOS build, so each Mac stays on its line (the Legacy build is for
  Intel Macs on macOS 12). Each offers that build's `.zip` with an EdDSA signature. The macOS app
  reads its feed from `SUFeedURL` in Info.plist, which CMake sets to
  `https://github.com/grxtor/kite/releases/latest/download/appcast-<Kite_BUILD_ARTIFACT>.xml`.
- `kite-update.json`: every file of the release with its size and SHA-256, plus the tag.
  `kite-update.json.sig` is an Ed25519 signature over the manifest's exact bytes. `kite-updater`
  installs a file only if the manifest verifies, names the release being installed, and lists
  the file with the hash the download turns out to have.

`.github/workflows/release.yml` makes and signs these in the `create_release` job, on every tag,
with `packaging/update-feed/kite_update_feed.py`, verifies them the way the apps will, and
attaches them to the draft release next to the binaries. When the release is published,
`publish.yml` copies its notes into the appcasts (as Markdown, which Sparkle shows as text).

The script fails the release when:

- the tag is not `v` + the version in `CMakeLists.txt` (`Kite_VERSION_*`). That version is what
  both updaters compare against: a build that reports 1.2.0 from a v1.3.0 tag would update to
  itself forever. Bump the version before tagging.
- a secret does not belong to the public key built into the apps, or is missing.
- a macOS build's `.zip` is missing.

## Keys and secrets

| Secret (grxtor/kite) | Public key in `CMakeLists.txt` | Signs |
|---|---|---|
| `SPARKLE_ED_PRIVATE_KEY` | `MACOSX_SPARKLE_UPDATE_PUBLIC_KEY` (→ `SUPublicEDKey`) | each macOS `.zip` (Sparkle's EdDSA) |
| `KITE_UPDATE_ED_PRIVATE_KEY` | `Kite_UPDATE_PUBLIC_KEY` (→ `BuildConfig.UPDATE_PUBLIC_KEY`) | `kite-update.json` |

Both are base64 of a 32-byte Ed25519 seed, Sparkle's own key format (`generate_keys -x`,
`sign_update --ed-key-file`). Backups are kept offline by the maintainer. Losing a key, or
changing the public key, strands every installed copy: Sparkle only takes a new key from an
update signed with the old one, and `kite-updater` only knows its built-in key. Those users
would have to download Kite by hand once.

## Without an Apple Developer ID

The macOS app is ad-hoc signed and not notarized. What that means for updates:

- Sparkle accepts an update on its EdDSA signature alone. It still requires the new app's
  (ad-hoc) code signature to be valid, which CI checks with `codesign --verify --deep --strict`.
  An ad-hoc signature has no identity, so it cannot be matched against the old one; Sparkle
  does not try.
- Sparkle removes the quarantine flag from what it installs, so an update opens without a
  Gatekeeper warning. The **first** install, downloaded with a browser, still gets one:
  System Settings → Privacy & Security → "Open Anyway", or
  `xattr -dr com.apple.quarantine /Applications/Kite.app`.
- Kite has to run from a folder it can write to, normally Applications. A copy opened straight
  from the disk image or from Downloads (App Translocation) is read-only: Sparkle says so and
  asks to move Kite to Applications first.
- Not tried yet: whether macOS's App Management protection (Ventura and later) asks once
  before an ad-hoc signed Kite in Applications replaces itself. An ad-hoc signature has no Team
  ID that would mark the update as the app's own; the end-to-end run below used a folder
  outside Applications.

A Developer ID would make the first download open without a warning (notarization), let
Sparkle check that an update comes from the same developer as well as the EdDSA key, and
make the App Management question go away. Windows has the same shape: without a code-signing
certificate SmartScreen warns about the first `Setup.exe`; updates run without it.

## Testing locally

```sh
python3 -m unittest discover -s packaging/update-feed     # also a ctest: UpdateFeedScript
ctest --test-dir build -C Debug -R 'UpdateManifest|UpdateSettingsController'
```

The script's tests run it against a fake release directory: RFC 8032 test vectors, the
manifest and appcasts it writes, their signatures, and every way `make` refuses. The
`UpdateManifest` test checks a manifest the script signed with the updater's own code
(Monocypher), so the two implementations are tested against each other.

An end-to-end run on macOS, without Sparkle's windows:

1. Configure with `-DKite_BUILD_ARTIFACT=macOS-ARM-Qt6
   -DMACOSX_SPARKLE_UPDATE_FEED_URL=http://127.0.0.1:8765/appcast-macOS-ARM-Qt6.xml`, build
   and `cmake --install` twice with two versions in `Kite_VERSION_*` (say 1.2.99 and 1.3.0).
   Give both bundles a test `CFBundleIdentifier`, so Sparkle's settings do not land in the real
   Kite's defaults, and sign them ad-hoc as `.github/actions/package/macos` does.
2. Zip the newer one with `ditto -c -k --sequesterRsrc --keepParent Kite.app
   Kite-macOS-ARM-Qt6-v1.3.0.zip` and run `kite_update_feed.py make` and `verify` on it (with
   stand-ins for the two Intel zips and a copy of `CMakeLists.txt` saying 1.3.0).
3. Serve the feed directory: `python3 -m http.server 8765 --bind 127.0.0.1`.
4. Build `sparkle-cli` from the Sparkle sources of the pinned version (it left the binary
   distribution in 2.9; no Xcode needed):
   `clang -fobjc-arc -fmodules -DSPU_OBJC_DIRECT_MEMBERS= -DSPU_OBJC_DIRECT= -F <Sparkle dir>
   -framework Sparkle -framework AppKit -Wl,-rpath,<Sparkle dir> -sectcreate __TEXT
   __info_plist Info.plist sparkle-cli/*.m -o sparkle-cli`, with the placeholders in
   `sparkle-cli/Info.plist` filled in.
5. `sparkle-cli <older>/Kite.app --check-immediately --verbose` installs 1.3.0 in place; the
   same run against a zip with one bit flipped must end in "improperly signed" with the bundle
   untouched.

Snapshots of the screens, without an updater or a network:
`KITE_QML_ROUTE="page=settings;section=updates;updates=pending"` (`idle`, `pendingGame`,
`managed`) and `KITE_QML_ROUTE="ask=update"` (`updateGame`, `updateManaged`).
