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

/* What is wrong with an instance's mods folder, worked out from what the
 * files say about themselves - no network, no game launch.
 *
 * Asks the questions the loader asks at startup, the same way: is every
 * required mod there, at a version it accepts; is anything there that a
 * mod refuses to run with; is a mod there twice; is a file built for a
 * loader this instance does not have. Whatever the loader would refuse
 * to start over is `blocking`.
 *
 * A pure function over plain data, so it is tested on made-up folders and
 * knows nothing about Mod, ModFolderModel or the instance: the caller
 * reads those into Entry and Environment. */
namespace ModHealth
{
	/* The game and loader the mods will run on. */
	struct Environment {
		/// The instance's Minecraft version; empty when unknown.
		QString minecraft;
		/// The mod loaders the instance has, by platform id ("fabric",
		/// "quilt", "forge", "neoforge"), its primary loader first. No
		/// loader means nothing is checked: the mods tab already asks for
		/// one.
		QStringList loaders;
		/// Ids the game and its loader answer to themselves, with their
		/// versions where known ("" otherwise): "minecraft",
		/// "fabricloader", "quilt_loader", "forge", "neoforge", ... A
		/// dependency on one of these is checked against that version and
		/// is never "missing".
		QHash<QString, QString> builtIns;
	};

	/* One file of the folder. */
	struct Entry {
		/// How the caller tells files apart (ContentFilterModel's key).
		QString key;
		/// What to call it.
		QString name;
		bool enabled = true;
		/// What the file says about itself; default-constructed for one
		/// not read yet, which then takes part in nothing.
		ModDetails details;
	};

	enum class Kind {
		/// A required mod nobody provides.
		MissingDependency,
		/// A required mod that is installed but switched off.
		DisabledDependency,
		/// A required mod that is installed and on, at a version outside
		/// the range asked for.
		WrongDependencyVersion,
		/// "minecraft" outside the range asked for.
		WrongMinecraft,
		/// The loader itself ("fabricloader", "forge", ...) outside the
		/// range asked for.
		WrongLoaderVersion,
		/// A mod it declares it cannot run with is installed and on.
		Incompatible,
		/// A mod it declares it should not run with: a warning only.
		Discouraged,
		/// Another enabled file has the same mod id.
		Duplicate,
		/// Built only for loaders this instance does not have: the loader
		/// will not load it at all.
		WrongLoader
	};

	struct Issue {
		Kind kind = Kind::MissingDependency;
		/// Whether the loader refuses to start over it.
		bool blocking = true;
		/// The file the issue is about.
		QString key;
		QString name;
		/// The id on the other end: the dependency, the mod it clashes
		/// with, the duplicated id, "minecraft", the loader's id. Empty
		/// for WrongLoader.
		QString otherId;
		/// The installed file behind otherId - the switched-off
		/// dependency, the one at the wrong version, the one it clashes
		/// with, the other copy - or "" when there is none.
		QString otherKey;
		/// otherKey's name, or otherId when there is no file.
		QString otherName;
		/// The declaration behind every kind but Duplicate and
		/// WrongLoader.
		ModDependency dependency;
		/// For the wrong-version kinds: the version that is there.
		QString foundVersion;
	};

	/* The loaders whose metadata this environment reads, in the order a
	 * file built for several of them is read by: the instance's own
	 * loaders first, then the ones they also load -
	 *   quilt    also reads fabric
	 *   neoforge also reads forge before Minecraft 1.20.5 (later
	 *            versions read only neoforge.mods.toml)
	 *   forge, neoforge also read fabric when @p hasConnector (Sinytra
	 *            Connector, mod id "connectormod" in its builds for 1.20.1
	 *            and "connector" in newer ones, runs Fabric mods there). */
	QStringList readableLoaders(const Environment& environment,
								bool hasConnector);

	/* Every issue in @p entries, blocking ones first, then by name.
	 *
	 * Rules, per enabled entry that has been read:
	 *   - details.loaders non-empty and none of them readable: one
	 *     WrongLoader issue (not blocking - the file is skipped, the game
	 *     still starts) and nothing else about that entry.
	 *   - Its dependencies are those whose `loader` is the first of
	 *     readableLoaders() the file was built for.
	 *   - An id is provided by builtIns, by the mod_id of any entry and by
	 *     the `provides` of any entry. Ids compare case-insensitively.
	 *     "java", "fml", "javafml", "lowcodefml" and "mixinextras" are
	 *     treated as always present: they are the runtime's to provide.
	 *     A file with no metadata of its own can still provide the mods it
	 *     carries (kotlinforforge-all.jar, the Connector jar).
	 *   - Sinytra Connector (an enabled entry that is or provides
	 *     "connectormod" or "connector") is found the same way. A file
	 *     whose Fabric metadata is read through it (the instance has
	 *     neither Fabric nor Quilt among its loaders) looks ids up with "-"
	 *     and "_" taken for the same, as Connector does ("fabric-api" is the
	 *     Forgified Fabric API's "fabric_api"); no other file does, in the
	 *     required and the clash checks alike.
	 *   - Required:
	 *       builtIn whose version is No -> WrongMinecraft ("minecraft")
	 *         or WrongLoaderVersion (any other built-in);
	 *       enabled providers, at least one Yes or Unknown -> fine;
	 *       enabled providers, all No -> WrongDependencyVersion;
	 *       only disabled providers -> DisabledDependency;
	 *       no provider -> MissingDependency.
	 *     All blocking.
	 *     A built-in also answers to the versions the loader's own
	 *     VersionSupportMatrix (FML, net.neoforged.fml.loading and
	 *     net.minecraftforge.fml.loading) adds for the game it runs, and
	 *     the range is met when any of them is: on NeoForge with Minecraft
	 *     1.21.1 "minecraft" is also 1.21 and "neoforge" also 21.0.166 (so
	 *     [1.21,1.21.1) holds, as the loader says); on Forge with 1.20.1
	 *     "forge" is also 47.1.79. Only required dependencies use this.
	 *   - Incompatible / Discouraged: an enabled provider other than the
	 *     entry itself, or a built-in, whose version is Yes (an empty
	 *     range is Yes) -> Incompatible (blocking) / Discouraged (not).
	 *     Unknown is not reported.
	 *   - Duplicate: two or more enabled entries with the same non-empty
	 *     mod_id -> one issue for each of them, blocking.
	 * The same (kind, key, otherId) is reported once. */
	QList<Issue> check(const QList<Entry>& entries,
					   const Environment& environment);
} // namespace ModHealth
