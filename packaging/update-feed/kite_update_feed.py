#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
"""Makes and signs Kite's update feeds for one release.

  make       For every macOS build, appcast-<artifact>.xml: Sparkle's feed, with an EdDSA
             signature over the build's .zip. For everything else, kite-update.json, which lists
             every release file with its size and SHA-256, and kite-update.json.sig, an Ed25519
             signature over the manifest's exact bytes. kite-updater checks both before it
             installs anything on Windows or Linux.
  add-notes  Puts a published release's notes into appcasts made by `make` (the notes are written
             after the build, so `make` cannot know them).
  verify     Checks a manifest, its signature and the files it lists, and appcasts against the
             archives they name: what the apps do, run by the release workflow before uploading.

The release workflow runs `make` and `verify` on a tag; publish.yml runs `add-notes`. Locally:

  kite_update_feed.py make --tag v1.3.0 --cmake-lists CMakeLists.txt \\
      --sparkle-key-file sparkle.key --update-key-file update.key \\
      --base-url https://github.com/grxtor/kite/releases/download/v1.3.0 --out out/ FILES...

Keys are base64 of a 32-byte Ed25519 seed, the format of Sparkle's `generate_keys -x`. The version
and both public keys are read from CMakeLists.txt, so a tag that does not match the version, or a
secret that does not match the key built into the apps, fails the release instead of shipping a
feed nobody can use.

Standard library only: Ed25519 (RFC 8032) is written out below, since neither hashlib nor the
runners' Python carry it. It signs a handful of files per release, so speed does not matter.
"""

from __future__ import annotations

import argparse
import base64
import datetime as _dt
import email.utils
import hashlib
import json
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from typing import Callable, Iterable, Iterator

# ------------------------------------------------------------------------------------------------
# Ed25519, RFC 8032 section 6 (the reference implementation, with streamed hashing)
# ------------------------------------------------------------------------------------------------

