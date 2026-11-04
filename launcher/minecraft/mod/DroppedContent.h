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

#include <QString>
#include <QStringList>
#include <QUrl>

#include "ModDetails.h"
#include "ModHealth.h"

/* What a file or folder dropped on Kite is, from what is inside it rather
 * than its name: a mod, a resource, data or shader pack, a world or a modpack.
 * Everything here only reads; DropPlan decides where things go. */
namespace DroppedContent
{
	enum class Kind {
		Unknown,
		Mod,
		ResourcePack,
		DataPack,
		ShaderPack,
		World,
		/// A .mrpack, or an archive InstanceImportTask can import (CurseForge,
		/// MultiMC/Prism, Technic): the New instance flow takes it.
		Modpack
	};

	struct Item {
		Kind kind = Kind::Unknown;
		/// Local path to install from. A world folder dropped inside a wrapping
		/// folder points at the world, not the wrapper.
		QString path;
		/// The dropped thing's own name.
		QString fileName;
		bool isFolder = false;
		/// Mods only; empty for a jar that declares nothing (old mods).
		QString modId;
		QString name;
		QString version;
		/// Mods only: everything the metadata says, for the compatibility check.
		ModDetails details;
		/// Whether any metadata was found (details is meaningful).
		bool hasMetadata = false;
	};

	/// @p fileUrlOrPath: a file:// URL or a local path. Cheap: reads an archive's
	/// table of contents and, for mods, a few metadata files; never extracts.
	Item classify(const QString& fileUrlOrPath);

	/// What was dropped, by kind; the words of "2 mods and 1 resource pack".
	struct Counts {
		int mods = 0;
		int resourcePacks = 0;
		int dataPacks = 0;
		int shaderPacks = 0;
		int worlds = 0;
		int modpacks = 0;
		/// Files that are none of the above; not part of total().
		int unknown = 0;

		void add(Kind kind);
		int total() const;
	};

	/// "2 mods and 1 resource pack" (translated, plurals by count); empty for no
	/// counts. Tells the toast what a drop did.
	QString describe(const Counts& counts);

	/// "2 mods", "1 resource pack": one kind and its count, for one line of the
	/// drag overlay; empty for Unknown or a count of 0.
	QString describe(Kind kind, int count);

	/// "1 file Kite doesn't recognise" / "3 files Kite doesn't recognise".
	QString describeUnknown(int count);

	/// The ContentTab / InstanceContentController kind a dropped item is
	/// installed under: "mods", "resourcepacks", "shaderpacks"; empty for worlds,
	/// data packs (they need a world), modpacks and unknown files.
	QString contentKind(Kind kind);

	/// Where @p item would conflict in the folder @p dir: the file there with
	/// the same content (a duplicate, to skip). Empty when none; folders never
	/// match. Compares sizes first, so only files of equal size are hashed.
	QString identicalFileIn(const QString& dir, const Item& item);

	/// The file in @p dir (mods folder) that declares the same mod id as
	/// @p mod, a disabled copy included; empty when none or when @p mod has no
	/// id.
	QString sameModIn(const QString& dir, const Item& mod);

	/// A sentence for a mod built for another loader or Minecraft version than
	/// the instance runs; empty when it fits or nothing is known.
	QString compatibilityProblem(const Item& mod,
								 const ModHealth::Environment& environment);
	/// A link to a project page of a platform Kite installs from.
	struct ProjectLink {
		/// "modrinth" or "curseforge"; empty when the URL is neither.
		QString provider;
		/// "mods", "resourcepacks", "shaderpacks" or "datapacks".
		QString contentType;
		QString slug;

		bool valid() const
		{
			return !provider.isEmpty() && !slug.isEmpty();
		}
	};

	/// modrinth.com/mod|resourcepack|shader|datapack/<slug> and
	/// curseforge.com/minecraft/mc-mods|texture-packs|shaders|data-packs/<slug>,
	/// with or without www. and with trailing version or file parts. Modpack
	/// links and everything else give an invalid link: they are for Import.
	ProjectLink parseProjectLink(const QUrl& url);

	/// Where an instance keeps what a drop can add to it.
	struct Target {
		QString modsDir;
		QString resourcePacksDir;
		QString shaderPacksDir;
		/// The instance's Minecraft version and loaders, for the fit check.
		ModHealth::Environment environment;
	};

	/// One dropped thing and what is already there for it.
	struct Entry {
		Item item;
		/// The folder it is installed into; empty for worlds and data packs,
		/// which have no fixed folder (a world's datapacks folder is picked).
		QString dir;
		/// The file in @p dir with the same content (nothing to add).
		QString identical;
		/// Another version of the same mod in @p dir.
		QString sameMod;
		/// compatibilityProblem() of a mod that is going in.
		QString warning;
	};

	/// A drop sorted: what goes where, and the rest.
	struct Plan {
		/// Mods, packs, data packs and worlds, in the order dropped.
		QList<Entry> entries;
		/// Local paths of modpacks, for Import.
		QStringList modpacks;
		/// Modrinth / CurseForge project links, and any other web links.
		QStringList projectLinks;
		QStringList otherLinks;
		/// Names of files that are none of the above.
		QStringList unknown;
		/// Everything recognised, modpacks included.
		Counts counts;
	};

	/// Sorts @p sources (file:// URLs, paths, web links) and looks in the
	/// folders of @p target for what each would meet there. Only reads.
	Plan plan(const QStringList& sources, const Target& target);

	/// What a drop did, for the sentence that tells the user.
	struct Outcome {
		Counts added;
		/// Older copies of added mods that were swapped out.
		int replaced = 0;
		/// Files already in the instance, byte for byte.
		int identical = 0;
		QStringList failed;
		/// Files that must wait for the game to close.
		QStringList locked;
		QStringList unknown;
		/// compatibilityProblem() sentences of the mods that went in anyway.
		QStringList warnings;
	};

	/// The toast: "Added 3 mods and 1 shader pack. ...". @p tone is one of
	/// "success", "warning", "danger" or "neutral".
	QString summarize(const Outcome& outcome, QString* tone);
} // namespace DroppedContent
