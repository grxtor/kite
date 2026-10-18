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
 * named after the project slug (`sodium.pw.toml`). That file is the format
 * other tools in this ecosystem read and write, so this is the on-disk
 * shape of our own provenance sidecars: an instance's `mods/.index/` stays
 * readable by packwiz itself and by launchers that follow it, and packs
 * that arrive with such an index are understood without a conversion step.
 *
 * The canonical part of the format is small:
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
 * Everything we track beyond that (whether a file came in as a transitive
 * dependency, when it was installed, its size) lives under `x-kite-*`
 * keys, which is the extension namespace the format reserves; foreign
 * readers skip them instead of choking on them.
 *
 * Note that `hash-format` is load-bearing rather than decorative. Other
 * launchers record whatever digest the provider hands them - Modrinth is
 * asked for SHA-512 first - so a sidecar's hash is frequently not a SHA-1.
 * Reading one into a field that the download verifier treats as SHA-1
 * would make every affected file fail its integrity check, so the pair is
 * kept together and `Entry::sha1` is only populated when the format
 * actually says sha1.
 */
namespace Packwiz {
	/* True when `fileName` is one of our per-file TOML sidecars. */
	bool isSidecarFileName(const QString& fileName);

	/* `sodium.pw.toml` -> `sodium`. Empty for anything else. */
	QString slugFromFileName(const QString& fileName);

	/* Sidecar file name to use for `entry`.
	 *
	 * The slug is preferred, because that is the name every other tool
	 * looks a project up by. Locally added files and dependencies whose
	 * slug we never learned fall back to the archive's own base name,
	 * which is unique within the folder. */
	QString sidecarFileName(const ModMetadataIndex::Entry& entry);

	/* Name to fall back to when the preferred one is already taken by a
	 * different file - two versions of the same mod share a slug, so they
	 * would otherwise want the same sidecar and the second would erase
	 * the first. Derived from the archive name, which is unique within a
	 * folder. */
	QString fallbackSidecarFileName(const ModMetadataIndex::Entry& entry);

	/* Serialize `entry` as TOML. Returns an empty array if `entry` is not
	 * worth writing (no file name). */
	QByteArray serialize(const ModMetadataIndex::Entry& entry);

	/* Parse a sidecar. `slugHint` is the slug taken from the file name,
	 * used when the file itself does not carry one - that is how packwiz
	 * and its readers get the slug for files they wrote themselves.
	 *
	 * Returns an invalid entry (empty `fileName`) when the TOML is broken
	 * or does not describe a file. */
	ModMetadataIndex::Entry parse(const QByteArray& bytes,
								  const QString& slugHint);
}
