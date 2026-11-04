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

#include "ImportBuild.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include <map>
#include <memory>

#include "ImportCopy.h"
#include "LauncherScan.h"
#include "SafeFiles.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "settings/INIFile.h"

namespace LauncherImport
{
	namespace
	{
		/* Kite's name for the game folder inside an instance. */
		const QString kGameFolder = QStringLiteral("minecraft");
		const QString kDisabledSuffix = QStringLiteral(".disabled");
		constexpr int kPackFormatVersion = 1;
		constexpr qint64 kHashChunkBytes = 1024 * 1024;

		bool writeConfig(const QString& stagingPath, const BuildRequest& request,
						 const QString& linkedGameDir)
		{
			INIFile ini;
			ini.set(QStringLiteral("InstanceType"), QStringLiteral("OneSix"));
			ini.set(QStringLiteral("name"), request.name);
			ini.set(QStringLiteral("iconKey"),
					request.iconKey.isEmpty() ? QStringLiteral("default") : request.iconKey);
			ini.set(kImportedFromKey, request.instance.key);
			if (!linkedGameDir.isEmpty()) {
				ini.set(kLinkedGameDirKey, linkedGameDir);
			}
			return ini.saveFile(QDir(stagingPath).filePath(QStringLiteral("instance.cfg")));
		}

		/* The game and its loader by id and version; the rest (LWJGL,
		 * intermediary mappings) is resolved from Kite's metadata on the first
		 * launch, as for an instance made in Kite. */
		bool writePack(const QString& stagingPath, const FoundInstance& instance)
		{
			QJsonArray components;
			components.append(QJsonObject{{QStringLiteral("uid"), QStringLiteral("net.minecraft")},
										  {QStringLiteral("version"), instance.minecraftVersion},
										  {QStringLiteral("important"), true}});
			const QString loaderUid = componentOfLoader(instance.loader);
			if (!loaderUid.isEmpty()) {
				components.append(QJsonObject{{QStringLiteral("uid"), loaderUid},
											  {QStringLiteral("version"), instance.loaderVersion}});
			}
			const QJsonObject root{{QStringLiteral("formatVersion"), kPackFormatVersion},
								   {QStringLiteral("components"), components}};
			QSaveFile file(QDir(stagingPath).filePath(QStringLiteral("mmc-pack.json")));
			return file.open(QIODevice::WriteOnly) &&
				   file.write(QJsonDocument(root).toJson()) >= 0 && file.commit();
		}

		QString sha1Of(const QString& path, const std::atomic_bool& cancel)
		{
			QFile file(path);
			if (!file.open(QIODevice::ReadOnly)) {
				return {};
			}
			QCryptographicHash hash(QCryptographicHash::Sha1);
			while (!file.atEnd()) {
				if (cancel.load()) {
					return {};
				}
				hash.addData(file.read(kHashChunkBytes));
			}
			return QString::fromLatin1(hash.result().toHex());
		}

		/* Writes each known mod's origin into the copied folders' indexes,
		 * where Kite's update checks read it. A mod its own index already
		 * describes (a Prism instance's) is left as it is. */
		class OriginWriter
		{
		  public:
			explicit OriginWriter(const QString& gameDir) : m_gameDir(gameDir) {}

			void write(const QString& relative, const ModOrigin& origin,
					   const QString& sha1 = {})
			{
				const QString path = joinSafe(m_gameDir, relative);
				if (path.isEmpty()) {
					return;
				}
				const QFileInfo file(path);
				if (!file.isFile() && !QFileInfo(path + kDisabledSuffix).isFile()) {
					return;
				}
				ModMetadataIndex& index = indexFor(file.absolutePath());
				if (index.contains(file.fileName())) {
					return;
				}
				ModMetadataIndex::Entry entry;
				entry.fileName = file.fileName();
				entry.platform = origin.platform;
				entry.projectId = origin.projectId;
				entry.versionId = origin.versionId;
				entry.versionNumber = origin.versionNumber;
				entry.name = origin.name.isEmpty() ? file.completeBaseName() : origin.name;
				entry.slug = origin.slug;
				entry.downloadUrl = origin.downloadUrl;
				entry.sha1 = sha1;
				entry.installedAt = QDateTime::currentDateTimeUtc();
				if (index.put(entry)) {
					++m_written;
				}
			}

