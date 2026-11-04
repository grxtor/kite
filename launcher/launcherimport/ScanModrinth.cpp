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

/* The Modrinth App keeps each instance ("profile") in profiles/<path>/, which
 * is the game folder itself. Since 0.8 the list is a table in app.db; before,
 * each profile had a profile.json beside its files. Neither is written to:
 * the database is read with SqliteReader, the JSON with a size cap. */

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>

#include "LauncherScan.h"
#include "ScanCommon.h"
#include "SqliteReader.h"

namespace LauncherImport
{
	namespace
	{
		constexpr int kMaxCacheRows = 200000;

		const QString kProfilesFolder = QStringLiteral("profiles");

		/* "vanilla" -> "", the four loaders as they are; false for another. */
		bool mapLoader(const QString& modrinth, QString* loader)
		{
			if (modrinth.isEmpty() || modrinth == QLatin1String("vanilla")) {
				loader->clear();
				return true;
			}
			if (!componentOfLoader(modrinth).isEmpty()) {
				*loader = modrinth;
				return true;
			}
			return false;
		}

		/* Forge and NeoForge builds may carry the game version in front
		 * ("1.20.1-47.2.0"); Kite's lists name them without it. */
		QString loaderVersionFor(const QString& loader, const QString& version,
								 const QString& minecraft)
		{
			const QString prefix = minecraft + QLatin1Char('-');
			if ((loader == QLatin1String("forge") || loader == QLatin1String("neoforge")) &&
				version.startsWith(prefix)) {
				return version.mid(prefix.size());
			}
			return version;
		}

		void finish(FoundInstance* instance, const QString& loader)
		{
			if (!mapLoader(loader, &instance->loader)) {
				instance->problem = QCoreApplication::translate(
					"LauncherImport", "It uses a mod loader Kite doesn't support.");
			} else if (instance->minecraftVersion.isEmpty()) {
				instance->problem = ScanCommon::unknownVersionProblem();
			}
			instance->loaderVersion = loaderVersionFor(
				instance->loader, instance->loaderVersion, instance->minecraftVersion);
			if (instance->importable()) {
				instance->linkBlocker = ScanCommon::linkBlockerFor(instance->gameDir);
			}
		}

		/* The profile's folder, or "" when @p path is not one plain name
		 * (a database naming "../x" must not lead out of profiles/). */
		QString profileDir(const QString& dataDir, const QString& path)
		{
			if (!isSafeName(path)) {
				return {};
			}
			const QString root = QDir(dataDir).filePath(kProfilesFolder);
			const QString dir = QDir(root).filePath(path);
			return isUsableDir(root, dir) ? dir : QString();
		}

		FoundLauncher scanDatabase(const QString& dataDir, const QString& dbPath)
		{
			FoundLauncher launcher;
			launcher.source = Source::ModrinthApp;
			launcher.dataDir = dataDir;
			SqliteReader db;
			const auto unreadable = QCoreApplication::translate(
				"LauncherImport", "Kite can't read the Modrinth App's list of instances.");
			if (!db.open(dbPath)) {
				launcher.problem = unreadable;
				return launcher;
			}
			const bool ok = db.readTable(QStringLiteral("profiles"), [&](const QVariantMap& row) {
				const QString dir = profileDir(dataDir, row.value(QStringLiteral("path")).toString());
				if (dir.isEmpty()) {
					return true;
				}
				FoundInstance instance;
				instance.source = Source::ModrinthApp;
				instance.key = ScanCommon::instanceKey(Source::ModrinthApp, ScanCommon::canonical(dir));
				instance.gameDir = dir;
				instance.name = row.value(QStringLiteral("name")).toString().trimmed();
				if (instance.name.isEmpty()) {
					instance.name = QFileInfo(dir).fileName();
				}
				instance.minecraftVersion = row.value(QStringLiteral("game_version")).toString();
				instance.loaderVersion = row.value(QStringLiteral("mod_loader_version")).toString();
				instance.lastPlayed = parseTime(row.value(QStringLiteral("last_played")));
				instance.modrinthDatabase = dbPath;
				/* Cached under the app's own folder (caches/icons). */
				instance.iconFile = usableIconFile(dataDir, row.value(QStringLiteral("icon_path")).toString());
				finish(&instance, row.value(QStringLiteral("mod_loader")).toString());
				/* Files are still arriving while a pack installs. */
				if (instance.importable() &&
					row.value(QStringLiteral("install_stage")).toString() ==
						QLatin1String("pack_installing")) {
					instance.problem = QCoreApplication::translate(
						"LauncherImport", "It is still being installed in the Modrinth App.");
				}
				launcher.instances.append(instance);
				return launcher.instances.size() < kMaxInstancesPerLauncher;
			});
			if (!ok && launcher.instances.isEmpty()) {
				launcher.problem = unreadable;
			}
			return launcher;
		}

		/* profile.json's "projects": path -> where the file came from. */
		QList<ModOrigin> legacyMods(const QJsonObject& projects)
		{
			QList<ModOrigin> mods;
			for (auto it = projects.begin(); it != projects.end(); ++it) {
				const QString relative = QString(it.key()).replace('\\', '/');
				/* Checked against a made-up root: only the shape matters. */
				if (joinSafe(QStringLiteral("/r"), relative).isEmpty()) {
					continue;
				}
				const QJsonObject metadata = it.value().toObject().value(QStringLiteral("metadata")).toObject();
				if (metadata.value(QStringLiteral("type")).toString() != QLatin1String("modrinth")) {
					continue;
				}
				const QJsonObject project = metadata.value(QStringLiteral("project")).toObject();
				const QJsonObject version = metadata.value(QStringLiteral("version")).toObject();
				ModOrigin origin;
				origin.relativePath = relative;
				origin.platform = QStringLiteral("modrinth");
				origin.projectId = project.value(QStringLiteral("id")).toString();
				origin.slug = project.value(QStringLiteral("slug")).toString();
				origin.name = project.value(QStringLiteral("title")).toString();
				origin.versionId = version.value(QStringLiteral("id")).toString();
				origin.versionNumber = version.value(QStringLiteral("version_number")).toString();
				if (!origin.projectId.isEmpty()) {
					mods.append(origin);
				}
			}
			return mods;
		}

