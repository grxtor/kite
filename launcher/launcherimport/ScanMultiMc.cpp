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

/* Prism Launcher and its relatives keep instances the way Kite does (Kite
 * descends from MultiMC): instance.cfg for the name and play times,
 * mmc-pack.json for the game version and loader, and the game files in
 * .minecraft or minecraft beside them. Mods carry their own index
 * (mods/.index/<slug>.pw.toml), which a copy brings along as it is. */

#include <QDir>
#include <QJsonArray>

#include "LauncherScan.h"
#include "ScanCommon.h"
#include "settings/INIFile.h"

namespace LauncherImport
{
	namespace
	{
		/* Components the game and loader pull in by themselves. */
		bool isDependency(const QString& uid)
		{
			static const QStringList dependencies = {
				QStringLiteral("org.lwjgl"), QStringLiteral("org.lwjgl3"),
				QStringLiteral("net.fabricmc.intermediary"),
				QStringLiteral("org.quiltmc.hashed")};
			return dependencies.contains(uid);
		}

		/* The launcher's main config file names its instance folder. */
		QString configFileName(Source source)
		{
			switch (source) {
			case Source::Prism: return QStringLiteral("prismlauncher.cfg");
			case Source::PolyMC: return QStringLiteral("polymc.cfg");
			case Source::MultiMC: return QStringLiteral("multimc.cfg");
			case Source::MeshMC: return QStringLiteral("meshmc.cfg");
			default: return {};
			}
		}

		/* Prism writes QSettings-style values: "a, b" in quotes, with \" and
		 * \\ inside. MultiMC's INIFile leaves the quotes on. */
		QString iniValue(const INIFile& ini, const QString& key)
		{
			QString value = ini.get(key, QString()).toString();
			if (value.size() >= 2 && value.startsWith('"') && value.endsWith('"')) {
				value = value.mid(1, value.size() - 2);
				value.replace(QStringLiteral("\\\""), QStringLiteral("\""));
				value.replace(QStringLiteral("\\\\"), QStringLiteral("\\"));
			}
			return value;
		}

		bool readIni(const QString& path, INIFile* ini)
		{
			QByteArray bytes;
			return readCapped(path, kMaxConfigBytes, &bytes) && ini->loadFile(bytes);
		}

		QString instancesDir(Source source, const QString& dataDir)
		{
			const QString fallback = QDir(dataDir).filePath(QStringLiteral("instances"));
			INIFile config;
			if (!readIni(QDir(dataDir).filePath(configFileName(source)), &config)) {
				return fallback;
			}
			const QString configured = iniValue(config, QStringLiteral("InstanceDir"));
			if (configured.isEmpty()) {
				return fallback;
			}
			/* Read from wherever the launcher keeps them, other disks too;
			 * reading is all that happens there. */
			const QString dir = QDir::isAbsolutePath(configured)
									? QDir::cleanPath(configured)
									: QDir::cleanPath(QDir(dataDir).filePath(configured));
			return QFileInfo(dir).isDir() ? dir : fallback;
		}

		/* As MinecraftInstance::gameRoot(): "minecraft" when only it exists. */
		QString gameDirOf(const QString& instanceDir)
		{
			const QDir dir(instanceDir);
			const bool plain = dir.exists(QStringLiteral("minecraft"));
			const bool dotted = dir.exists(QStringLiteral(".minecraft"));
			return dir.filePath(plain && !dotted ? QStringLiteral("minecraft")
												 : QStringLiteral(".minecraft"));
		}

