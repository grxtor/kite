/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include "ModDetails.h"

// What is wrong with an instance's mods folder, from what the files say about
// themselves (no network, no launch). It asks what the loader asks at startup:
// required mods present and in range, no refused combination, no duplicate, no
// file built for another loader; what the loader would refuse is `blocking`.
// A pure function over plain data: the caller fills Entry and Environment.
namespace ModHealth
{
	struct Environment {
		/// The instance's Minecraft version; empty when unknown.
		QString minecraft;
		/// The instance's loaders by platform id ("fabric", "quilt", "forge",
		/// "neoforge"), primary first. No loader means nothing is checked.
		QStringList loaders;
		/// Ids the game and loader answer to themselves ("minecraft",
		/// "fabricloader", "forge", ...) with their versions ("" if unknown). A
		/// dependency on one is checked against that version and is never
		/// "missing".
		QHash<QString, QString> builtIns;
	};

	struct Entry {
		/// How the caller tells files apart.
		QString key;
		QString name;
		bool enabled = true;
		/// What the file says about itself; default-constructed (not read yet)
		/// takes part in nothing.
		ModDetails details;
	};

	enum class Kind {
		MissingDependency,
		DisabledDependency,
		WrongDependencyVersion,
		WrongMinecraft,
		WrongLoaderVersion,
		Incompatible,
		Discouraged,
		Duplicate,
		/// Built only for loaders this instance does not have; never loaded.
		WrongLoader
	};

	struct Issue {
		Kind kind = Kind::MissingDependency;
		/// Whether the loader refuses to start over it.
		bool blocking = true;
		QString key;
		QString name;
		/// The id on the other end (dependency, clashing mod, duplicated id,
		/// loader id); empty for WrongLoader.
		QString otherId;
		/// The installed file behind otherId, or "" if there is none.
		QString otherKey;
		/// otherKey's name, or otherId when there is no file.
		QString otherName;
		/// The declaration behind every kind but Duplicate and WrongLoader.
		ModDependency dependency;
		/// For the wrong-version kinds: the version that is there.
		QString foundVersion;
	};

	// The loaders whose metadata the environment reads, in reading order: the
	// instance's own, then the ones they also load. Quilt also reads fabric;
	// neoforge also reads forge before Minecraft 1.20.5; forge and neoforge
	// also read fabric when @p hasConnector (Sinytra Connector).
	QStringList readableLoaders(const Environment& environment,
								bool hasConnector);

	// Every issue in @p entries, blocking ones first, then by name; the same
	// (kind, key, otherId) is reported once. Rules, per enabled entry that has
	// been read:
	//  - details.loaders non-empty and none readable: one non-blocking
	//    WrongLoader (the file is skipped) and nothing else for that entry.
	//  - Its dependencies are those for the first of readableLoaders() it was
	//    built for.
	//  - An id is provided by builtIns, any entry's mod_id or `provides`,
	//    compared case-insensitively. "java", "fml", "javafml", "lowcodefml"
	//    and "mixinextras" always count as present. A file without metadata
	//    can still provide the mods it carries (kotlinforforge-all.jar, the
	//    Connector jar).
	//  - A file whose Fabric metadata is read through Sinytra Connector (an
	//    enabled entry that is or provides "connectormod" or "connector"; the
	//    instance has neither Fabric nor Quilt) looks ids up with "-" and "_"
	//    as equal.
	//  - Required (all blocking): a built-in whose version is No gives
	//    WrongMinecraft ("minecraft") or WrongLoaderVersion; enabled providers
	//    with one Yes/Unknown are fine, all No give WrongDependencyVersion;
	//    only disabled providers give DisabledDependency; none gives
	//    MissingDependency. A built-in also answers to the versions the
	//    loader's VersionSupportMatrix adds for the game, and the range holds
	//    if any matches (NeoForge on 1.21.1: "minecraft" is also 1.21,
	//    "neoforge" also 21.0.166; Forge on 1.20.1: "forge" is also 47.1.79).
	//    Only required dependencies use this.
	//  - Incompatible (blocking) / Discouraged: an enabled provider other than
	//    the entry itself, or a built-in, whose version is Yes (empty range =
	//    Yes). Unknown is not reported.
	//  - Duplicate: two or more enabled entries with the same non-empty mod_id,
	//    one blocking issue for each.
	QList<Issue> check(const QList<Entry>& entries,
					   const Environment& environment);
} // namespace ModHealth