		FoundLauncher scanLegacy(const QString& dataDir)
		{
			FoundLauncher launcher;
			launcher.source = Source::ModrinthApp;
			launcher.dataDir = dataDir;
			const QString root = QDir(dataDir).filePath(kProfilesFolder);
			const QFileInfoList entries =
				QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
			for (const QFileInfo& entry : entries) {
				if (launcher.instances.size() >= kMaxInstancesPerLauncher) {
					break;
				}
				const QString dir = profileDir(dataDir, entry.fileName());
				const QString manifest = QDir(dir).filePath(QStringLiteral("profile.json"));
				if (dir.isEmpty() || !QFileInfo::exists(manifest)) {
					continue;
				}
				FoundInstance instance;
				instance.source = Source::ModrinthApp;
				instance.key = ScanCommon::instanceKey(Source::ModrinthApp, ScanCommon::canonical(dir));
				instance.gameDir = dir;
				instance.name = entry.fileName();
				QJsonObject profile;
				QString error;
				if (!readJsonObject(manifest, kMaxManifestBytes, &profile, &error)) {
					instance.problem = error;
					launcher.instances.append(instance);
					continue;
				}
				const QJsonObject metadata = profile.value(QStringLiteral("metadata")).toObject();
				const QString name = metadata.value(QStringLiteral("name")).toString().trimmed();
				if (!name.isEmpty()) {
					instance.name = name;
				}
				instance.minecraftVersion = metadata.value(QStringLiteral("game_version")).toString();
				instance.loaderVersion = metadata.value(QStringLiteral("loader_version"))
											 .toObject().value(QStringLiteral("id")).toString();
				instance.lastPlayed = parseTime(metadata.value(QStringLiteral("last_played")).toVariant());
				instance.mods = legacyMods(profile.value(QStringLiteral("projects")).toObject());
				instance.iconFile = usableIconFile(dataDir, metadata.value(QStringLiteral("icon")).toString());
				finish(&instance, metadata.value(QStringLiteral("loader")).toString());
				launcher.instances.append(instance);
			}
			return launcher;
		}

		/* Adds what a cached version says about its files: each file's SHA-1
		 * names the project and version it belongs to. */
		void addVersion(const QJsonObject& version, QHash<QString, ModOrigin>* known)
		{
			const QString projectId = version.value(QStringLiteral("project_id")).toString();
			const QString versionId = version.value(QStringLiteral("id")).toString();
			if (projectId.isEmpty() || versionId.isEmpty()) {
				return;
			}
			for (const QJsonValue& value : version.value(QStringLiteral("files")).toArray()) {
				const QJsonObject file = value.toObject();
				const QString sha1 = file.value(QStringLiteral("hashes")).toObject()
										 .value(QStringLiteral("sha1")).toString().toLower();
				if (sha1.isEmpty()) {
					continue;
				}
				ModOrigin origin;
				origin.platform = QStringLiteral("modrinth");
				origin.projectId = projectId;
				origin.versionId = versionId;
				origin.versionNumber = version.value(QStringLiteral("version_number")).toString();
				origin.name = version.value(QStringLiteral("name")).toString();
				origin.downloadUrl = file.value(QStringLiteral("url")).toString();
				known->insert(sha1, origin);
			}
		}
	} // namespace

	FoundLauncher scanModrinthApp(const QString& dataDir)
	{
		const QString dbPath = QDir(dataDir).filePath(QStringLiteral("app.db"));
		FoundLauncher launcher = QFileInfo(dbPath).isFile() ? scanDatabase(dataDir, dbPath)
															: scanLegacy(dataDir);
		ScanCommon::checkLoaders(&launcher);
		return launcher;
	}

	QHash<QString, ModOrigin> readModrinthCache(const QString& databasePath)
	{
		QHash<QString, ModOrigin> known;
		SqliteReader db;
		if (!db.open(databasePath)) {
			return known;
		}
		int rows = 0;
		db.readTable(QStringLiteral("cache"), [&](const QVariantMap& row) {
			const QString type = row.value(QStringLiteral("data_type")).toString();
			const QVariant data = row.value(QStringLiteral("data"));
			const QByteArray json = data.typeId() == QMetaType::QByteArray
										? data.toByteArray()
										: data.toString().toUtf8();
			const QJsonObject object = QJsonDocument::fromJson(json).object();
			if (type == QLatin1String("version")) {
				addVersion(object, &known);
			} else if (type == QLatin1String("file")) {
				/* hash -> project and version, without the version's files. */
				const QString sha1 = object.value(QStringLiteral("hash")).toString().toLower();
				ModOrigin origin;
				origin.platform = QStringLiteral("modrinth");
				origin.projectId = object.value(QStringLiteral("project_id")).toString();
				origin.versionId = object.value(QStringLiteral("version_id")).toString();
				if (!sha1.isEmpty() && !origin.projectId.isEmpty() && !known.contains(sha1)) {
					known.insert(sha1, origin);
				}
			}
			return ++rows < kMaxCacheRows;
		});
		return known;
	}
} // namespace LauncherImport