_P = 2**255 - 19
_L = 2**252 + 27742317777372353535851937790883648493
_D = -121665 * pow(121666, _P - 2, _P) % _P
_SQRT_M1 = pow(2, (_P - 1) // 4, _P)

Chunks = Callable[[], Iterable[bytes]]


def _inv(x: int) -> int:
    return pow(x, _P - 2, _P)


def _point_add(p: tuple, q: tuple) -> tuple:
    a = (p[1] - p[0]) * (q[1] - q[0]) % _P
    b = (p[1] + p[0]) * (q[1] + q[0]) % _P
    c = 2 * p[3] * q[3] * _D % _P
    d = 2 * p[2] * q[2] % _P
    e, f, g, h = b - a, d - c, d + c, b + a
    return (e * f % _P, g * h % _P, f * g % _P, e * h % _P)


def _point_mul(s: int, p: tuple) -> tuple:
    q = (0, 1, 1, 0)
    while s > 0:
        if s & 1:
            q = _point_add(q, p)
        p = _point_add(p, p)
        s >>= 1
    return q


def _point_equal(p: tuple, q: tuple) -> bool:
    return (p[0] * q[2] - q[0] * p[2]) % _P == 0 and (p[1] * q[2] - q[1] * p[2]) % _P == 0


def _recover_x(y: int, sign: int) -> int | None:
    if y >= _P:
        return None
    x2 = (y * y - 1) * _inv(_D * y * y + 1)
    if x2 == 0:
        return None if sign else 0
    x = pow(x2, (_P + 3) // 8, _P)
    if (x * x - x2) % _P != 0:
        x = x * _SQRT_M1 % _P
    if (x * x - x2) % _P != 0:
        return None
    if (x & 1) != sign:
        x = _P - x
    return x


_GY = 4 * _inv(5) % _P
_GX = _recover_x(_GY, 0)
_G = (_GX, _GY, 1, _GX * _GY % _P)


def _compress(p: tuple) -> bytes:
    zinv = _inv(p[2])
    x = p[0] * zinv % _P
    y = p[1] * zinv % _P
    return int.to_bytes(y | ((x & 1) << 255), 32, "little")


def _decompress(s: bytes) -> tuple | None:
    if len(s) != 32:
        return None
    y = int.from_bytes(s, "little")
    sign = y >> 255
    y &= (1 << 255) - 1
    x = _recover_x(y, sign)
    if x is None:
        return None
    return (x, y, 1, x * y % _P)


def _expand_seed(seed: bytes) -> tuple[int, bytes]:
    if len(seed) != 32:
        raise ValueError("an Ed25519 seed is 32 bytes")
    digest = hashlib.sha512(seed).digest()
    a = int.from_bytes(digest[:32], "little")
    a &= (1 << 254) - 8
    a |= 1 << 254
    return a, digest[32:]


def ed25519_public_key(seed: bytes) -> bytes:
    a, _ = _expand_seed(seed)
    return _compress(_point_mul(a, _G))


def _sha512_of(prefix: bytes, chunks: Chunks) -> int:
    h = hashlib.sha512(prefix)
    for chunk in chunks():
        h.update(chunk)
    return int.from_bytes(h.digest(), "little")


def ed25519_sign(seed: bytes, chunks: Chunks) -> bytes:
    """Signs the message \\a chunks yields; it is read twice, as Ed25519 hashes it twice."""
    a, prefix = _expand_seed(seed)
    public = _compress(_point_mul(a, _G))
    r = _sha512_of(prefix, chunks) % _L
    big_r = _compress(_point_mul(r, _G))
    k = _sha512_of(big_r + public, chunks) % _L
    s = (r + k * a) % _L
    return big_r + int.to_bytes(s, 32, "little")


def ed25519_verify(public: bytes, chunks: Chunks, signature: bytes) -> bool:
    if len(public) != 32 or len(signature) != 64:
        return False
    a = _decompress(public)
    big_r = _decompress(signature[:32])
    if a is None or big_r is None:
        return False
    s = int.from_bytes(signature[32:], "little")
    if s >= _L:
        return False
    k = _sha512_of(signature[:32] + public, chunks) % _L
    return _point_equal(_point_mul(s, _G), _point_add(big_r, _point_mul(k, a)))


def bytes_chunks(data: bytes) -> Chunks:
    return lambda: (data,)


def file_chunks(path: Path, size: int = 1 << 20) -> Chunks:
    def read() -> Iterator[bytes]:
        with open(path, "rb") as stream:
            while chunk := stream.read(size):
                yield chunk

    return read


# ------------------------------------------------------------------------------------------------
# What a release is
# ------------------------------------------------------------------------------------------------

MANIFEST_NAME = "kite-update.json"
SIGNATURE_NAME = MANIFEST_NAME + ".sig"
MANIFEST_FORMAT = "kite-update-manifest"
MANIFEST_SCHEMA = 1

SPARKLE_NS = "http://www.andymatuschak.org/xml-namespaces/sparkle"
ET.register_namespace("sparkle", SPARKLE_NS)

# The macOS builds Sparkle updates, by CI artifact name (build.yml's matrix), with the lowest macOS
# each runs on (its deployment target). Each has its own feed, so an Intel Mac on macOS 12 stays on
# the Legacy line.
MAC_BUILDS = {
    "macOS-ARM-Qt6": "13.0",
    "macOS-Intel-Qt6": "13.0",
    "macOS-Intel-Legacy-Qt6": "12.0",
}


class FeedError(Exception):
    pass


def appcast_name(artifact: str) -> str:
    return f"appcast-{artifact}.xml"


def mac_zip_name(artifact: str, tag: str) -> str:
    return f"Kite-{artifact}-{tag}.zip"


def read_key(path: Path) -> bytes:
    try:
        seed = base64.b64decode(path.read_text().strip(), validate=True)
    except (OSError, ValueError) as error:
        raise FeedError(f"cannot read the key in {path}: {error}") from None
    if len(seed) != 32:
        raise FeedError(f"{path} does not hold a 32-byte Ed25519 seed")
    return seed


def decode_public_key(text: str, what: str) -> bytes:
    try:
        key = base64.b64decode(text, validate=True)
    except ValueError:
        key = b""
    if len(key) != 32:
        raise FeedError(f"{what} is not a base64 Ed25519 public key: {text!r}")
    return key


def read_cmake_lists(path: Path) -> dict:
    """The version and public keys the apps are built with, from the top-level CMakeLists.txt."""
    try:
        text = path.read_text()
    except OSError as error:
        raise FeedError(f"cannot read {path}: {error}") from None

    def setting(name: str, pattern: str) -> str:
        match = re.search(rf"^\s*set\(\s*{name}\s+{pattern}", text, re.MULTILINE)
        if not match:
            raise FeedError(f"{path} sets no {name}")
        return match.group(1)

    version = ".".join(
        setting(part, r"(\d+)\s*\)")
        for part in ("Kite_VERSION_MAJOR", "Kite_VERSION_MINOR", "Kite_VERSION_HOTFIX")
    )
    return {
        "version": version,
        "sparkle_public_key": decode_public_key(
            setting("MACOSX_SPARKLE_UPDATE_PUBLIC_KEY", r'"([^"]*)"'),
            "MACOSX_SPARKLE_UPDATE_PUBLIC_KEY"),
        "update_public_key": decode_public_key(
            setting("Kite_UPDATE_PUBLIC_KEY", r'"([^"]*)"'), "Kite_UPDATE_PUBLIC_KEY"),
    }


def sha256_of(path: Path) -> str:
    h = hashlib.sha256()
    for chunk in file_chunks(path)():
        h.update(chunk)
    return h.hexdigest()


def rfc822_now() -> str:
    return email.utils.format_datetime(_dt.datetime.now(_dt.timezone.utc).replace(microsecond=0))


# ------------------------------------------------------------------------------------------------
# make
# ------------------------------------------------------------------------------------------------

def build_manifest(files: list[Path], tag: str, version: str) -> bytes:
    """The manifest's bytes, exactly as they are signed and uploaded."""
    names = [path.name for path in files]
    if len(set(names)) != len(names):
        raise FeedError("two release files share a name")
    reserved = {MANIFEST_NAME, SIGNATURE_NAME}
    assets = []
    for path in sorted(files, key=lambda p: p.name):
        if path.name in reserved or path.name.startswith("appcast-"):
            continue
        assets.append({"name": path.name, "size": path.stat().st_size, "sha256": sha256_of(path)})
    if not assets:
        raise FeedError("no release files to list")
    manifest = {
        "format": MANIFEST_FORMAT,
        "schema": MANIFEST_SCHEMA,
        "name": "Kite",
        "version": version,
        "tag": tag,
        "created": _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "assets": assets,
    }
    return (json.dumps(manifest, indent=2, ensure_ascii=False) + "\n").encode("utf-8")


def build_appcast(artifact: str, zip_path: Path, tag: str, version: str, base_url: str,
                  release_page: str, seed: bytes) -> bytes:
    signature = base64.b64encode(ed25519_sign(seed, file_chunks(zip_path))).decode("ascii")

    rss = ET.Element("rss", {"version": "2.0"})
    channel = ET.SubElement(rss, "channel")
    ET.SubElement(channel, "title").text = "Kite"
    ET.SubElement(channel, "link").text = release_page.rsplit("/tag/", 1)[0]
    ET.SubElement(channel, "description").text = f"Kite updates for the {artifact} build"
    ET.SubElement(channel, "language").text = "en"

    item = ET.SubElement(channel, "item")
    ET.SubElement(item, "title").text = f"Kite {version}"
    ET.SubElement(item, "pubDate").text = rfc822_now()
    # Compared with the installed app's CFBundleVersion, which CMake sets to the same version.
    ET.SubElement(item, f"{{{SPARKLE_NS}}}version").text = version
    ET.SubElement(item, f"{{{SPARKLE_NS}}}shortVersionString").text = version
    ET.SubElement(item, f"{{{SPARKLE_NS}}}minimumSystemVersion").text = MAC_BUILDS[artifact]
    ET.SubElement(item, f"{{{SPARKLE_NS}}}fullReleaseNotesLink").text = release_page
    ET.SubElement(item, "enclosure", {
        "url": f"{base_url.rstrip('/')}/{zip_path.name}",
        "length": str(zip_path.stat().st_size),
        "type": "application/octet-stream",
        f"{{{SPARKLE_NS}}}edSignature": signature,
    })
    return serialize_appcast(rss)


def serialize_appcast(rss: ET.Element) -> bytes:
    ET.indent(rss, space="  ")
    return ET.tostring(rss, encoding="utf-8", xml_declaration=True) + b"\n"


def make(args: argparse.Namespace) -> int:
    cmake = read_cmake_lists(Path(args.cmake_lists))
    version = cmake["version"]
    tag = args.tag
    if tag != f"v{version}":
        raise FeedError(
            f"the tag {tag} is not v{version}, the version in {args.cmake_lists}. Apps built from "
            "it would report the old version and update to themselves forever.")

    sparkle_seed = read_key(Path(args.sparkle_key_file))
    update_seed = read_key(Path(args.update_key_file))
    if ed25519_public_key(sparkle_seed) != cmake["sparkle_public_key"]:
        raise FeedError("the Sparkle key does not belong to MACOSX_SPARKLE_UPDATE_PUBLIC_KEY")
    if ed25519_public_key(update_seed) != cmake["update_public_key"]:
        raise FeedError("the update key does not belong to Kite_UPDATE_PUBLIC_KEY")

    files = [Path(name) for name in args.files]
    for path in files:
        if not path.is_file():
            raise FeedError(f"{path} is not a file")
    by_name = {path.name: path for path in files}

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    base_url = args.base_url.rstrip("/")
    release_page = base_url.replace("/releases/download/", "/releases/tag/")

    written = []
    for artifact in MAC_BUILDS:
        zip_name = mac_zip_name(artifact, tag)
        if zip_name not in by_name:
            raise FeedError(f"{zip_name} is missing; Macs on {artifact} would get no update")
        appcast = build_appcast(artifact, by_name[zip_name], tag, version, base_url,
                                release_page, sparkle_seed)
        (out / appcast_name(artifact)).write_bytes(appcast)
        written.append(appcast_name(artifact))

    manifest = build_manifest(files, tag, version)
    (out / MANIFEST_NAME).write_bytes(manifest)
    signature = ed25519_sign(update_seed, bytes_chunks(manifest))
    (out / SIGNATURE_NAME).write_text(base64.b64encode(signature).decode("ascii") + "\n")
    written += [MANIFEST_NAME, SIGNATURE_NAME]

    for name in written:
        print(out / name)
    return 0


# ------------------------------------------------------------------------------------------------
# add-notes
# ------------------------------------------------------------------------------------------------

def add_notes_to_appcast(appcast: bytes, notes: str) -> bytes:
    """Sets every item's description to \\a notes, as Markdown: Sparkle draws that as text, links
    included, and never as HTML."""
    rss = ET.fromstring(appcast)
    items = rss.findall("./channel/item")
    if not items:
        raise FeedError("the appcast has no item")
    for item in items:
        for old in item.findall("description"):
            item.remove(old)
        description = ET.Element("description", {f"{{{SPARKLE_NS}}}format": "markdown"})
        description.text = notes
        # Before the enclosure, where Sparkle's own appcasts put it.
        enclosure = item.find("enclosure")
        index = list(item).index(enclosure) if enclosure is not None else len(item)
        item.insert(index, description)
    return serialize_appcast(rss)


def add_notes(args: argparse.Namespace) -> int:
    notes = Path(args.notes_file).read_text(encoding="utf-8").strip()
    if not notes:
        print("The release has no notes; the appcasts stay as they are.")
        return 0
    for name in args.appcasts:
        path = Path(name)
        path.write_bytes(add_notes_to_appcast(path.read_bytes(), notes))
        print(path)
    return 0


# ------------------------------------------------------------------------------------------------
# verify
# ------------------------------------------------------------------------------------------------

def parse_manifest(data: bytes, signature_text: str, public_key: bytes) -> dict:
    """Checks the signature first, then the shape: the order kite-updater uses."""
    try:
        signature = base64.b64decode(signature_text.strip(), validate=True)
    except ValueError:
        raise FeedError("the manifest signature is not base64") from None
    if not ed25519_verify(public_key, bytes_chunks(data), signature):
        raise FeedError("the manifest signature does not verify")
    manifest = json.loads(data)
    if manifest.get("format") != MANIFEST_FORMAT or manifest.get("schema") != MANIFEST_SCHEMA:
        raise FeedError("not a Kite update manifest of a known schema")
    for asset in manifest.get("assets", []):
        if not re.fullmatch(r"[0-9a-f]{64}", asset.get("sha256", "")):
            raise FeedError(f"{asset.get('name')} has no SHA-256")
    return manifest


def parse_appcast(data: bytes) -> list[dict]:
    rss = ET.fromstring(data)
    items = []
    for item in rss.findall("./channel/item"):
        enclosure = item.find("enclosure")
        if enclosure is None:
            raise FeedError("an appcast item has no enclosure")
        description = item.find("description")
        items.append({
            "version": item.findtext(f"{{{SPARKLE_NS}}}version"),
            "short_version": item.findtext(f"{{{SPARKLE_NS}}}shortVersionString"),
            "minimum_system": item.findtext(f"{{{SPARKLE_NS}}}minimumSystemVersion"),
            "url": enclosure.get("url"),
            "length": int(enclosure.get("length", "-1")),
            "signature": enclosure.get(f"{{{SPARKLE_NS}}}edSignature"),
            "notes": description.text if description is not None else None,
            "notes_format": (description.get(f"{{{SPARKLE_NS}}}format")
                             if description is not None else None),
        })
    return items


def verify(args: argparse.Namespace) -> int:
    cmake = read_cmake_lists(Path(args.cmake_lists))
    files = {Path(name).name: Path(name) for name in args.files}
    feeds = Path(args.feeds)

    manifest = parse_manifest((feeds / MANIFEST_NAME).read_bytes(),
                              (feeds / SIGNATURE_NAME).read_text(), cmake["update_public_key"])
    if manifest["tag"] != args.tag or manifest["version"] != cmake["version"]:
        raise FeedError("the manifest names another release")
    listed = {asset["name"]: asset for asset in manifest["assets"]}
    for name, path in files.items():
        asset = listed.get(name)
        if asset is None:
            raise FeedError(f"{name} is not in the manifest")
        if asset["size"] != path.stat().st_size or asset["sha256"] != sha256_of(path):
            raise FeedError(f"{name} does not match its manifest entry")

    for artifact in MAC_BUILDS:
        items = parse_appcast((feeds / appcast_name(artifact)).read_bytes())
        if len(items) != 1 or items[0]["version"] != cmake["version"]:
            raise FeedError(f"{appcast_name(artifact)} does not offer {cmake['version']}")
        item = items[0]
        archive = files.get(item["url"].rsplit("/", 1)[-1])
        if archive is None or item["length"] != archive.stat().st_size:
            raise FeedError(f"{appcast_name(artifact)} names a file that is not this release's")
        signature = base64.b64decode(item["signature"] or "", validate=True)
        if not ed25519_verify(cmake["sparkle_public_key"], file_chunks(archive), signature):
            raise FeedError(f"the signature in {appcast_name(artifact)} does not verify")

    print(f"{MANIFEST_NAME} lists {len(listed)} files; {len(MAC_BUILDS)} appcasts verify.")
    return 0


# ------------------------------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    commands = parser.add_subparsers(dest="command", required=True)

    make_cmd = commands.add_parser("make", help="write and sign the feeds for a release")
    make_cmd.add_argument("--tag", required=True)
    make_cmd.add_argument("--cmake-lists", required=True)
    make_cmd.add_argument("--sparkle-key-file", required=True)
    make_cmd.add_argument("--update-key-file", required=True)
    make_cmd.add_argument("--base-url", required=True,
                          help="where the release's files download from, ending in the tag")
    make_cmd.add_argument("--out", required=True)
    make_cmd.add_argument("files", nargs="+")
    make_cmd.set_defaults(run=make)

    notes_cmd = commands.add_parser("add-notes", help="put release notes into appcasts")
    notes_cmd.add_argument("--notes-file", required=True)
    notes_cmd.add_argument("appcasts", nargs="+")
    notes_cmd.set_defaults(run=add_notes)

    verify_cmd = commands.add_parser("verify", help="check the feeds as the apps do")
    verify_cmd.add_argument("--tag", required=True)
    verify_cmd.add_argument("--cmake-lists", required=True)
    verify_cmd.add_argument("--feeds", required=True, help="the directory `make` wrote to")
    verify_cmd.add_argument("files", nargs="+")
    verify_cmd.set_defaults(run=verify)

    args = parser.parse_args(argv)
    try:
        return args.run(args)
    except FeedError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
