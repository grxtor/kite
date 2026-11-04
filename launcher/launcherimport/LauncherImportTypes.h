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

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>

/*
 * What another launcher has on this computer, as read from its own files.
 * Everything here comes from files Kite does not own, so it is data to check,
 * never a path to trust: the scanners keep only paths they found by listing a
 * folder or that stay inside the folder they read.
 */
namespace LauncherImport
{
	enum class Source {
		Prism,
		PolyMC,
		MultiMC,
		MeshMC,
		ModrinthApp,
		CurseForge,
		Official,
		ATLauncher,
		Technic,
		FTBApp
	};

	/* "prism", "modrinth", ...: stable, for settings and QML. */
	QString sourceKey(Source source);
	/* "Prism Launcher", "Modrinth App", ...: brand names, not translated. */
	QString sourceName(Source source);
	/* Whether Kite can read this launcher's instances, or only notices it. */
	bool sourceSupported(Source source);

	/* How an instance comes over. Copy takes the game files into Kite's own
	 * folder; Link points Kite's instance at the source's game folder, so both
	 * launchers then write to the same files. */
	enum class Mode {
		Copy,
		Link
	};

	/* Where one mod (or resource or shader pack) came from, per the source
	 * launcher's manifest, so Kite's update checks know it. @p relativePath is
	 * relative to the game folder ("mods/sodium.jar") and already checked to
	 * stay inside it. */
	struct ModOrigin {
		QString relativePath;
		QString platform; /* "modrinth" | "curseforge" */
		QString projectId;
		QString versionId;
		QString versionNumber;
		QString name;
		QString slug;
		QString downloadUrl;
	};

	struct FoundInstance {
		Source source = Source::Prism;
		/* Unique among everything one scan finds: source key plus the
		 * instance's folder. Also recorded in the imported instance so a later
		 * scan can tell it has been brought over. */
		QString key;
		QString name;
		QString minecraftVersion;
		/* "", "fabric", "quilt", "forge" or "neoforge". */
		QString loader;
		QString loaderVersion;
		/* The folder with the game's files (saves, mods, options.txt). */
		QString gameDir;
		QDateTime lastPlayed;
		/* Bytes a copy takes; -1 until counted. */
		qint64 copyBytes = -1;
		/* What the manifest says about the mods; empty when the folder
		 * carries its own index (Prism) or the launcher records none. */
		QList<ModOrigin> mods;
		/* A Modrinth App database whose cache maps file hashes to projects;
		 * read at import time, since it needs the files hashed. */
		QString modrinthDatabase;
		/* Prism's icon key, when it names a built-in icon. */
		QString iconKey;
		/* Why Link is not offered; empty when it is. */
		QString linkBlocker;
		/* Why the instance cannot be imported at all; empty when it can. */
		QString problem;
		/* Parts Kite leaves behind (custom components, jar mods). */
		QStringList leftBehind;

		bool importable() const
		{
			return problem.isEmpty();
		}
		bool linkable() const
		{
			return importable() && linkBlocker.isEmpty();
		}
	};

	struct FoundLauncher {
		Source source = Source::Prism;
		QString dataDir;
		QList<FoundInstance> instances;
		/* Why the launcher's list could not be read; empty when it was. */
		QString problem;
	};
} // namespace LauncherImport
