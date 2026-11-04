#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
"""Tests for kite_update_feed.py against a fake release directory.

  python3 -m unittest discover -s packaging/update-feed
"""

from __future__ import annotations

import base64
import contextlib
import hashlib
import io
import json
import re
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import kite_update_feed as feed  # noqa: E402

REPO = Path(__file__).resolve().parents[2]

# RFC 8032, section 7.1: TEST 1, 2 and 3.
RFC8032_VECTORS = [
    ("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
     "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
     "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"),
    ("4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
     "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
     "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"),
    ("c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
     "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
     "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a"),
]

SPARKLE_SEED = bytes(range(32))
UPDATE_SEED = bytes(range(32, 64))

RELEASE_FILES = [
    "Kite-Linux-Qt6-x86_64.AppImage",
    "Kite-Linux-Qt6-x86_64.AppImage.zsync",
    "Kite-Linux-Qt6-Portable-{tag}.tar.gz",
    "Kite-Windows-MSVC-Qt6-Setup-{tag}.exe",
    "Kite-Windows-MSVC-Qt6-Portable-{tag}.zip",
    "Kite-macOS-ARM-Qt6-{tag}.zip",
    "Kite-macOS-ARM-Qt6-{tag}.dmg",
    "Kite-macOS-Intel-Qt6-{tag}.zip",
    "Kite-macOS-Intel-Legacy-Qt6-{tag}.zip",
]


def cmake_lists(version: str = "1.3.0", sparkle_seed: bytes = SPARKLE_SEED,
                update_seed: bytes = UPDATE_SEED) -> str:
    major, minor, hotfix = version.split(".")
    sparkle = base64.b64encode(feed.ed25519_public_key(sparkle_seed)).decode()
    update = base64.b64encode(feed.ed25519_public_key(update_seed)).decode()
    return (f"set(Kite_VERSION_MAJOR    {major})\n"
            f"set(Kite_VERSION_MINOR    {minor})\n"
            f"set(Kite_VERSION_HOTFIX   {hotfix})\n"
            f'    set(MACOSX_SPARKLE_UPDATE_PUBLIC_KEY "{sparkle}" CACHE STRING "x")\n'
            f'set(Kite_UPDATE_PUBLIC_KEY "{update}" CACHE STRING "x")\n')


class FakeRelease:
    """A release directory with small files standing in for the real packages."""

    def __init__(self, root: Path, tag: str = "v1.3.0", version: str = "1.3.0"):
        self.root = root
        self.tag = tag
        self.base_url = f"https://github.com/grxtor/kite/releases/download/{tag}"
        self.cmake = root / "CMakeLists.txt"
        self.cmake.write_text(cmake_lists(version))
        self.sparkle_key = root / "sparkle.key"
        self.sparkle_key.write_text(base64.b64encode(SPARKLE_SEED).decode() + "\n")
        self.update_key = root / "update.key"
        self.update_key.write_text(base64.b64encode(UPDATE_SEED).decode())
        self.release = root / "release"
        self.release.mkdir()
        self.files = []
        for index, pattern in enumerate(RELEASE_FILES):
            path = self.release / pattern.format(tag=tag)
            path.write_bytes(hashlib.sha512(path.name.encode()).digest() * (index + 3))
            self.files.append(path)
        self.out = root / "feeds"

    def make(self, *extra: str) -> tuple[int, str]:
        return run(["make", "--tag", self.tag, "--cmake-lists", str(self.cmake),
                    "--sparkle-key-file", str(self.sparkle_key),
                    "--update-key-file", str(self.update_key),
                    "--base-url", self.base_url, "--out", str(self.out), *extra,
                    *map(str, self.files)])

    def verify(self) -> tuple[int, str]:
        return run(["verify", "--tag", self.tag, "--cmake-lists", str(self.cmake),
                    "--feeds", str(self.out), *map(str, self.files)])


def run(argv: list[str]) -> tuple[int, str]:
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        code = feed.main(argv)
    return code, out.getvalue() + err.getvalue()