			int written() const
			{
				return m_written;
			}

		  private:
			ModMetadataIndex& indexFor(const QString& folder)
			{
				auto& index = m_indexes[folder];
				if (!index) {
					index = std::make_unique<ModMetadataIndex>(QDir(folder));
					index->load();
				}
				return *index;
			}

			QString m_gameDir;
			std::map<QString, std::unique_ptr<ModMetadataIndex>> m_indexes;
			int m_written = 0;
		};

		/* The Modrinth App records no origin per file, only per hash in its
		 * cache; the copied files are hashed to look theirs up. */
		void writeModrinthOrigins(const QString& gameDir, const QString& database,
								  const std::atomic_bool& cancel, OriginWriter* writer)
		{
			const QHash<QString, ModOrigin> known = readModrinthCache(database);
			if (known.isEmpty()) {
				return;
			}
			for (const QString& folder : {QStringLiteral("mods"), QStringLiteral("resourcepacks"),
										  QStringLiteral("shaderpacks")}) {
				const QFileInfoList files =
					QDir(QDir(gameDir).filePath(folder)).entryInfoList(QDir::Files, QDir::Name);
				for (const QFileInfo& file : files) {
					if (cancel.load()) {
						return;
					}
					const QString sha1 = sha1Of(file.absoluteFilePath(), cancel);
					if (!known.contains(sha1)) {
						continue;
					}
					QString name = file.fileName();
					if (name.endsWith(kDisabledSuffix)) {
						name.chop(kDisabledSuffix.size());
					}
					writer->write(folder + QLatin1Char('/') + name, known.value(sha1), sha1);
				}
			}
		}
	} // namespace

	BuildResult buildInstance(const BuildRequest& request, const QString& stagingPath,
							  const std::atomic_bool& cancel, BuildProgress* progress)
	{
		BuildResult result;
		const FoundInstance& instance = request.instance;
		if (!instance.importable()) {
			result.error = instance.problem;
			return result;
		}
		if (!QDir().mkpath(stagingPath)) {
			result.error = QCoreApplication::translate(
				"LauncherImport", "Could not make the instance folder.");
			return result;
		}

		QString linkedGameDir;
		if (request.mode == Mode::Link) {
			if (!instance.linkable()) {
				result.error = instance.linkBlocker;
				return result;
			}
			linkedGameDir = QFileInfo(instance.gameDir).canonicalFilePath();
			if (progress) {
				progress->total = 0;
			}
		} else {
			const QString gameDir = QDir(stagingPath).filePath(kGameFolder);
			const CopyPlan plan = planCopy(instance.gameDir, &cancel);
			if (plan.cancelled || cancel.load()) {
				result.cancelled = true;
				return result;
			}
			if (plan.tooLarge) {
				result.error = QCoreApplication::translate(
					"LauncherImport", "Its game folder has too many files to copy.");
				return result;
			}
			if (progress) {
				progress->total = plan.bytes;
			}
			std::atomic<qint64> copied{0};
			if (!runCopy(plan, gameDir, cancel, progress ? &progress->done : &copied,
						 &result.error)) {
				result.cancelled = cancel.load();
				return result;
			}
			result.copiedBytes = plan.bytes;
			result.skippedLinks = plan.skippedLinks;

			OriginWriter writer(gameDir);
			for (const ModOrigin& origin : instance.mods) {
				writer.write(origin.relativePath, origin);
			}
			if (!instance.modrinthDatabase.isEmpty()) {
				writeModrinthOrigins(gameDir, instance.modrinthDatabase, cancel, &writer);
			}
			result.mappedMods = writer.written();
			if (cancel.load()) {
				result.cancelled = true;
				return result;
			}
		}

		if (!writePack(stagingPath, instance) || !writeConfig(stagingPath, request, linkedGameDir)) {
			result.error = QCoreApplication::translate(
				"LauncherImport", "Could not write the instance's settings.");
			return result;
		}
		result.ok = true;
		return result;
	}
} // namespace LauncherImport
