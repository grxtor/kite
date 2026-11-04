# SPDX-FileCopyrightText: 2026 grxtor
# SPDX-License-Identifier: Apache-2.0
"""Writes the Modrinth App fixtures: modrinth/app.db and modrinth-wal/app.db
with its write-ahead log. Synthetic data only.

  python3 -I make_modrinth_db.py      (from this folder)

The tables follow the Modrinth App's own (profiles, cache), cut to the columns
Kite reads plus a few it skips. One row has a large "groups" value so it spills
onto overflow pages, and the cache has enough rows for an interior page.
"""
import hashlib
import json
import os
import shutil
import sqlite3

HERE = os.path.dirname(os.path.abspath(__file__))

PROFILES = """CREATE TABLE profiles (
    path TEXT NOT NULL,
    install_stage TEXT NOT NULL,
    name TEXT NOT NULL,
    icon_path TEXT NULL,
    game_version TEXT NOT NULL,
    mod_loader TEXT NOT NULL,
    mod_loader_version TEXT NULL,
    groups JSONB NOT NULL,
    created INTEGER NOT NULL,
    modified INTEGER NOT NULL,
    last_played INTEGER NULL,
    PRIMARY KEY (path)
)"""
CACHE = """CREATE TABLE cache (
    id TEXT NOT NULL,
    data_type TEXT NOT NULL,
    alias TEXT NULL,
    data JSONB NULL,
    created INTEGER NOT NULL,
    expires INTEGER NOT NULL,
    UNIQUE (data_type, alias),
    UNIQUE (data_type, id)
)"""

FILES = {
    "Fabric Profile/mods/sodium.jar": b"synthetic jar: modrinth sodium\n",
    "Fabric Profile/mods/iris.jar": b"synthetic jar: modrinth iris\n",
    "Fabric Profile/mods/unknown.jar": b"synthetic jar: not on modrinth\n",
    "Fabric Profile/shaderpacks/BSL.zip": b"synthetic shader pack\n",
    "Fabric Profile/saves/World/level.dat": b"synthetic level data\n",
    "Fabric Profile/options.txt": b"lang:en_us\n",
    "Forge Profile/options.txt": b"lang:en_us\n",
    "Installing/options.txt": b"lang:en_us\n",
    "WAL Profile/options.txt": b"lang:en_us\n",
}


def sha1(data):
    return hashlib.sha1(data).hexdigest()


def write_profiles(root, names):
    for rel, data in FILES.items():
        if rel.split("/")[0] not in names:
            continue
        path = os.path.join(root, "profiles", rel)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data)


def fill(con):
    con.execute(PROFILES)
    con.execute(CACHE)
    profiles = [
        ("Fabric Profile", "installed", "Fabric Profile", None, "1.20.1", "fabric", "0.15.11",
         json.dumps(["group-%04d" % i for i in range(800)]), 1700000000, 1700000000, 1730000000),
        ("Forge Profile", "installed", "Forge Profile", None, "1.20.1", "forge", "1.20.1-47.2.0",
         "[]", 1700000000, 1700000000, None),
        ("Installing", "pack_installing", "Installing", None, "1.21.1", "fabric", "0.16.5",
         "[]", 1700000000, 1700000000, None),
        ("../escape", "installed", "Escape", None, "1.20.1", "vanilla", None,
         "[]", 1700000000, 1700000000, None),
    ]
    con.executemany("INSERT INTO profiles VALUES (?,?,?,?,?,?,?,?,?,?,?)", profiles)
    sodium = sha1(FILES["Fabric Profile/mods/sodium.jar"])
    iris = sha1(FILES["Fabric Profile/mods/iris.jar"])
    version = {"id": "OihdIimA", "project_id": "AANobbMI", "name": "Sodium 0.5.8",
               "version_number": "mc1.20.1-0.5.8",
               "files": [{"hashes": {"sha1": sodium, "sha512": "x"}, "url":
                          "https://cdn.modrinth.com/data/AANobbMI/versions/OihdIimA/sodium.jar",
                          "filename": "sodium.jar", "primary": True, "size": 30}]}
    rows = [("OihdIimA", "version", None, json.dumps(version), 1, 2),
            (iris, "file", "Fabric Profile/mods/iris.jar",
             json.dumps({"hash": iris, "project_id": "YL57xq9U", "version_id": "s5eFLITc"}), 1, 2)]
    for i in range(300):
        rows.append(("filler-%04d" % i, "project", None,
                     json.dumps({"id": "filler-%04d" % i, "title": "Filler project %d" % i,
                                 "description": "synthetic " * 12}), 1, 2))
    con.executemany("INSERT INTO cache VALUES (?,?,?,?,?,?)", rows)
    con.commit()


def plain():
    root = os.path.join(HERE, "modrinth")
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(root)
    con = sqlite3.connect(os.path.join(root, "app.db"))
    fill(con)
    con.close()
    write_profiles(root, {"Fabric Profile", "Forge Profile", "Installing"})


def with_wal():
    root = os.path.join(HERE, "modrinth-wal")
    shutil.rmtree(root, ignore_errors=True)
    os.makedirs(root)
    work = os.path.join(root, "work.db")
    con = sqlite3.connect(work)
    con.execute("PRAGMA journal_mode=WAL")
    fill(con)
    con.execute("PRAGMA wal_checkpoint(TRUNCATE)")
    con.execute("PRAGMA wal_autocheckpoint=0")
    # Only in the log: what a running Modrinth App has not checkpointed yet.
    con.execute("INSERT INTO profiles VALUES (?,?,?,?,?,?,?,?,?,?,?)",
                ("WAL Profile", "installed", "WAL Profile", None, "1.21.1", "vanilla", None,
                 "[]", 1700000000, 1700000000, 1731000000))
    con.commit()
    shutil.copyfile(work, os.path.join(root, "app.db"))
    shutil.copyfile(work + "-wal", os.path.join(root, "app.db-wal"))
    con.close()
    for leftover in (work, work + "-wal", work + "-shm"):
        if os.path.exists(leftover):
            os.remove(leftover)
    write_profiles(root, {"Fabric Profile", "Forge Profile", "Installing", "WAL Profile"})


if __name__ == "__main__":
    plain()
    with_wal()
