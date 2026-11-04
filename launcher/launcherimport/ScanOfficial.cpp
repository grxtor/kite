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

/* The official launcher's "installations" are profiles in
 * launcher_profiles.json: a version to run and, optionally, a game folder
 * (the data folder itself by default). A loader installer (Fabric, Forge, ...)
 * adds a version of its own under versions/<id>/<id>.json that inherits from
 * the game version, which is where the game version and loader are read. */

#include <QDir>
#include <QJsonArray>

#include "LauncherScan.h"
#include "ScanCommon.h"

namespace LauncherImport
{
	namespace
	{
		/* Version folders looked at to find the newest installed release. */
		constexpr int kMaxVersionFolders = 2000;

		struct Version {
			QString minecraft;
			QString loader;
			QString loaderVersion;
		};

		/* What a version id says on its own: the installers' naming
		 * ("fabric-loader-0.15.11-1.20.1", "1.20.1-forge-47.2.0"). */
		Version fromId(const QString& id)
		{
			Version version;
			const QStringList parts = id.split('-');
			const auto loaderFirst = [&](const QString& loader) {
				/* <loader>-loader-<loader version>-<game version> */
				if (parts.size() >= 4) {
					version.loader = loader;
					version.loaderVersion = parts.at(2);
					version.minecraft = parts.mid(3).join('-');
				}
			};
			if (id.startsWith(QLatin1String("fabric-loader-"))) {
				loaderFirst(QStringLiteral("fabric"));
			} else if (id.startsWith(QLatin1String("quilt-loader-"))) {
				loaderFirst(QStringLiteral("quilt"));
			} else if (id.startsWith(QLatin1String("neoforge-"))) {
				version.loader = QStringLiteral("neoforge");
				version.loaderVersion = id.mid(QStringLiteral("neoforge-").size());
			} else if (parts.size() >= 3 &&
					   parts.at(1).startsWith(QLatin1String("forge"), Qt::CaseInsensitive)) {
				/* 1.20.1-forge-47.2.0, or the older 1.12.2-forge1.12.2-14.23.5 */
				version.loader = QStringLiteral("forge");
				version.minecraft = parts.at(0);
				version.loaderVersion = parts.at(2);
			} else {
				version.minecraft = id;
			}
			return version;
		}

		/* A library's version, from its "group:name:version" coordinate. */
		QString libraryVersion(const QJsonArray& libraries, const QString& prefix)
		{
			for (const QJsonValue& value : libraries) {
				const QString name = value.toObject().value(QStringLiteral("name")).toString();
				if (name.startsWith(prefix)) {
					return name.mid(prefix.size()).section(':', 0, 0);
				}
			}
			return {};
		}

		/* The value after @p flag in the version's game arguments. */
		QString argumentAfter(const QJsonObject& json, const QString& flag)
		{
			const QJsonArray game = json.value(QStringLiteral("arguments")).toObject()
										.value(QStringLiteral("game")).toArray();
			for (int i = 0; i + 1 < game.size(); ++i) {
				if (game.at(i).toString() == flag) {
					return game.at(i + 1).toString();
				}
			}
			return {};
		}

		/* The id's reading, corrected by its version file when there is one:
		 * the file says which game version it builds on, and its libraries and
		 * arguments name the loader exactly. */
		Version readVersion(const QString& dataDir, const QString& id)
		{
			Version version = fromId(id);
			if (!isSafeName(id)) {
				return version;
			}
			QJsonObject json;
			QString error;
			const QString path = QDir(dataDir).filePath(
				QStringLiteral("versions/%1/%1.json").arg(id));
			if (!readJsonObject(path, kMaxSmallJsonBytes, &json, &error)) {
				return version;
			}
			const QString inherits = json.value(QStringLiteral("inheritsFrom")).toString();
			if (!inherits.isEmpty()) {
				version.minecraft = inherits;
			}
			const QJsonArray libraries = json.value(QStringLiteral("libraries")).toArray();
			const QString fabric = libraryVersion(libraries, QStringLiteral("net.fabricmc:fabric-loader:"));
			const QString quilt = libraryVersion(libraries, QStringLiteral("org.quiltmc:quilt-loader:"));
			const QString neoforge = argumentAfter(json, QStringLiteral("--fml.neoForgeVersion"));
			const QString forge = argumentAfter(json, QStringLiteral("--fml.forgeVersion"));
			if (!fabric.isEmpty()) {
				version.loader = QStringLiteral("fabric");
				version.loaderVersion = fabric;
			} else if (!quilt.isEmpty()) {
				version.loader = QStringLiteral("quilt");
				version.loaderVersion = quilt;
			} else if (!neoforge.isEmpty()) {
				version.loader = QStringLiteral("neoforge");
				version.loaderVersion = neoforge;
			} else if (!forge.isEmpty()) {
				version.loader = QStringLiteral("forge");
				version.loaderVersion = forge;
			}
			return version;
		}

