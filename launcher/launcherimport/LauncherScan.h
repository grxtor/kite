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
#include <QVariant>

#include <atomic>

#include "LauncherImportTypes.h"
#include "LauncherLocations.h"

/*
 * Reading other launchers' instance lists. Each scanner takes the launcher's
 * data folder and reads only: its instance list, each instance's manifest and
 * the names of the files in its game folder. Nothing is opened for writing.
 */
namespace LauncherImport
{
	/* Prism Launcher, PolyMC, MultiMC and MeshMC: instances/<name>/ with an
	 * instance.cfg and an mmc-pack.json, Kite's own format. */
	FoundLauncher scanMultiMc(Source source, const QString& dataDir);

	/* The Modrinth App: its app.db database (0.8 and later) or, in older
	 * installs, profiles/<name>/profile.json. */
	FoundLauncher scanModrinthApp(const QString& dataDir);

	/* The CurseForge app: @p instancesDir/<name>/minecraftinstance.json. */
	FoundLauncher scanCurseForge(const QString& instancesDir);

	/* The official Minecraft Launcher: launcher_profiles.json and the version
	 * files under versions/. */
	FoundLauncher scanOfficial(const QString& dataDir);

	/* Every launcher with data under @p env, supported or not (an
	 * unsupported one comes without instances). Each data folder once, even
	 * when two candidate paths lead to it. */
	QList<FoundLauncher> scanAll(const SearchEnvironment& env,
								 const std::atomic_bool* cancel = nullptr);

	/* Fills every importable instance's copyBytes. A folder several
	 * instances share (the official launcher's) is counted once. */
	void countCopySizes(QList<FoundLauncher>* launchers,
						const std::atomic_bool* cancel = nullptr);

	/* The Modrinth App's record of which file is which project: SHA-1 ->
	 * origin (relativePath empty), from the versions and files it cached.
	 * Empty when the database cannot be read. */
	QHash<QString, ModOrigin> readModrinthCache(const QString& databasePath);

	/* "net.fabricmc.fabric-loader" -> "fabric" and back; "" for anything that
	 * is not one of the four loaders. */
	QString loaderOfComponent(const QString& uid);
	QString componentOfLoader(const QString& loader);

	/* A launcher's idea of a time: seconds or milliseconds since 1970, or an
	 * ISO 8601 text. Invalid for "never" (0, year 1). */
	QDateTime parseTime(const QVariant& value);
} // namespace LauncherImport
