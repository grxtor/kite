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

#include "DroppedContent.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

#include "FileSystem.h"
#include "MMCZip.h"
#include "ModMetadataReader.h"
#include "PackLayout.h"
#include "VersionRange.h"
#include "models/WorldTools.h"

namespace
{
	const QStringList kArchiveSuffixes = {QStringLiteral("zip"),
										  QStringLiteral("jar"),
										  QStringLiteral("litemod"),
										  QStringLiteral("mrpack")};

	bool hasEntry(const QStringList& entries, const QString& name)
	{
		return entries.contains(name);
	}

	/* The first path segment's folder when @p entry is "folder/<name>" with
	 * <name> directly inside; "folder/a/b" does not count. */
	bool isOneLevelDown(const QString& entry, const QString& name)
	{
		const int slash = entry.indexOf(QLatin1Char('/'));
		return slash > 0 && entry.mid(slash + 1) == name;
	}

	bool hasEntryAtRootOrOneDown(const QStringList& entries,
								 const QString& name)
	{
		for (const QString& entry : entries) {
			if (entry == name || isOneLevelDown(entry, name)) {
				return true;
			}
		}
		return false;
	}

	/* Archives InstanceImportTask takes: Modrinth (modrinth.index.json),
	 * MultiMC/Prism (instance.cfg), Technic (bin/modpack.jar) and CurseForge
	 * (a manifest.json that says it is a modpack, so a stray one is not). */
	bool isModpackArchive(const QString& path, const QStringList& entries)
	{
		if (hasEntryAtRootOrOneDown(entries,
									QStringLiteral("modrinth.index.json")) ||
			hasEntryAtRootOrOneDown(entries, QStringLiteral("instance.cfg")) ||
			hasEntry(entries, QStringLiteral("bin/modpack.jar")) ||
			hasEntry(entries, QStringLiteral("bin/version.json"))) {
			return true;
		}
		for (const QString& entry : entries) {
			if (entry != QLatin1String("manifest.json") &&
				!isOneLevelDown(entry, QStringLiteral("manifest.json"))) {
				continue;
			}
			const QJsonObject manifest =
				QJsonDocument::fromJson(MMCZip::readFileFromZip(path, entry))
					.object();
			return manifest.value(QLatin1String("manifestType")).toString() ==
				   QLatin1String("minecraftModpack");
		}
		return false;
	}

	void fillMod(DroppedContent::Item& item,
				 const std::shared_ptr<ModDetails>& details)
	{
		item.kind = DroppedContent::Kind::Mod;
		if (!details) {
			return;
		}
		item.hasMetadata = true;
		item.details = *details;
		item.modId = details->mod_id;
		item.version = details->version;
		if (!details->name.isEmpty()) {
			item.name = details->name;
		}
	}

	std::shared_ptr<ModDetails> detailsOfArchive(const QString& path)
	{
		const ModMetadata::EntryReader read = [&path](const QStringList& names) {
			return MMCZip::readFilesFromZip(path, names);
		};
		return ModMetadata::readDetails(read(ModMetadata::kMetadataEntries),
										read);
	}

	std::shared_ptr<ModDetails> detailsOfFolder(const QString& path)
	{
		const ModMetadata::EntryReader read = [&path](const QStringList& names) {
			ModMetadata::ZipEntries files;
			const QDir root(path);
			for (const QString& name : names) {
				// A nested jar's name comes from metadata: keep it inside.
				const QString file = root.filePath(name);
				if (!QDir::cleanPath(file).startsWith(
						QDir::cleanPath(root.path()) + QLatin1Char('/'))) {
					continue;
				}
				QFile handle(file);
				if (handle.open(QIODevice::ReadOnly)) {
					files.insert(name, handle.readAll());
				}
			}
			return files;
		};
		return ModMetadata::readDetails(read(ModMetadata::kMetadataEntries),
										read);
	}