		/* The newest installed game version of @p type ("release", or
		 * "snapshot" for either), for the "Latest release" profiles, which
		 * name no version. */
		QString newestInstalled(const QString& dataDir, bool snapshots)
		{
			QString newest;
			QDateTime newestTime;
			const QDir versions(QDir(dataDir).filePath(QStringLiteral("versions")));
			const QStringList ids = versions.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
			for (int i = 0; i < ids.size() && i < kMaxVersionFolders; ++i) {
				const QString& id = ids.at(i);
				QJsonObject json;
				QString error;
				if (!isSafeName(id) ||
					!readJsonObject(versions.filePath(QStringLiteral("%1/%1.json").arg(id)),
									kMaxSmallJsonBytes, &json, &error) ||
					json.contains(QStringLiteral("inheritsFrom"))) {
					continue;
				}
				const QString type = json.value(QStringLiteral("type")).toString();
				if (type != QLatin1String("release") &&
					!(snapshots && type == QLatin1String("snapshot"))) {
					continue;
				}
				const QDateTime time = parseTime(json.value(QStringLiteral("releaseTime")).toVariant());
				if (time.isValid() && (!newestTime.isValid() || time > newestTime)) {
					newest = id;
					newestTime = time;
				}
			}
			return newest;
		}

		FoundInstance readProfile(const QString& dataDir, const QString& id,
								  const QJsonObject& profile)
		{
			FoundInstance instance;
			instance.source = Source::Official;
			instance.key = ScanCommon::instanceKey(
				Source::Official, ScanCommon::canonical(dataDir) + QLatin1Char('#') + id);
			const QString type = profile.value(QStringLiteral("type")).toString();
			instance.name = profile.value(QStringLiteral("name")).toString().trimmed();
			if (instance.name.isEmpty()) {
				instance.name = type == QLatin1String("latest-snapshot")
									? QCoreApplication::translate("LauncherImport", "Latest snapshot")
									: QCoreApplication::translate("LauncherImport", "Latest release");
			}
			instance.lastPlayed = parseTime(profile.value(QStringLiteral("lastUsed")).toVariant());
			/* A block name ("Grass", "Crafting_Table"). A picture of the
			 * player's own comes as a "data:" address, which is no name and is
			 * left out. */
			const QString icon = profile.value(QStringLiteral("icon")).toString().toLower();
			if (isSafeName(icon)) {
				instance.iconKey = icon;
			}
			/* A game folder must be given in full; the launcher itself
			 * refuses a relative one. */
			const QString gameDir = profile.value(QStringLiteral("gameDir")).toString();
			instance.gameDir = gameDir.isEmpty() ? dataDir : QDir::cleanPath(gameDir);
			if (!QDir::isAbsolutePath(instance.gameDir) || !QFileInfo(instance.gameDir).isDir()) {
				instance.problem = ScanCommon::missingFolderProblem();
				return instance;
			}
			QString versionId = profile.value(QStringLiteral("lastVersionId")).toString();
			if (type == QLatin1String("latest-release") || type == QLatin1String("latest-snapshot")) {
				versionId = newestInstalled(dataDir, type == QLatin1String("latest-snapshot"));
			}
			/* A version id is a folder name under versions/; anything else
			 * ("../x") is not one the launcher could have made. */
			if (versionId.isEmpty() || !isSafeName(versionId)) {
				instance.problem = ScanCommon::unknownVersionProblem();
				return instance;
			}
			const Version version = readVersion(dataDir, versionId);
			instance.minecraftVersion = version.minecraft;
			instance.loader = version.loader;
			instance.loaderVersion = version.loaderVersion;
			if (instance.minecraftVersion.isEmpty()) {
				instance.problem = ScanCommon::unknownVersionProblem();
				return instance;
			}
			instance.linkBlocker = ScanCommon::linkBlockerFor(instance.gameDir);
			return instance;
		}
	} // namespace

	FoundLauncher scanOfficial(const QString& dataDir)
	{
		FoundLauncher launcher;
		launcher.source = Source::Official;
		launcher.dataDir = dataDir;
		QJsonObject root;
		QString error;
		if (!readJsonObject(QDir(dataDir).filePath(QStringLiteral("launcher_profiles.json")),
							kMaxSmallJsonBytes, &root, &error)) {
			launcher.problem = error;
			return launcher;
		}
		const QJsonObject profiles = root.value(QStringLiteral("profiles")).toObject();
		for (auto it = profiles.begin(); it != profiles.end(); ++it) {
			if (launcher.instances.size() >= kMaxInstancesPerLauncher) {
				break;
			}
			launcher.instances.append(readProfile(dataDir, it.key(), it.value().toObject()));
		}
		ScanCommon::checkLoaders(&launcher);
		return launcher;
	}
} // namespace LauncherImport
