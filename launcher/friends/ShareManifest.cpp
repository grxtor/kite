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

#include "ShareManifest.h"

#include <QDir>
#include <QFileInfo>
#include <QHash>
#include <QVariantList>

#include <algorithm>

#include "minecraft/Component.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModMetadataIndex.h"

namespace ShareManifest
{
	namespace
	{
		const QString kDisabledSuffix = QStringLiteral(".disabled");
		const QString kSha1 = QStringLiteral("sha1");

		Skipped skipped(const QString& name, const QString& fileName,
						const QString& kind, Skipped::Reason reason,
						const QString& sha1 = QString())
		{
			return {name, fileName, kind, reason, sha1};
		}

		QString recordedSha1(const ModMetadataIndex::Entry& entry)
		{
			if (!entry.sha1.isEmpty()) {
				return entry.sha1;
			}
			return entry.hashFormat == kSha1 ? entry.hash : QString();
		}

		/* A file (or, for packs, a folder) the game reads from @p folder. */
		QFileInfoList contentOf(const Folder& folder)
		{
			QDir dir(folder.path);
			auto filters = QDir::Files | QDir::NoDotAndDotDot;
			if (folder.kind != ShareCodes::Kind::Mod) {
				filters |= QDir::Dirs;
			}
			QFileInfoList out;
			for (const QFileInfo& info : dir.entryInfoList(filters, QDir::Name)) {
				if (!info.fileName().startsWith(QLatin1Char('.'))) {
					out.append(info);
				}
			}
			return out;
		}

		void readFolder(const Folder& folder, Built& built)
		{
			ModMetadataIndex index{QDir(folder.path)};
			index.load();
			for (const QFileInfo& info : contentOf(folder)) {
				const QString fileName = info.fileName();
				const auto entry =
					index.get(ModMetadataIndex::canonicalFileName(fileName));
				const QString name = !entry.name.isEmpty()
										 ? entry.name
										 : info.completeBaseName();
				if (fileName.endsWith(kDisabledSuffix)) {
					built.skipped.append(skipped(name, fileName, folder.kind,
												 Skipped::Reason::TurnedOff));
					continue;
				}
				const ShareCodes::Entry candidate{folder.kind, entry.platform,
												  entry.projectId,
												  entry.versionId};
				if (!ShareCodes::isValid(candidate)) {
					/* Added by hand, or a sidecar without a usable version id
					 * (an .mrpack's): Modrinth may still know it by its hash. */
					built.skipped.append(skipped(name, fileName, folder.kind,
												 Skipped::Reason::NoSource,
												 recordedSha1(entry)));
					continue;
				}
				built.manifest.content.append(candidate);
				built.included.append({name, fileName, folder.kind, entry.platform});
			}
		}

		QString reasonName(Skipped::Reason reason)
		{
			return reason == Skipped::Reason::TurnedOff
					   ? QStringLiteral("turnedOff")
					   : QStringLiteral("noSource");
		}

		template <typename T> void sortByName(QList<T>& list)
		{
			std::stable_sort(list.begin(), list.end(), [](const T& a, const T& b) {
				return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
			});
		}
	} // namespace

	std::optional<Source> sourceOf(MinecraftInstance& instance)
	{
		auto profile = instance.getPackProfile();
		Source source;
		source.name = instance.name();
		source.minecraftVersion =
			profile ? profile->getComponentVersion("net.minecraft") : QString();
		if (source.minecraftVersion.isEmpty()) {
			return std::nullopt;
		}
		source.loader = QStringLiteral("vanilla");
		for (const ModLoaderInfo& loader : knownModLoaders()) {
			const QString version = profile->getComponentVersion(loader.uid);
			if (version.isEmpty()) {
				continue;
			}
			source.loader = loader.platformId;
			source.loaderVersion = version;
			if (loader.platformId.isEmpty()) {
				source.unsupportedLoader = loader.brandName;
			}
			break;
		}
		source.folders = {{ShareCodes::Kind::Mod, instance.modsRoot()},
						  {ShareCodes::Kind::ResourcePack,
						   instance.resourcePacksDir()},
						  {ShareCodes::Kind::ShaderPack, instance.shaderPacksDir()},
						  {ShareCodes::Kind::DataPack, instance.dataPacksDir()}};
		return source;
	}

	Built build(const Source& source)
	{
		Built built;
		built.manifest.name = source.name;
		built.manifest.minecraftVersion = source.minecraftVersion;
		built.manifest.loader = source.loader;
		built.manifest.loaderVersion = source.loaderVersion;
		for (const Folder& folder : source.folders) {
			/* Vanilla loads no mods; a stray file there is not shared. */
			if (folder.kind == ShareCodes::Kind::Mod &&
				source.loader == QLatin1String("vanilla")) {
				continue;
			}
			readFolder(folder, built);
		}
		sortByName(built.included);
		sortByName(built.skipped);
		return built;
	}

	QStringList hashesToLookUp(const Built& built)
	{
		QStringList hashes;
		for (const Skipped& one : built.skipped) {
			if (one.reason == Skipped::Reason::NoSource && !one.sha1.isEmpty()) {
				hashes.append(one.sha1);
			}
		}
		return hashes;
	}

	void applyHashLookup(Built& built, const QJsonObject& answer)
	{
		QList<Skipped> still;
		for (const Skipped& one : built.skipped) {
			const QJsonObject version = answer.value(one.sha1).toObject();
			const ShareCodes::Entry entry{
				one.kind, ShareCodes::Platform::Modrinth,
				version.value("project_id").toString(),
				version.value("id").toString()};
			if (one.sha1.isEmpty() || one.reason != Skipped::Reason::NoSource ||
				entry.projectId.isEmpty() || entry.fileId.isEmpty()) {
				still.append(one);
				continue;
			}
			built.manifest.content.append(entry);
			built.included.append(
				{one.name, one.fileName, one.kind, entry.platform});
		}
		built.skipped = still;
		sortByName(built.included);
	}

	QVariantMap toMap(const Built& built)
	{
		QVariantList included;
		QHash<QString, int> counts;
		for (const Item& item : built.included) {
			included.append(QVariantMap{
				{"name", item.name}, {"kind", item.kind}, {"platform", item.platform}});
			++counts[item.kind];
		}
		QVariantList skippedList;
		for (const Skipped& one : built.skipped) {
			skippedList.append(QVariantMap{{"name", one.name},
										   {"kind", one.kind},
										   {"reason", reasonName(one.reason)}});
		}
		return {{"name", built.manifest.name},
				{"minecraftVersion", built.manifest.minecraftVersion},
				{"loader", built.manifest.loader},
				{"loaderVersion", built.manifest.loaderVersion},
				{"included", included},
				{"skipped", skippedList},
				{"mods", counts.value(ShareCodes::Kind::Mod)},
				{"resourcePacks", counts.value(ShareCodes::Kind::ResourcePack)},
				{"shaderPacks", counts.value(ShareCodes::Kind::ShaderPack)},
				{"dataPacks", counts.value(ShareCodes::Kind::DataPack)}};
	}
} // namespace ShareManifest