	/* The folder holding level.dat: @p dir itself, or its only-level-down child. */
	QString worldFolderIn(const QString& dir)
	{
		if (QFileInfo(QDir(dir).filePath(QStringLiteral("level.dat"))).isFile()) {
			return WorldTools::isWorld(dir) ? dir : QString();
		}
		const QStringList children =
			QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot);
		for (const QString& child : children) {
			const QString nested = QDir(dir).filePath(child);
			if (QFileInfo(QDir(nested).filePath(QStringLiteral("level.dat")))
					.isFile() &&
				WorldTools::isWorld(nested)) {
				return nested;
			}
		}
		return QString();
	}

	DroppedContent::Kind packKind(const QFileInfo& info)
	{
		if (PackLayout::isResourcePack(info)) {
			return DroppedContent::Kind::ResourcePack;
		}
		if (PackLayout::isDataPack(info)) {
			return DroppedContent::Kind::DataPack;
		}
		if (PackLayout::isShaderPack(info)) {
			return DroppedContent::Kind::ShaderPack;
		}
		return DroppedContent::Kind::Unknown;
	}

	void classifyFolder(DroppedContent::Item& item)
	{
		const QString world = worldFolderIn(item.path);
		if (!world.isEmpty()) {
			item.kind = DroppedContent::Kind::World;
			item.path = world;
			return;
		}
		if (const auto details = detailsOfFolder(item.path)) {
			fillMod(item, details);
			return;
		}
		if (QFileInfo(QDir(item.path).filePath(QStringLiteral("mcmod.info")))
				.isFile()) {
			item.kind = DroppedContent::Kind::Mod;
			return;
		}
		item.kind = packKind(QFileInfo(item.path));
	}

	void classifyArchive(DroppedContent::Item& item, const QString& suffix)
	{
		if (suffix == QLatin1String("mrpack")) {
			item.kind = DroppedContent::Kind::Modpack;
			return;
		}
		const QStringList entries = PackLayout::archiveEntries(item.path);
		if (suffix == QLatin1String("litemod")) {
			item.kind = DroppedContent::Kind::Mod;
			return;
		}
		if (entries.isEmpty()) {
			return; // Not an archive, or an empty one.
		}
		if (suffix == QLatin1String("zip")) {
			if (isModpackArchive(item.path, entries)) {
				item.kind = DroppedContent::Kind::Modpack;
				return;
			}
			if (hasEntryAtRootOrOneDown(entries,
										QStringLiteral("level.dat")) &&
				WorldTools::isWorld(item.path)) {
				item.kind = DroppedContent::Kind::World;
				return;
			}
		}
		if (const auto details = detailsOfArchive(item.path)) {
			fillMod(item, details);
			return;
		}
		if (suffix == QLatin1String("jar")) {
			// Mods from before metadata existed carry only classes.
			item.kind = DroppedContent::Kind::Mod;
			return;
		}
		item.kind = packKind(QFileInfo(item.path));
	}

	QString loaderLabel(const QString& loader)
	{
		static const QHash<QString, QString> names = {
			{QStringLiteral("fabric"), QStringLiteral("Fabric")},
			{QStringLiteral("quilt"), QStringLiteral("Quilt")},
			{QStringLiteral("forge"), QStringLiteral("Forge")},
			{QStringLiteral("neoforge"), QStringLiteral("NeoForge")},
			{QStringLiteral("liteloader"), QStringLiteral("LiteLoader")}};
		return names.value(loader, loader);
	}

	QString loaderList(const QStringList& loaders)
	{
		QStringList labels;
		for (const QString& loader : loaders) {
			labels << loaderLabel(loader);
		}
		return labels.join(QStringLiteral(", "));
	}

	bool isModFileName(const QString& name)
	{
		for (const QString& suffix :
			 {QStringLiteral(".jar"), QStringLiteral(".zip"),
			  QStringLiteral(".litemod"), QStringLiteral(".jar.disabled"),
			  QStringLiteral(".zip.disabled")}) {
			if (name.endsWith(suffix, Qt::CaseInsensitive)) {
				return true;
			}
		}
		return false;
	}
} // namespace

namespace DroppedContent
{
	Item classify(const QString& fileUrlOrPath)
	{
		Item item;
		item.path = QDir::cleanPath(FS::pathFromUrlOrPath(fileUrlOrPath));
		const QFileInfo info(item.path);
		item.fileName = info.fileName();
		item.isFolder = info.isDir();
		if (!info.exists()) {
			return item;
		}
		item.name = info.completeBaseName();

		if (info.isDir()) {
			classifyFolder(item);
		} else if (kArchiveSuffixes.contains(info.suffix().toLower())) {
			classifyArchive(item, info.suffix().toLower());
		}
		return item;
	}