		void readComponents(const QString& instanceDir, FoundInstance* instance)
		{
			QJsonObject pack;
			QString error;
			if (!readJsonObject(QDir(instanceDir).filePath(QStringLiteral("mmc-pack.json")),
								kMaxSmallJsonBytes, &pack, &error)) {
				instance->problem = error;
				return;
			}
			const QJsonArray components = pack.value(QStringLiteral("components")).toArray();
			bool custom = false;
			for (const QJsonValue& value : components) {
				const QJsonObject component = value.toObject();
				const QString uid = component.value(QStringLiteral("uid")).toString();
				const QString version =
					component.value(QStringLiteral("version")).toString();
				if (uid.isEmpty() || component.value(QStringLiteral("disabled")).toBool()) {
					continue;
				}
				if (uid == QLatin1String("net.minecraft")) {
					instance->minecraftVersion = version;
				} else if (!loaderOfComponent(uid).isEmpty()) {
					instance->loader = loaderOfComponent(uid);
					instance->loaderVersion = version;
				} else if (!isDependency(uid)) {
					custom = true;
				}
			}
			if (custom) {
				instance->leftBehind.append(QCoreApplication::translate(
					"LauncherImport", "custom components"));
			}
			const QDir jarMods(QDir(instanceDir).filePath(QStringLiteral("jarmods")));
			if (!jarMods.isEmpty(QDir::Files)) {
				instance->leftBehind.append(
					QCoreApplication::translate("LauncherImport", "jar mods"));
			}
			if (instance->minecraftVersion.isEmpty()) {
				instance->problem = ScanCommon::unknownVersionProblem();
			}
		}

		FoundInstance readInstance(Source source, const QString& instanceDir)
		{
			FoundInstance instance;
			instance.source = source;
			instance.key = ScanCommon::instanceKey(source, ScanCommon::canonical(instanceDir));
			instance.name = QFileInfo(instanceDir).fileName();
			instance.gameDir = gameDirOf(instanceDir);

			INIFile cfg;
			if (!readIni(QDir(instanceDir).filePath(QStringLiteral("instance.cfg")), &cfg)) {
				instance.problem = QCoreApplication::translate(
					"LauncherImport", "Its details file is missing.");
				return instance;
			}
			const QString name = iniValue(cfg, QStringLiteral("name"));
			if (!name.trimmed().isEmpty()) {
				instance.name = name.trimmed();
			}
			instance.iconKey = iniValue(cfg, QStringLiteral("iconKey"));
			instance.lastPlayed = parseTime(cfg.get(QStringLiteral("lastLaunchTime"), 0));
			const QString type = iniValue(cfg, QStringLiteral("InstanceType"));
			if (!type.isEmpty() && type != QLatin1String("OneSix")) {
				instance.problem = QCoreApplication::translate(
					"LauncherImport", "It is in an old format Kite can't read.");
				return instance;
			}
			readComponents(instanceDir, &instance);
			if (!instance.importable()) {
				return instance;
			}
			/* A game folder that is a link out of the instance would make a
			 * copy read, and Link write, somewhere this launcher does not
			 * keep. */
			if (isLink(QFileInfo(instance.gameDir)) &&
				!resolvesInside(instanceDir, instance.gameDir)) {
				instance.problem = QCoreApplication::translate(
					"LauncherImport", "Its game folder is a link to another place.");
			} else if (!QFileInfo(instance.gameDir).isDir()) {
				/* A new instance that never ran has no game folder yet; there
				 * is nothing to copy, but it imports as an empty one. */
				instance.linkBlocker = ScanCommon::missingFolderProblem();
			} else {
				instance.linkBlocker = ScanCommon::linkBlockerFor(instance.gameDir);
			}
			return instance;
		}
	} // namespace

	FoundLauncher scanMultiMc(Source source, const QString& dataDir)
	{
		FoundLauncher launcher;
		launcher.source = source;
		launcher.dataDir = dataDir;
		const QString root = instancesDir(source, dataDir);
		const QFileInfoList entries =
			QDir(root).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
		for (const QFileInfo& entry : entries) {
			if (launcher.instances.size() >= kMaxInstancesPerLauncher) {
				break;
			}
			/* The launcher's own working folders (_LAUNCHER_TEMP, .tmp). */
			const QString name = entry.fileName();
			if (name.startsWith('.') || name.startsWith('_')) {
				continue;
			}
			if (!isUsableDir(root, entry.filePath()) ||
				!QFileInfo::exists(QDir(entry.filePath()).filePath(QStringLiteral("instance.cfg")))) {
				continue;
			}
			launcher.instances.append(readInstance(source, entry.filePath()));
		}
		ScanCommon::checkLoaders(&launcher);
		return launcher;
	}
} // namespace LauncherImport
