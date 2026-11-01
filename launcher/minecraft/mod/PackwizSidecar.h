/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <QByteArray>
#include <QString>

#include "ModMetadataIndex.h"

/*
 * Packwiz sidecar translation layer.
 *
 * packwiz keeps one TOML file per managed file in the pack's index folder,
 * named after the project slug (`sodium.pw.toml`). Our provenance sidecars use
 * the same format, so `mods/.index/` stays readable by packwiz and other
 * launchers and packs that ship such an index need no conversion.
 *
 *     name = "Sodium"
 *     filename = "sodium-fabric-0.5.8.jar"
 *     side = "client"
 *
 *     [download]
 *     mode = "url"                 # or "metadata:curseforge"
 *     url = "https://..."
 *     hash-format = "sha512"
 *     hash = "..."
 *
 *     [update.modrinth]            # or [update.curseforge]
 *     mod-id = "AANobbMI"
 *     version = "..."
 *
 * What we track beyond that (transitive dependency flag, install time, size)
 * lives under `x-kite-*` keys, the format's extension namespace; foreign
 * readers skip them.
 *
 * `hash-format` is load-bearing: other tools store whatever digest the
 * provider gave (Modrinth is asked for SHA-512 first), so the hash is often
 * not a SHA-1. `Entry::sha1` is only filled when the format says sha1.
 */
namespace Packwiz {
	// True when `fileName` is one of our per-file TOML sidecars.
	bool isSidecarFileName(const QString& fileName);

	// `sodium.pw.toml` -> `sodium`. Empty for anything else.
	QString slugFromFileName(const QString& fileName);

	// Sidecar file name for `entry`: the slug, which other tools look a project
	// up by; entries without a known slug use the archive's base name.
	QString sidecarFileName(const ModMetadataIndex::Entry& entry);

	// Fallback name when the preferred one belongs to a different file: two
	// versions of one mod share a slug and would erase each other. Derived from
	// the archive name.
	QString fallbackSidecarFileName(const ModMetadataIndex::Entry& entry);

	// Serializes `entry` as TOML; empty if it has no file name.
	QByteArray serialize(const ModMetadataIndex::Entry& entry);

	// Parses a sidecar. `slugHint` (from the file name) is used when the file
	// carries no slug. Returns an invalid entry (empty `fileName`) if the TOML
	// is broken or describes no file.
	ModMetadataIndex::Entry parse(const QByteArray& bytes,
								  const QString& slugHint);
}