	void Counts::add(Kind kind)
	{
		switch (kind) {
			case Kind::Mod:
				++mods;
				break;
			case Kind::ResourcePack:
				++resourcePacks;
				break;
			case Kind::DataPack:
				++dataPacks;
				break;
			case Kind::ShaderPack:
				++shaderPacks;
				break;
			case Kind::World:
				++worlds;
				break;
			case Kind::Modpack:
				++modpacks;
				break;
			case Kind::Unknown:
				++unknown;
				break;
		}
	}

	int Counts::total() const
	{
		return mods + resourcePacks + dataPacks + shaderPacks + worlds +
			   modpacks;
	}

	QString describe(Kind kind, int count)
	{
		const auto many = [count](const QString& text) {
			return text.arg(count);
		};
		if (count < 1) {
			return QString();
		}
		switch (kind) {
			case Kind::Mod:
				return count == 1 ? QCoreApplication::translate(
										"DroppedContent", "1 mod")
								  : many(QCoreApplication::translate(
										"DroppedContent", "%1 mods"));
			case Kind::ResourcePack:
				return count == 1
						   ? QCoreApplication::translate("DroppedContent",
														 "1 resource pack")
						   : many(QCoreApplication::translate(
								 "DroppedContent", "%1 resource packs"));
			case Kind::DataPack:
				return count == 1
						   ? QCoreApplication::translate("DroppedContent",
														 "1 data pack")
						   : many(QCoreApplication::translate(
								 "DroppedContent", "%1 data packs"));
			case Kind::ShaderPack:
				return count == 1
						   ? QCoreApplication::translate("DroppedContent",
														 "1 shader pack")
						   : many(QCoreApplication::translate(
								 "DroppedContent", "%1 shader packs"));
			case Kind::World:
				return count == 1 ? QCoreApplication::translate(
										"DroppedContent", "1 world")
								  : many(QCoreApplication::translate(
										"DroppedContent", "%1 worlds"));
			case Kind::Modpack:
				return count == 1 ? QCoreApplication::translate(
										"DroppedContent", "1 modpack")
								  : many(QCoreApplication::translate(
										"DroppedContent", "%1 modpacks"));
			case Kind::Unknown:
				break;
		}
		return QString();
	}

	QString describeUnknown(int count)
	{
		if (count < 1) {
			return QString();
		}
		return count == 1
				   ? QCoreApplication::translate(
						 "DroppedContent", "1 file Kite doesn't recognise")
				   : QCoreApplication::translate(
						 "DroppedContent", "%1 files Kite doesn't recognise")
						 .arg(count);
	}

	QString describe(const Counts& counts)
	{
		QStringList parts;
		for (const auto& [kind, count] :
			 {std::pair{Kind::Mod, counts.mods},
			  std::pair{Kind::ResourcePack, counts.resourcePacks},
			  std::pair{Kind::DataPack, counts.dataPacks},
			  std::pair{Kind::ShaderPack, counts.shaderPacks},
			  std::pair{Kind::World, counts.worlds},
			  std::pair{Kind::Modpack, counts.modpacks}}) {
			if (count > 0) {
				parts << describe(kind, count);
			}
		}
		if (parts.size() < 2) {
			return parts.value(0);
		}
		const QString last = parts.takeLast();
		const QString head =
			parts.join(QCoreApplication::translate("DroppedContent", ", "));
		return QCoreApplication::translate("DroppedContent", "%1 and %2")
			.arg(head, last);
	}

	QString contentKind(Kind kind)
	{
		switch (kind) {
			case Kind::Mod:
				return QStringLiteral("mods");
			case Kind::ResourcePack:
				return QStringLiteral("resourcepacks");
			case Kind::ShaderPack:
				return QStringLiteral("shaderpacks");
			default:
				return QString();
		}
	}