class Ed25519Test(unittest.TestCase):
    def test_rfc8032_vectors(self):
        for secret, public, message, signature in RFC8032_VECTORS:
            seed, msg = bytes.fromhex(secret), bytes.fromhex(message)
            self.assertEqual(feed.ed25519_public_key(seed).hex(), public)
            self.assertEqual(feed.ed25519_sign(seed, feed.bytes_chunks(msg)).hex(), signature)
            self.assertTrue(feed.ed25519_verify(bytes.fromhex(public), feed.bytes_chunks(msg),
                                                bytes.fromhex(signature)))

    def test_tampering_fails(self):
        secret, public, _, _ = RFC8032_VECTORS[1]
        seed, key = bytes.fromhex(secret), bytes.fromhex(public)
        signature = feed.ed25519_sign(seed, feed.bytes_chunks(b"kite"))
        self.assertFalse(feed.ed25519_verify(key, feed.bytes_chunks(b"kitf"), signature))
        broken = bytes([signature[0] ^ 1]) + signature[1:]
        self.assertFalse(feed.ed25519_verify(key, feed.bytes_chunks(b"kite"), broken))
        other = feed.ed25519_public_key(bytes(32))
        self.assertFalse(feed.ed25519_verify(other, feed.bytes_chunks(b"kite"), signature))
        # s >= L is rejected, not reduced.
        high = signature[:32] + int.to_bytes(feed._L + 1, 32, "little")
        self.assertFalse(feed.ed25519_verify(key, feed.bytes_chunks(b"kite"), high))

    def test_streamed_file_signs_like_bytes(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "blob"
            data = bytes(range(256)) * 9000
            path.write_bytes(data)
            self.assertEqual(feed.ed25519_sign(SPARKLE_SEED, feed.file_chunks(path, size=4096)),
                             feed.ed25519_sign(SPARKLE_SEED, feed.bytes_chunks(data)))


class MakeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.release = FakeRelease(Path(self.tmp.name))

    def tearDown(self):
        self.tmp.cleanup()

    def test_writes_every_feed_and_they_verify(self):
        code, output = self.release.make()
        self.assertEqual(code, 0, output)
        names = sorted(path.name for path in self.release.out.iterdir())
        self.assertEqual(names, sorted([
            "appcast-macOS-ARM-Qt6.xml", "appcast-macOS-Intel-Qt6.xml",
            "appcast-macOS-Intel-Legacy-Qt6.xml", "kite-update.json", "kite-update.json.sig"]))
        code, output = self.release.verify()
        self.assertEqual(code, 0, output)

    def test_manifest_lists_every_file_with_its_hash(self):
        self.release.make()
        data = (self.release.out / "kite-update.json").read_bytes()
        signature = (self.release.out / "kite-update.json.sig").read_text()
        manifest = feed.parse_manifest(data, signature, feed.ed25519_public_key(UPDATE_SEED))
        self.assertEqual(manifest["tag"], "v1.3.0")
        self.assertEqual(manifest["version"], "1.3.0")
        self.assertRegex(manifest["created"], r"^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\dZ$")
        listed = {asset["name"]: asset for asset in manifest["assets"]}
        self.assertEqual(set(listed), {path.name for path in self.release.files})
        for path in self.release.files:
            self.assertEqual(listed[path.name]["size"], path.stat().st_size)
            self.assertEqual(listed[path.name]["sha256"],
                             hashlib.sha256(path.read_bytes()).hexdigest())

    def test_manifest_signature_is_over_the_exact_bytes(self):
        self.release.make()
        data = (self.release.out / "kite-update.json").read_bytes()
        signature = (self.release.out / "kite-update.json.sig").read_text()
        key = feed.ed25519_public_key(UPDATE_SEED)
        reformatted = json.dumps(json.loads(data)).encode()
        with self.assertRaisesRegex(feed.FeedError, "does not verify"):
            feed.parse_manifest(reformatted, signature, key)
        with self.assertRaisesRegex(feed.FeedError, "does not verify"):
            feed.parse_manifest(data, signature, feed.ed25519_public_key(SPARKLE_SEED))

    def test_appcast_offers_the_zip_signed_for_sparkle(self):
        self.release.make()
        for artifact, minimum in feed.MAC_BUILDS.items():
            data = (self.release.out / f"appcast-{artifact}.xml").read_bytes()
            self.assertIn(b'xmlns:sparkle="http://www.andymatuschak.org/xml-namespaces/sparkle"',
                          data)
            items = feed.parse_appcast(data)
            self.assertEqual(len(items), 1)
            item = items[0]
            zip_name = f"Kite-{artifact}-v1.3.0.zip"
            zip_path = self.release.release / zip_name
            self.assertEqual(item["version"], "1.3.0")
            self.assertEqual(item["short_version"], "1.3.0")
            self.assertEqual(item["minimum_system"], minimum)
            self.assertEqual(item["url"], f"{self.release.base_url}/{zip_name}")
            self.assertEqual(item["length"], zip_path.stat().st_size)
            self.assertIsNone(item["notes"])
            signature = base64.b64decode(item["signature"])
            self.assertTrue(feed.ed25519_verify(feed.ed25519_public_key(SPARKLE_SEED),
                                                feed.bytes_chunks(zip_path.read_bytes()),
                                                signature))

    def test_a_tag_that_is_not_the_cmake_version_fails(self):
        self.release.cmake.write_text(cmake_lists("1.2.0"))
        code, output = self.release.make()
        self.assertEqual(code, 1)
        self.assertIn("is not v1.2.0", output)
        self.assertFalse(self.release.out.exists())

    def test_a_key_that_is_not_the_built_in_one_fails(self):
        self.release.cmake.write_text(cmake_lists(update_seed=bytes(32)))
        code, output = self.release.make()
        self.assertEqual(code, 1)
        self.assertIn("Kite_UPDATE_PUBLIC_KEY", output)
        self.release.cmake.write_text(cmake_lists(sparkle_seed=bytes(32)))
        code, output = self.release.make()
        self.assertEqual(code, 1)
        self.assertIn("MACOSX_SPARKLE_UPDATE_PUBLIC_KEY", output)

    def test_a_missing_mac_build_fails(self):
        self.release.files = [path for path in self.release.files
                              if "Intel-Legacy" not in path.name]
        code, output = self.release.make()
        self.assertEqual(code, 1)
        self.assertIn("Kite-macOS-Intel-Legacy-Qt6-v1.3.0.zip is missing", output)

    def test_verify_notices_a_changed_file(self):
        self.release.make()
        exe = next(path for path in self.release.files if path.name.endswith(".exe"))
        exe.write_bytes(exe.read_bytes() + b"!")
        code, output = self.release.verify()
        self.assertEqual(code, 1)
        self.assertIn("does not match its manifest entry", output)

    def test_verify_notices_a_swapped_appcast(self):
        self.release.make()
        arm = self.release.out / "appcast-macOS-ARM-Qt6.xml"
        intel = self.release.out / "appcast-macOS-Intel-Qt6.xml"
        arm.write_bytes(re.sub(rb'edSignature="[^"]*"',
                               re.search(rb'edSignature="[^"]*"', intel.read_bytes()).group(0),
                               arm.read_bytes()))
        code, output = self.release.verify()
        self.assertEqual(code, 1)
        self.assertIn("does not verify", output)


class AddNotesTest(unittest.TestCase):
    def test_notes_become_markdown_and_signatures_stay(self):
        with tempfile.TemporaryDirectory() as tmp:
            release = FakeRelease(Path(tmp))
            release.make()
            appcast = release.out / "appcast-macOS-ARM-Qt6.xml"
            before = feed.parse_appcast(appcast.read_bytes())[0]
            notes = Path(tmp) / "notes.md"
            notes.write_text("## Öne çıkanlar\n\n- **Bold** & <b>not html</b> ]]> done\n",
                             encoding="utf-8")
            code, output = run(["add-notes", "--notes-file", str(notes), str(appcast)])
            self.assertEqual(code, 0, output)
            after = feed.parse_appcast(appcast.read_bytes())[0]
            self.assertEqual(after["notes_format"], "markdown")
            self.assertEqual(after["notes"],
                             "## Öne çıkanlar\n\n- **Bold** & <b>not html</b> ]]> done")
            self.assertEqual({k: v for k, v in after.items() if not k.startswith("notes")},
                             {k: v for k, v in before.items() if not k.startswith("notes")})
            # The markup is escaped text, not elements.
            self.assertIn(b"&lt;b&gt;not html&lt;/b&gt;", appcast.read_bytes())
            # Running it again replaces the notes rather than adding a second description.
            notes.write_text("Second", encoding="utf-8")
            run(["add-notes", "--notes-file", str(notes), str(appcast)])
            self.assertEqual(appcast.read_bytes().count(b"<description"), 2)  # channel + item
            self.assertEqual(feed.parse_appcast(appcast.read_bytes())[0]["notes"], "Second")
            self.assertEqual(release.verify()[0], 0)

    def test_empty_notes_leave_the_appcast_alone(self):
        with tempfile.TemporaryDirectory() as tmp:
            release = FakeRelease(Path(tmp))
            release.make()
            appcast = release.out / "appcast-macOS-ARM-Qt6.xml"
            before = appcast.read_bytes()
            notes = Path(tmp) / "notes.md"
            notes.write_text("  \n")
            self.assertEqual(run(["add-notes", "--notes-file", str(notes), str(appcast)])[0], 0)
            self.assertEqual(appcast.read_bytes(), before)


class RepositoryTest(unittest.TestCase):
    """The real CMakeLists.txt is what the release workflow reads."""

    def test_cmake_lists_has_a_version_and_both_keys(self):
        cmake = feed.read_cmake_lists(REPO / "CMakeLists.txt")
        self.assertRegex(cmake["version"], r"^\d+\.\d+\.\d+$")
        self.assertEqual(len(cmake["sparkle_public_key"]), 32)
        self.assertEqual(len(cmake["update_public_key"]), 32)
        self.assertNotEqual(cmake["sparkle_public_key"], cmake["update_public_key"])

    def test_every_mac_build_has_a_ci_job(self):
        build = (REPO / ".github/workflows/build.yml").read_text()
        for artifact, minimum in feed.MAC_BUILDS.items():
            name = artifact.removesuffix("-Qt6")
            match = re.search(rf"artifact-name: {re.escape(name)}\n.*?macosx-deployment-target: "
                              rf"([\d.]+)", build, re.DOTALL)
            self.assertIsNotNone(match, artifact)
            self.assertEqual(match.group(1), minimum, artifact)


if __name__ == "__main__":
    unittest.main()
