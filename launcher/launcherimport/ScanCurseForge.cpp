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

/* The CurseForge app keeps each instance in Instances/<name>/, the game folder
 * itself, with a minecraftinstance.json that names the game version, the
 * loader and every installed addon with its CurseForge project and file ids.
 * The JSON grows with the pack (it carries each addon's details), hence the
 * larger cap. */

#include <QDir>
#include <QJsonArray>

#include "LauncherScan.h"
#include "ScanCommon.h"

namespace LauncherImport
{
	namespace
	{
		constexpr int kMaxAddons = 20000;

		/* CurseForge's ModLoaderType numbers. */
		constexpr int kForgeType = 1;
		constexpr int kFabricType = 4;
		constexpr int kQuiltType = 5;
		constexpr int kNeoForgeType = 6;

		const QString kManifest = QStringLiteral("minecraftinstance.json");

		/* Where an addon's file can be; the manifest does not say. */
		QStringList addonFolders()
		{
			return {QStringLiteral("mods"), QStringLiteral("resourcepacks"),
					QStringLiteral("shaderpacks")};
		}

		/* "forge-47.2.0" -> forge 47.2.0; "fabric-0.15.7-1.20.1" -> fabric
		 * 0.15.7. Empty loader for one Kite does not know. */
		void readLoader(const QJsonObject& base, const QString& minecraft,
						QString* loader, QString* version)
		{
			const QString name = base.value(QStringLiteral("name")).toString();
			switch (base.value(QStringLiteral("type")).toInt()) {
			case kForgeType: *loader = QStringLiteral("forge"); break;
			case kFabricType: *loader = QStringLiteral("fabric"); break;
			case kQuiltType: *loader = QStringLiteral("quilt"); break;
			case kNeoForgeType: *loader = QStringLiteral("neoforge"); break;
			default: loader->clear(); return;
			}
			*version = base.value(QStringLiteral("forgeVersion")).toString();
			if (version->isEmpty()) {
				const QString prefix = *loader + QLatin1Char('-');
				*version = name.startsWith(prefix) ? name.mid(prefix.size()) : QString();
			}
			const QString suffix = QLatin1Char('-') + minecraft;
			if (!minecraft.isEmpty() && version->endsWith(suffix)) {
				version->chop(suffix.size());
			}
			const QString mcPrefix = minecraft + QLatin1Char('-');
			if (!minecraft.isEmpty() && version->startsWith(mcPrefix)) {
				*version = version->mid(mcPrefix.size());
			}
		}

		QList<ModOrigin> readAddons(const QJsonArray& addons, const QString& gameDir)
		{
			QList<ModOrigin> mods;
			const QDir game(gameDir);
			for (const QJsonValue& value : addons) {
				if (mods.size() >= kMaxAddons) {
					break;
				}
				const QJsonObject addon = value.toObject();
				const QJsonObject file = addon.value(QStringLiteral("installedFile")).toObject();
				const QString fileName = file.value(QStringLiteral("fileName")).toString();
				const qint64 projectId = addon.value(QStringLiteral("addonID")).toVariant().toLongLong();
				const qint64 fileId = file.value(QStringLiteral("id")).toVariant().toLongLong();
				if (!isSafeName(fileName) || projectId <= 0 || fileId <= 0) {
					continue;
				}
				for (const QString& folder : addonFolders()) {
					const QString relative = folder + QLatin1Char('/') + fileName;
					if (!game.exists(relative) &&
						!game.exists(relative + QStringLiteral(".disabled"))) {
						continue;
					}
					ModOrigin origin;
					origin.relativePath = relative;
					origin.platform = QStringLiteral("curseforge");
					origin.projectId = QString::number(projectId);
					origin.versionId = QString::number(fileId);
					origin.name = addon.value(QStringLiteral("name")).toString();
					origin.downloadUrl = file.value(QStringLiteral("downloadUrl")).toString();
					mods.append(origin);
					break;
				}
			}
			return mods;
		}

		FoundInstance readInstance(const QString& dir)
		{
			FoundInstance instance;
			instance.source = Source::CurseForge;
			instance.key = ScanCommon::instanceKey(Source::CurseForge, ScanCommon::canonical(dir));
			instance.gameDir = dir;
			instance.name = QFileInfo(dir).fileName();
			QJsonObject manifest;
			QString error;
			if (!readJsonObject(QDir(dir).filePath(kManifest), kMaxManifestBytes, &manifest, &error)) {
				instance.problem = error;
				return instance;
			}
			const QString name = manifest.value(QStringLiteral("name")).toString().trimmed();
			if (!name.isEmpty()) {
				instance.name = name;
			}
			const QJsonObject base = manifest.value(QStringLiteral("baseModLoader")).toObject();
			instance.minecraftVersion = manifest.value(QStringLiteral("gameVersion")).toString();
			if (instance.minecraftVersion.isEmpty()) {
				instance.minecraftVersion = base.value(QStringLiteral("minecraftVersion")).toString();
			}
			instance.lastPlayed = parseTime(manifest.value(QStringLiteral("lastPlayed")).toVariant());
			if (!base.isEmpty()) {
				readLoader(base, instance.minecraftVersion, &instance.loader, &instance.loaderVersion);
				if (instance.loader.isEmpty()) {
					instance.problem = QCoreApplication::translate(
						"LauncherImport", "It uses a mod loader Kite doesn't support.");
					return instance;
				}
			}
			if (instance.minecraftVersion.isEmpty()) {
				instance.problem = ScanCommon::unknownVersionProblem();
				return instance;
			}
			instance.mods = readAddons(manifest.value(QStringLiteral("installedAddons")).toArray(), dir);
			/* A picture the player set, if it is kept with CurseForge's
			 * files. A modpack's own thumbnail is a web address, which is not
			 * fetched: the list stays on this computer. */
			instance.iconFile = usableIconFile(QFileInfo(dir).absolutePath() + QStringLiteral("/.."),
											   manifest.value(QStringLiteral("profileImagePath")).toString());
			instance.linkBlocker = ScanCommon::linkBlockerFor(dir);
			return instance;
		}
	} // namespace

	FoundLauncher scanCurseForge(const QString& instancesDir)
	{
		FoundLauncher launcher;
		launcher.source = Source::CurseForge;
		launcher.dataDir = instancesDir;
		const QFileInfoList entries =
			QDir(instancesDir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
		for (const QFileInfo& entry : entries) {
			if (launcher.instances.size() >= kMaxInstancesPerLauncher) {
				break;
			}
			if (!isUsableDir(instancesDir, entry.filePath()) ||
				!QFileInfo::exists(QDir(entry.filePath()).filePath(kManifest))) {
				continue;
			}
			launcher.instances.append(readInstance(entry.filePath()));
		}
		ScanCommon::checkLoaders(&launcher);
		return launcher;
	}
} // namespace LauncherImport