	QString identicalFileIn(const QString& dir, const Item& item)
	{
		const QFileInfo source(item.path);
		if (item.isFolder || !source.isFile()) {
			return QString();
		}
		QString hash;
		QDirIterator files(dir, QDir::Files);
		while (files.hasNext()) {
			const QFileInfo existing(files.next());
			if (existing.size() != source.size()) {
				continue;
			}
			if (hash.isEmpty()) {
				hash = FS::sha1Hex(item.path);
			}
			if (!hash.isEmpty() && FS::sha1Hex(existing.filePath()) == hash) {
				return existing.fileName();
			}
		}
		return QString();
	}

	QString sameModIn(const QString& dir, const Item& mod)
	{
		if (mod.modId.isEmpty()) {
			return QString();
		}
		QDirIterator files(dir, QDir::Files);
		while (files.hasNext()) {
			const QFileInfo existing(files.next());
			if (!isModFileName(existing.fileName())) {
				continue;
			}
			const auto details = detailsOfArchive(existing.filePath());
			if (details &&
				details->mod_id.compare(mod.modId, Qt::CaseInsensitive) == 0) {
				return existing.fileName();
			}
		}
		return QString();
	}

	QString compatibilityProblem(const Item& mod,
								 const ModHealth::Environment& environment)
	{
		if (!mod.hasMetadata || environment.loaders.isEmpty()) {
			return QString();
		}
		ModHealth::Entry entry;
		entry.key = mod.fileName;
		entry.name = mod.name;
		entry.details = mod.details;
		for (const ModHealth::Issue& issue :
			 ModHealth::check({entry}, environment)) {
			if (issue.kind == ModHealth::Kind::WrongLoader) {
				return QCoreApplication::translate(
						   "DroppedContent", "“%1” is built for %2, not %3.")
					.arg(mod.name, loaderList(mod.details.loaders),
						 loaderList(environment.loaders));
			}
			if (issue.kind == ModHealth::Kind::WrongMinecraft) {
				const QString wanted = VersionRange::describe(issue.dependency);
				if (wanted.isEmpty()) {
					return QCoreApplication::translate(
							   "DroppedContent",
							   "“%1” is made for another Minecraft version "
							   "than %2.")
						.arg(mod.name, environment.minecraft);
				}
				return QCoreApplication::translate(
						   "DroppedContent",
						   "“%1” is made for Minecraft %2, not %3.")
					.arg(mod.name, wanted, environment.minecraft);
			}
		}
		return QString();
	}

	Plan plan(const QStringList& sources, const Target& target)
	{
		Plan result;
		for (const QString& source : sources) {
			const QUrl url(source);
			if (url.scheme() == QLatin1String("http") ||
				url.scheme() == QLatin1String("https")) {
				(parseProjectLink(url).valid() ? result.projectLinks
											   : result.otherLinks)
					<< source;
				continue;
			}

			Entry entry;
			entry.item = classify(source);
			const Item& item = entry.item;
			result.counts.add(item.kind);
			if (item.kind == Kind::Unknown) {
				result.unknown << (item.fileName.isEmpty() ? source
														   : item.fileName);
				continue;
			}
			if (item.kind == Kind::Modpack) {
				result.modpacks << item.path;
				continue;
			}

			entry.dir = item.kind == Kind::Mod ? target.modsDir
						: item.kind == Kind::ResourcePack
							? target.resourcePacksDir
						: item.kind == Kind::ShaderPack ? target.shaderPacksDir
														: QString();
			if (!entry.dir.isEmpty()) {
				entry.identical = identicalFileIn(entry.dir, item);
				if (item.kind == Kind::Mod && entry.identical.isEmpty()) {
					entry.sameMod = sameModIn(entry.dir, item);
					entry.warning =
						compatibilityProblem(item, target.environment);
				}
			}
			result.entries.append(entry);
		}
		return result;
	}

	ProjectLink parseProjectLink(const QUrl& url)
	{
		static const QHash<QString, QString> modrinthTypes = {
			{QStringLiteral("mod"), QStringLiteral("mods")},
			{QStringLiteral("resourcepack"), QStringLiteral("resourcepacks")},
			{QStringLiteral("shader"), QStringLiteral("shaderpacks")},
			{QStringLiteral("datapack"), QStringLiteral("datapacks")}};
		static const QHash<QString, QString> curseForgeTypes = {
			{QStringLiteral("mc-mods"), QStringLiteral("mods")},
			{QStringLiteral("texture-packs"), QStringLiteral("resourcepacks")},
			{QStringLiteral("shaders"), QStringLiteral("shaderpacks")},
			{QStringLiteral("data-packs"), QStringLiteral("datapacks")}};

		ProjectLink link;
		if (url.scheme() != QLatin1String("https") &&
			url.scheme() != QLatin1String("http")) {
			return link;
		}
		QString host = url.host().toLower();
		if (host.startsWith(QLatin1String("www."))) {
			host.remove(0, 4);
		}
		const QStringList parts =
			url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
		if (host == QLatin1String("modrinth.com") && parts.size() >= 2) {
			link.contentType = modrinthTypes.value(parts.at(0));
			link.slug = parts.at(1);
			link.provider = QStringLiteral("modrinth");
		} else if (host == QLatin1String("curseforge.com") &&
				   parts.size() >= 3 &&
				   parts.at(0) == QLatin1String("minecraft")) {
			link.contentType = curseForgeTypes.value(parts.at(1));
			link.slug = parts.at(2);
			link.provider = QStringLiteral("curseforge");
		}
		if (link.contentType.isEmpty()) {
			return ProjectLink();
		}
		return link;
	}

	QString summarize(const Outcome& outcome, QString* tone)
	{
		QStringList sentences;
		const bool added = outcome.added.total() > 0;
		if (added) {
			sentences << QCoreApplication::translate("DroppedContent",
													 "Added %1.")
							 .arg(describe(outcome.added));
		}
		if (outcome.replaced == 1) {
			sentences << QCoreApplication::translate(
				"DroppedContent", "Replaced 1 older version.");
		} else if (outcome.replaced > 1) {
			sentences << QCoreApplication::translate(
							 "DroppedContent", "Replaced %1 older versions.")
							 .arg(outcome.replaced);
		}
		if (outcome.warnings.size() <= 2) {
			sentences << outcome.warnings;
		} else {
			sentences << QCoreApplication::translate(
							 "DroppedContent",
							 "%1 of them may not work in this instance.")
							 .arg(outcome.warnings.size());
		}
		if (outcome.failed.size() == 1) {
			sentences << QCoreApplication::translate(
							 "DroppedContent", "“%1” could not be added.")
							 .arg(outcome.failed.first());
		} else if (outcome.failed.size() > 1) {
			sentences << QCoreApplication::translate(
							 "DroppedContent", "%1 files could not be added.")
							 .arg(outcome.failed.size());
		}
		if (outcome.locked.size() == 1) {
			sentences << QCoreApplication::translate(
				"DroppedContent",
				"The game is running: 1 file can be added once it closes.");
		} else if (outcome.locked.size() > 1) {
			sentences << QCoreApplication::translate(
							 "DroppedContent",
							 "The game is running: %1 files can be added once "
							 "it closes.")
							 .arg(outcome.locked.size());
		}
		if (outcome.unknown.size() == 1) {
			sentences << QCoreApplication::translate(
							 "DroppedContent",
							 "Skipped “%1”: it is not a mod, pack or world.")
							 .arg(outcome.unknown.first());
		} else if (outcome.unknown.size() > 1) {
			sentences << QCoreApplication::translate(
							 "DroppedContent",
							 "Skipped %1 files that are not mods, packs or "
							 "worlds.")
							 .arg(outcome.unknown.size());
		}
		if (sentences.isEmpty()) {
			sentences << (outcome.identical > 0
							  ? QCoreApplication::translate(
									"DroppedContent",
									"Nothing new: that is already here.")
							  : QCoreApplication::translate("DroppedContent",
															"Nothing to add."));
		}

		const bool problem = !outcome.failed.isEmpty() ||
							 !outcome.locked.isEmpty() ||
							 !outcome.unknown.isEmpty();
		if (tone) {
			*tone = !added && problem ? QStringLiteral("danger")
					: !outcome.warnings.isEmpty() || problem
						? QStringLiteral("warning")
					: added ? QStringLiteral("success")
							: QStringLiteral("neutral");
		}
		return sentences.join(QLatin1Char(' '));
	}
} // namespace DroppedContent
