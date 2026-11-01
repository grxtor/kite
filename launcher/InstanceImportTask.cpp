/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
 * Copyright (C) 2026 Project Tick
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

#include "InstanceImportTask.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "core/LauncherContext.h"
#include "core/UiHost.h"
#include "InstanceList.h"
#include "MMCZip.h"
#include "archive/ExtractZipTask.h"
#include "NullInstance.h"
#include "settings/INISettingsObject.h"
#include "icons/IconUtils.h"
#include <QRegularExpression>
#include <QtConcurrentRun>

// FIXME: this does not belong here, it's Minecraft/Flame specific
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/flame/FileResolvingTask.h"
#include "modplatform/flame/PackManifest.h"
#include "modplatform/modrinth/ModrinthPackManifest.h"
#include "modplatform/PackContents.h"
#include "Json.h"
#include "modplatform/technic/TechnicPackProcessor.h"

#include "icons/IconList.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"

#include <QDirIterator>
#include <QHash>

#include <memory>

#include <QDir>
#include <QStandardPaths>

InstanceImportTask::InstanceImportTask(const QUrl sourceUrl)
{
	m_sourceUrl = sourceUrl;
}

void InstanceImportTask::executeTask()
{
	if (m_sourceUrl.isLocalFile()) {
		m_archivePath = m_sourceUrl.toLocalFile();
		processZipPack();
	} else {
		setStatus(tr("Downloading modpack:\n%1").arg(m_sourceUrl.toString()));
		m_downloadRequired = true;

		const QString path = m_sourceUrl.host() + '/' + m_sourceUrl.path();
		auto entry = LAUNCHER->metacache()->resolveEntry("general", path);
		entry->setStale(true);
		m_archiveEntry = entry;
		m_filesNetJob =
			new NetJob(tr("Modpack download"), LAUNCHER->network());
		m_filesNetJob->addNetAction(
			Net::Download::makeCached(m_sourceUrl, entry));
		m_archivePath = entry->getFullPath();
		auto job = m_filesNetJob.get();
		connect(job, &NetJob::succeeded, this,
				&InstanceImportTask::downloadSucceeded);
		connect(job, &NetJob::progress, this,
				&InstanceImportTask::downloadProgressChanged);
		connect(job, &NetJob::failed, this,
				&InstanceImportTask::downloadFailed);
		propagateStepsFrom(job);
		m_filesNetJob->start();
	}
}

void InstanceImportTask::downloadSucceeded()
{
	processZipPack();
	m_filesNetJob.reset();
}

void InstanceImportTask::downloadFailed(QString reason)
{
	m_filesNetJob.reset();
	if (m_aborted) {
		/* An aborted NetJob reports failure; m_aborted tells the two apart. */
		emitAborted();
		return;
	}
	emitFailed(reason);
}

void InstanceImportTask::downloadProgressChanged(qint64 current, qint64 total)
{
	setProgress(current / 2, total);
}

void InstanceImportTask::processZipPack()
{
	setStatus(tr("Inspecting modpack archive"));
	qDebug() << "Detecting modpack type for" << m_archivePath;

	// Scans the archive off the UI thread in one listEntries() pass;
	// findFolderOfFileInZip() reopens it per directory level.
	const QString archivePath = m_archivePath;
	const QString stagingPath = m_stagingPath;

	m_detectFuture = QtConcurrent::run([archivePath, stagingPath]() {
		DetectResult result;
		result.extractTarget = stagingPath;

		const QStringList entries = MMCZip::listEntries(archivePath);

		// Folder prefix of the first entry whose file name is `needle`: empty
		// at the root, null if absent.
		auto findFolder = [&entries](const QString& needle) -> QString {
			for (const QString& e : entries) {
				if (e == needle)
					return QLatin1String("");
				if (e.endsWith(QLatin1Char('/') + needle)) {
					return e.left(e.lastIndexOf(QLatin1Char('/')) + 1);
				}
			}
			return {};
		};

		result.mmcRoot = findFolder(QStringLiteral("instance.cfg"));
		result.modrinthRoot = findFolder(QStringLiteral("modrinth.index.json"));
		result.flameRoot = findFolder(QStringLiteral("manifest.json"));
		result.technicFound =
			entries.contains(QStringLiteral("bin/modpack.jar")) ||
			entries.contains(QStringLiteral("bin/version.json")) ||
			entries.contains(QStringLiteral("bin/modpack.jar/")) ||
			entries.contains(QStringLiteral("bin/version.json/"));

		if (result.technicFound && result.mmcRoot.isNull() &&
			result.modrinthRoot.isNull()) {
			QDir extractDir(stagingPath);
			extractDir.mkpath(QStringLiteral(".minecraft"));
			result.extractTarget =
				extractDir.absoluteFilePath(QStringLiteral(".minecraft"));
		}

		return result;
	});

	connect(&m_detectFutureWatcher, &QFutureWatcher<DetectResult>::finished,
			this, &InstanceImportTask::detectFinished);
	m_detectFutureWatcher.setFuture(m_detectFuture);
}

void InstanceImportTask::detectFinished()
{
	if (m_aborted) {
		/* The scan cannot be interrupted, so an abort is reported once it has
		 * finished. */
		emitAborted();
		return;
	}

	const DetectResult r = m_detectFuture.result();

	QString root;
	if (!r.mmcRoot.isNull()) {
		qDebug() << "Kite:" << r.mmcRoot;
		m_modpackType = ModpackType::Kite;
		root = r.mmcRoot;
	} else if (!r.modrinthRoot.isNull()) {
		qDebug() << "Modrinth:" << r.modrinthRoot;
		m_modpackType = ModpackType::Modrinth;
		root = r.modrinthRoot;
	} else if (r.technicFound) {
		qDebug() << "Technic";
		m_modpackType = ModpackType::Technic;
		/* Root stays empty: a Technic pack ships its files at the archive
		 * root. */
	} else if (!r.flameRoot.isNull()) {
		qDebug() << "Flame:" << r.flameRoot;
		m_modpackType = ModpackType::Flame;
		root = r.flameRoot;
	}

	if (m_modpackType == ModpackType::Unknown) {
		QFileInfo fi(m_archivePath);
		if (!fi.exists() || fi.size() == 0) {
			emitFailed(tr("Modpack archive is missing or empty:\n%1")
						   .arg(m_archivePath));
		} else {
			emitFailed(
				tr("Archive does not contain a recognized modpack type."));
		}
		return;
	}

	setStatus(tr("Extracting modpack"));

	auto* extract =
		new MMCZip::ExtractZipTask(m_archivePath, QDir(r.extractTarget), root);
	m_extractTask.reset(extract);

	/* No separate aborted signal: m_aborted tells an abort from a failure. */
	connect(extract, &Task::succeeded, this,
			&InstanceImportTask::extractFinished);
	connect(extract, &Task::failed, this, [this](const QString& reason) {
		m_extractTask.reset();
		if (m_aborted) {
			emitAborted();
			return;
		}
		/* Logged only: extractFailed() tells the user more than the zip
		 * library's reason, and decides what to do with the archive. */
		qWarning() << "Extracting" << m_archivePath << "failed:" << reason;
		extractFailed();
	});

	connect(extract, &Task::progress, this, &Task::setProgress);
	connect(extract, &Task::status, this, &Task::setStatus);
	propagateStepsFrom(extract);

	extract->start();
}

bool InstanceImportTask::abort()
{
	if (m_aborted) {
		/* Already stopping; a second verdict would emit finished() twice. */
		return true;
	}
	m_aborted = true;

	/* Every running step is told to stop, not just a "current task" pointer's:
	 * a step that forgot to register there would ignore an abort. Only one is
	 * live at a time. */
	if (m_filesNetJob) {
		m_filesNetJob->abort();
		return true;
	}
	if (m_extractTask) {
		m_extractTask->abort();
		return true;
	}
	if (m_modIdResolver) {
		m_modIdResolver->abort();
		return true;
	}

	if (m_detectFutureWatcher.isRunning()) {
		/* Inspecting is one uninterruptible pass; detectFinished() sees the
		 * flag and stops there. */
		return true;
	}

	emitAborted();
	return true;
}

/* Always a real failure; aborts are handled where the flag is checked. */
void InstanceImportTask::extractFailed()
{
	/* Only a cached download is ours to delete; a file the user dragged in is
	 * theirs. */
	if (m_archiveEntry) {
		qWarning() << "Discarding unusable cached archive" << m_archivePath;
		/* Remove the file, not just the entry's freshness: a stale entry whose
		 * file survives gets a 304 and the same damaged bytes. */
		if (!QFile::remove(m_archivePath)) {
			qWarning() << "Could not remove" << m_archivePath;
		}
		LAUNCHER->metacache()->evictEntry(m_archiveEntry);
		m_archiveEntry.reset();
		emitFailed(tr("Failed to extract modpack. The downloaded archive is "
					  "damaged; it has been discarded, so trying again will "
					  "download it afresh."));
		return;
	}
	emitFailed(tr("Failed to extract modpack. The archive appears to be "
				  "damaged."));
}

void InstanceImportTask::extractFinished()
{
	m_extractTask.reset();

	if (m_aborted) {
		emitAborted();
		return;
	}

	QDir extractDir(m_stagingPath);

	qDebug() << "Fixing permissions for extracted pack files...";
	QDirIterator it(extractDir, QDirIterator::Subdirectories);
	while (it.hasNext()) {
		auto filepath = it.next();
		QFileInfo file(filepath);
		auto permissions = QFile::permissions(filepath);
		auto origPermissions = permissions;
		if (file.isDir()) {
			permissions |= QFileDevice::Permission::ReadUser |
						   QFileDevice::Permission::WriteUser |
						   QFileDevice::Permission::ExeUser;
		} else {
			permissions |= QFileDevice::Permission::ReadUser |
						   QFileDevice::Permission::WriteUser;
		}
		if (origPermissions != permissions) {
			if (!QFile::setPermissions(filepath, permissions)) {
				logWarning(
					tr("Could not fix permissions for %1").arg(filepath));
			} else {
				qDebug() << "Fixed" << filepath;
			}
		}
	}

	switch (m_modpackType) {
		case ModpackType::Flame:
			processFlame();
			return;
		case ModpackType::Modrinth:
			processModrinth();
			return;
		case ModpackType::Kite:
			processKite();
			return;
		case ModpackType::Technic:
			processTechnic();
			return;
		case ModpackType::Unknown:
			emitFailed(
				tr("Archive does not contain a recognized modpack type."));
			return;
	}
}


namespace
{
	/* Parses a Modrinth CDN URL
	 * (https://cdn.modrinth.com/data/{projectId}/versions/{number}/{file})
	 * into the bare project id and the version number (e.g. "1.1.1+1.17"; the
	 * version id is not in the URL). Empty strings on failure. */
	struct ModrinthIds {
		QString projectId;
		/* A version number, not an id: used as one it broke dependency
		 * resolution and made the update check offer every mod an update to
		 * itself. */
		QString versionNumber;
	};

	ModrinthIds parseModrinthCdnUrl(const QUrl& url)
	{
		ModrinthIds out;
		if (!url.host().contains(QLatin1String("modrinth.com")))
			return out;
		const QStringList segs = url.path().split('/', Qt::SkipEmptyParts);
		const int dataIdx = segs.indexOf(QStringLiteral("data"));
		const int versionsIdx = segs.indexOf(QStringLiteral("versions"));
		if (dataIdx >= 0 && dataIdx + 1 < segs.size())
			out.projectId = segs.at(dataIdx + 1);
		if (versionsIdx >= 0 && versionsIdx + 1 < segs.size())
			out.versionNumber = segs.at(versionsIdx + 1);
		return out;
	}

	/* Maps a pack-relative `path` (e.g. "mods/sodium.jar") to the folder
	 * holding its file; empty if not under an indexable mod-like folder. */
	QString sidecarFolderForPath(const QString& minecraftDir,
								 const QString& packRelativePath)
	{
		const QString fwd = QString(packRelativePath).replace('\\', '/');
		const int slash = fwd.indexOf('/');
		if (slash < 1)
			return {};
		const QString top = fwd.left(slash);
		if (top == QLatin1String("mods") ||
			top == QLatin1String("resourcepacks") ||
			top == QLatin1String("shaderpacks") ||
			top == QLatin1String("texturepacks") ||
			top == QLatin1String("coremods")) {
			return FS::PathCombine(minecraftDir, top);
		}
		return {};
	}

	void writeModrinthModSidecars(const QString& minecraftDir,
								  const QVector<Modrinth::File>& files)
	{
		const QDateTime now = QDateTime::currentDateTimeUtc();
		for (const auto& f : files) {
			if (!f.clientSupported) {
				/* Never installed on a client, so no sidecar: orphaned
				 * `.pw.toml` files are never pruned, and one written here
				 * would claim a mod the instance lacks. */
				continue;
			}
			const QString folder = sidecarFolderForPath(minecraftDir, f.path);
			if (folder.isEmpty())
				continue;
			const ModrinthIds ids = parseModrinthCdnUrl(f.downloadUrl);
			if (ids.projectId.isEmpty())
				continue;

			QDir folderDir(folder);
			ModMetadataIndex idx(folderDir);
			ModMetadataIndex::Entry e;
			e.fileName = QFileInfo(f.path).fileName();
			e.platform = QStringLiteral("modrinth");
			e.projectId = ids.projectId;
			/* An mrpack has no version id; the recorded hash identifies the
			 * file, and the version number goes in its own field. */
			e.versionNumber = ids.versionNumber;
			/* mrpack manifests carry no display names; use the file name. */
			e.name = QFileInfo(f.path).completeBaseName();
			e.downloadUrl = f.downloadUrl.toString();
			e.sha1 = f.sha1;
			e.fileSize = f.fileSize;
			e.installedAt = now;
			idx.put(e);
		}
	}

	void writeFlameModSidecars(const QString& minecraftDir,
							   const QVector<Flame::File>& files)
	{
		const QDateTime now = QDateTime::currentDateTimeUtc();
		for (const auto& f : files) {
			if (f.fileName.isEmpty() || f.projectId == 0 || f.fileId == 0)
				continue;
			const QString folder =
				FS::PathCombine(minecraftDir, f.targetFolder);
			QDir folderDir(folder);
			ModMetadataIndex idx(folderDir);
			ModMetadataIndex::Entry e;
			e.fileName = f.fileName;
			e.platform = QStringLiteral("curseforge");
			e.projectId = QString::number(f.projectId);
			e.versionId = QString::number(f.fileId);
			e.name = QFileInfo(f.fileName).completeBaseName();
			e.downloadUrl = f.url.toString();
			e.installedAt = now;
			idx.put(e);
		}
	}

	/* Whether the file at @p path already has the expected digest, to skip
	 * re-downloading what an update does not change. Read off the disk, not
	 * compared between manifests, so a replaced or truncated jar is caught. */
	bool fileMatchesHash(const QString& path, const QString& sha1,
						 qint64 expectedSize)
	{
		if (sha1.isEmpty()) {
			return false;
		}
		const QFileInfo info(path);
		if (!info.isFile()) {
			return false;
		}
		if (expectedSize > 0 && info.size() != expectedSize) {
			return false;
		}

		const QString got = FS::sha1Hex(path);
		return !got.isEmpty() && got.compare(sha1, Qt::CaseInsensitive) == 0;
	}

	/* The first of @p candidates (game-relative) whose file under @p gameDir
	 * matches the digest, to be kept as installed; empty if none does. */
	QString firstInstalledMatch(const QDir& gameDir,
								const QStringList& candidates,
								const QString& sha1, qint64 expectedSize)
	{
		for (const QString& candidate : candidates) {
			if (fileMatchesHash(gameDir.absoluteFilePath(candidate), sha1,
								expectedSize)) {
				return candidate;
			}
		}
		return {};
	}

	QStringList listFilesRelative(const QString& dir)
	{
		QStringList found;
		const QDir root(dir);
		if (!root.exists()) {
			return found;
		}
		QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::System,
						QDirIterator::Subdirectories);
		while (it.hasNext()) {
			found.append(root.relativeFilePath(it.next()));
		}
		return found;
	}
} // namespace

void InstanceImportTask::processFlame()
{
	/* Asked first: whether this replaces an instance decides the directory
	 * name the overrides move into. */
	if (!resolveUpdateTargetFromCatalogue()) {
		emitFailed(tr("Installation cancelled."));
		return;
	}

	Flame::Manifest pack;
	try {
		QString configPath = FS::PathCombine(m_stagingPath, "manifest.json");
		Flame::loadManifest(pack, configPath);
		if (!QFile::remove(configPath)) {
			qWarning() << "Could not remove manifest.json from staging";
		}
	} catch (const JSONValidationError& e) {
		emitFailed(tr("Could not understand pack manifest:\n") + e.cause());
		return;
	}
	if (!pack.overrides.isEmpty()) {
		QString overridePath = FS::PathCombine(m_stagingPath, pack.overrides);
		if (QFile::exists(overridePath)) {
			QString mcPath = FS::PathCombine(m_stagingPath, gameDirName());
			if (!QFile::rename(overridePath, mcPath)) {
				emitFailed(tr("Could not rename the overrides folder:\n") +
						   pack.overrides);
				return;
			}
		} else {
			logWarning(tr("The specified overrides folder (%1) is missing. "
						  "Maybe the modpack was already used before?")
						   .arg(pack.overrides));
		}
	}

	configureFlameInstance(pack);

	/* The overrides are the game directory right now, before any download, so
	 * this is the only moment to tell them from the manifest's files. Read
	 * after configuring the instance, which moves bundled jar mods out into
	 * component patches. */
	m_packOverridePaths =
		listFilesRelative(FS::PathCombine(m_stagingPath, gameDirName()));

	m_modIdResolver =
		new Flame::FileResolvingTask(LAUNCHER->network(), pack);
	connect(m_modIdResolver.get(), &Flame::FileResolvingTask::succeeded, this,
			&InstanceImportTask::onFlameFileResolutionSucceeded);
	connect(m_modIdResolver.get(), &Flame::FileResolvingTask::failed,
			[&](QString reason) {
				m_modIdResolver.reset();
				emitFailed(tr("Unable to resolve mod IDs:\n") + reason);
			});
	connect(m_modIdResolver.get(), &Flame::FileResolvingTask::progress,
			[&](qint64 current, qint64 total) { setProgress(current, total); });
	connect(m_modIdResolver.get(), &Flame::FileResolvingTask::status,
			[&](QString status) { setStatus(status); });
	m_modIdResolver->start();
}

/* Which catalogue a set of downloads should come from; kept apart so neither
 * catalogue's network is trusted for the other's packs. */
enum class ContentSource { Modrinth, CurseForge };

static bool isKnownContentHost(const QUrl& url, ContentSource source);

static QString selectFlameIcon(const QString& instIcon,
							   const Flame::Manifest& pack)
{
	if (instIcon != "default")
		return instIcon;
	if (pack.name.contains("Direwolf20"))
		return "steve";
	if (pack.name.contains("FTB") || pack.name.contains("Feed The Beast"))
		return "ftb_logo";
	// Not the Kite default, so these packs can be told apart.
	return "flame";
}

void InstanceImportTask::configureFlameInstance(Flame::Manifest& pack)
{
	const static QMap<QString, QString> forgemap = {{"1.2.5", "3.4.9.171"},
													{"1.4.2", "6.0.1.355"},
													{"1.4.7", "6.6.2.534"},
													{"1.5.2", "7.8.1.737"}};

	struct FlameLoaderMapping {
		const char* prefix;
		QString version;
		const char* componentId;
	};
	FlameLoaderMapping loaderMappings[] = {
		{"forge-", {}, "net.minecraftforge"},
		{"fabric-", {}, "net.fabricmc.fabric-loader"},
		{"neoforge-", {}, "net.neoforged"},
		{"quilt-", {}, "org.quiltmc.quilt-loader"},
	};
	for (auto& loader : pack.minecraft.modLoaders) {
		auto id = loader.id;
		bool matched = false;
		for (auto& mapping : loaderMappings) {
			if (id.startsWith(mapping.prefix)) {
				id.remove(mapping.prefix);
				mapping.version = id;
				matched = true;
				break;
			}
		}
		if (!matched) {
			logWarning(tr("Unknown mod loader in manifest: %1").arg(id));
		}
	}

	QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
	instanceSettings->registerSetting("InstanceType", "Legacy");
	instanceSettings->set("InstanceType", "OneSix");
	MinecraftInstance instance(m_globalSettings, instanceSettings,
							   m_stagingPath);
	auto mcVersion = pack.minecraft.version;
	// Some packs have trailing dots in the Minecraft version.
	if (mcVersion.endsWith('.')) {
		mcVersion.remove(QRegularExpression("[.]+$"));
		logWarning(tr("Mysterious trailing dots removed from Minecraft version "
					  "while importing pack."));
	}
	auto components = instance.getPackProfile();
	components->buildingFromScratch();
	components->setComponentVersion("net.minecraft", mcVersion, true);

	auto& forgeMapping = loaderMappings[0];
	if (forgeMapping.version == "recommended") {
		if (forgemap.contains(mcVersion)) {
			forgeMapping.version = forgemap[mcVersion];
		} else {
			logWarning(
				tr("Could not map recommended forge version for Minecraft %1")
					.arg(mcVersion));
		}
	}

	for (const auto& mapping : loaderMappings) {
		if (!mapping.version.isEmpty()) {
			components->setComponentVersion(mapping.componentId,
											mapping.version);
		}
	}

	instance.setIconKey(selectFlameIcon(m_instIcon, pack));

	QString jarmodsPath =
		FS::PathCombine(m_stagingPath, gameDirName(), "jarmods");
	QFileInfo jarmodsInfo(jarmodsPath);
	if (jarmodsInfo.isDir()) {
		qDebug() << "Found jarmods:";
		QDir jarmodsDir(jarmodsPath);
		QStringList jarMods;
		for (auto info :
			 jarmodsDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files)) {
			qDebug() << info.fileName();
			jarMods.push_back(info.absoluteFilePath());
		}
		auto profile = instance.getPackProfile();
		profile->installJarMods(jarMods);
		FS::deletePath(jarmodsPath);
	}
	instance.setName(m_instName);

	/* Persists the pack-source hint for PackUpdater to read back without
	 * sniffing the manifest: the browser UI's hint if set, else recovered from
	 * the manifest. */
	PackSourceHint hint = m_packHint;
	if (hint.provider.isEmpty()) {
		hint.provider = QStringLiteral("curseforge");
	}
	if (hint.versionLabel.isEmpty() && !pack.version.isEmpty()) {
		hint.versionLabel = pack.version;
	}
	if (hint.packSlug.isEmpty() && !pack.name.isEmpty()) {
		/* CurseForge manifests carry no URL slug; the pack name stands in
		 * until the user attaches the pack. */
		hint.packSlug = pack.name;
	}
	if (hint.packName.isEmpty() && !pack.name.isEmpty()) {
		hint.packName = pack.name;
	}
	writePackSourceToInstance(instance, hint);
}

void InstanceImportTask::onFlameFileResolutionSucceeded()
{
	auto results = m_modIdResolver->getResults();

	/* Same check as the Modrinth path: the first point where every mod's URL
	 * is known, before anything is fetched. Unresolved files have no URL to
	 * judge and are handled as blocked mods. */
	{
		QStringList suspect;
		for (const auto& file : results.files) {
			if (!file.resolved || file.url.isEmpty()) {
				continue;
			}
			if (!isKnownContentHost(file.url, ContentSource::CurseForge)) {
				suspect.append(
					FS::PathCombine(file.targetFolder, file.fileName));
			}
		}
		suspect +=
			findBundledCode(FS::PathCombine(m_stagingPath, gameDirName()));
		suspect.removeDuplicates();

		if (!confirmUntrustedFiles(suspect)) {
			emitFailed(tr("Installation cancelled: the modpack's untrusted "
						  "content was not accepted."));
			return;
		}
	}

	m_filesNetJob = new NetJob(tr("Mod download"), LAUNCHER->network());

	QList<BlockedMod> blockedMods;

	/* Game-relative paths this version is responsible for, built from this
	 * loop (so it has the ".disabled" suffix and excludes skipped entries). */
	QStringList contentPaths = m_packOverridePaths;

	/* On an update, skip files already installed with the right hash;
	 * re-fetching a large pack to change a few mods is wasteful. A skipped
	 * path stays empty in staging, so the commit's merge keeps the existing
	 * file. */
	QDir installedGameDir;
	bool canReuseInstalled = false;
	if (!m_updateTarget.isEmpty()) {
		auto previous = LAUNCHER->instances()->getInstanceById(
			m_updateTarget.instanceId);
		if (auto minecraftPrevious =
				std::dynamic_pointer_cast<MinecraftInstance>(previous)) {
			installedGameDir = QDir(minecraftPrevious->gameRoot());
			canReuseInstalled = true;
			setStatus(tr("Checking files already installed..."));
		}
	}
	int reusedCount = 0;
	bool sawAnyDigest = false;

	for (auto result : results.files) {
		QString filename = result.fileName;
		if (!result.required) {
			filename += ".disabled";
		}

		auto gameRelPath = FS::PathCombine(result.targetFolder, filename);
		auto relpath = FS::PathCombine(gameDirName(), gameRelPath);
		auto path = FS::PathCombine(m_stagingPath, relpath);

		switch (result.type) {
			case Flame::File::Type::Folder: {
				logWarning(
					tr("This 'Folder' may need extracting: %1").arg(relpath));
				[[fallthrough]];
			}
			case Flame::File::Type::SingleFile:
				[[fallthrough]];
			case Flame::File::Type::Mod: {
				bool isBlocked = !result.resolved || !result.url.isValid() ||
								 result.url.isEmpty();
				if (isBlocked && !result.fileName.isEmpty()) {
					blockedMods.append({result.projectId, result.fileId,
										result.fileName, path, false});
					/* Recorded although the user fetches it: if handed over it
					 * belongs to the pack, and a path with no file costs
					 * nothing. */
					contentPaths.append(gameRelPath);
					break;
				}
				if (isBlocked) {
					logWarning(tr("Skipping mod %1 (project %2) - no download "
								  "URL and no filename available")
								   .arg(result.fileId)
								   .arg(result.projectId));
					break;
				}
				if (canReuseInstalled) {
					if (!result.sha1.isEmpty()) {
						sawAnyDigest = true;
					}
					/* Both the enabled and disabled names are checked:
					 * fetching the enabled copy would turn a disabled mod back
					 * on. The name the pack wants is tried first. */
					const QString otherRel =
						result.required
							? gameRelPath + QLatin1String(".disabled")
							: FS::PathCombine(result.targetFolder,
											  result.fileName);
					const QString keep = firstInstalledMatch(
						installedGameDir, {gameRelPath, otherRel}, result.sha1,
						result.fileSize);
					if (!keep.isEmpty()) {
						/* Recorded under its on-disk name. */
						contentPaths.append(keep);
						reusedCount++;
						break;
					}
				}

				qDebug() << "Will download" << result.url << "to" << path;
				auto dl = Net::Download::makeFile(result.url, path);
				m_filesNetJob->addNetAction(dl);
				contentPaths.append(gameRelPath);
				break;
			}
			case Flame::File::Type::Modpack:
				logWarning(tr("Nesting modpacks in modpacks is not "
							  "implemented, nothing was downloaded: %1")
							   .arg(relpath));
				break;
			case Flame::File::Type::Cmod2:
				[[fallthrough]];
			case Flame::File::Type::Ctoc:
				[[fallthrough]];
			case Flame::File::Type::Unknown:
				logWarning(tr("Unrecognized/unhandled PackageType for: %1")
							   .arg(relpath));
				break;
		}
	}

	if (reusedCount > 0) {
		qDebug() << "Keeping" << reusedCount
				 << "file(s) already installed at the right version";
	} else if (canReuseInstalled && !sawAnyDigest) {
		/* Without a digest from the API every file is fetched again; logged
		 * because that is expensive. */
		qWarning() << "CurseForge returned no SHA-1 for any file in this "
					  "version; the whole pack will be downloaded again";
	}

	if (!blockedMods.isEmpty()) {
		if (LAUNCHER->uiHost()->resolveBlockedMods(
				tr("Restricted Mods"),
				tr("The following mods have restricted downloads "
				   "and are not available through the API.\n"
				   "Click the Download button next to each mod "
				   "to open its download page in your browser.\n"
				   "Once all files appear in your Downloads "
				   "folder, click Continue."),
				blockedMods)) {
			QString downloadDir = QStandardPaths::writableLocation(
				QStandardPaths::DownloadLocation);
			for (const auto& mod : blockedMods) {
				if (mod.found) {
					QString srcPath =
						FS::PathCombine(downloadDir, mod.fileName);
					QFileInfo targetInfo(mod.targetPath);
					QDir().mkpath(targetInfo.absolutePath());

					if (QFile::copy(srcPath, mod.targetPath)) {
						qDebug() << "Copied restricted mod:" << mod.fileName;
					} else {
						logWarning(tr("Failed to copy %1 from downloads folder")
									   .arg(mod.fileName));
					}
				}
			}
		} else {
			logWarning(tr("User cancelled restricted mod downloads - %1 mod(s) "
						  "will be missing")
						   .arg(blockedMods.size()));
		}
	}

	m_modIdResolver.reset();

	if (!recordPackContents(contentPaths)) {
		m_filesNetJob.reset();
		emitFailed(tr("Update cancelled."));
		return;
	}

	/* Stash file metadata for the sidecars, as in the Modrinth path; without
	 * them the update check skips CurseForge-installed mods. */
	startModDownload([sidecarMcPath =
						  FS::PathCombine(m_stagingPath, gameDirName()),
					  sidecarFiles = results.files]() {
		writeFlameModSidecars(sidecarMcPath, sidecarFiles);
	});
}

void InstanceImportTask::startModDownload(
	const std::function<void()>& writeSidecars)
{
	connect(m_filesNetJob.get(), &NetJob::succeeded, this,
			[this, writeSidecars]() {
				writeSidecars();
				m_filesNetJob.reset();
				/* Only the optional head start on the game's own files is
				 * left; it finishes the task either way. */
				downloadFiles(openStagedInstance());
			});
	connect(m_filesNetJob.get(), &NetJob::failed, [this](QString reason) {
		emitFailed(reason);
		m_filesNetJob.reset();
	});
	connect(m_filesNetJob.get(), &NetJob::progress,
			[this](qint64 current, qint64 total) {
				setProgress(current, total);
			});
	propagateStepsFrom(m_filesNetJob.get());
	setStatus(tr("Downloading mods..."));
	m_filesNetJob->start();
}

/* Returns what the overrides put into the game directory (game-relative).
 * Taken before the copy, the last moment they are distinguishable from what is
 * already there. */
static QStringList applyModrinthOverrides(const QString& stagingPath,
										  const QString& mcPath)
{
	QString overridePath = FS::PathCombine(stagingPath, "overrides");
	QString clientOverridePath =
		FS::PathCombine(stagingPath, "client-overrides");

	QStringList applied;

	if (QFile::exists(overridePath)) {
		applied += listFilesRelative(overridePath);
		if (!FS::copy(overridePath, mcPath)()) {
			qWarning() << "Could not apply overrides from the modpack.";
		}
		FS::deletePath(overridePath);
	}

	if (QFile::exists(clientOverridePath)) {
		applied += listFilesRelative(clientOverridePath);
		if (!FS::copy(clientOverridePath, mcPath)()) {
			qWarning() << "Could not apply client-overrides from the modpack.";
		}
		FS::deletePath(clientOverridePath);
	}

	applied.removeDuplicates();
	return applied;
}

void InstanceImportTask::processModrinth()
{
	if (!resolveUpdateTargetFromCatalogue()) {
		emitFailed(tr("Installation cancelled."));
		return;
	}

	Modrinth::Manifest pack;
	try {
		QString configPath =
			FS::PathCombine(m_stagingPath, "modrinth.index.json");
		Modrinth::loadManifest(pack, configPath);
		if (!QFile::remove(configPath)) {
			qWarning() << "Could not remove modrinth.index.json from staging";
		}
	} catch (const JSONValidationError& e) {
		emitFailed(tr("Could not understand Modrinth modpack manifest:\n") +
				   e.cause());
		return;
	}

	QString mcPath = FS::PathCombine(m_stagingPath, gameDirName());

	QDir mcDir(mcPath);
	if (!mcDir.exists()) {
		mcDir.mkpath(".");
	}

	m_packOverridePaths = applyModrinthOverrides(m_stagingPath, mcPath);

	/* Every source is known and the overrides are unpacked: ask before
	 * fetching anything. Before configuring the instance, so refusing leaves
	 * only the staging directory, which is removed on failure. */
	{
		QStringList suspect;
		for (const auto& file : pack.files) {
			/* Files unsupported on the client are never fetched, so they are
			 * not worth a warning. */
			if (!file.clientSupported) {
				continue;
			}
			if (!isKnownContentHost(file.downloadUrl,
									ContentSource::Modrinth)) {
				suspect.append(file.path);
			}
		}
		suspect += findBundledCode(mcPath);
		suspect.removeDuplicates();

		if (!confirmUntrustedFiles(suspect)) {
			emitFailed(tr("Installation cancelled: the modpack's untrusted "
						  "content was not accepted."));
			return;
		}
	}

	QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
	instanceSettings->registerSetting("InstanceType", "Legacy");
	instanceSettings->set("InstanceType", "OneSix");
	MinecraftInstance instance(m_globalSettings, instanceSettings,
							   m_stagingPath);

	auto components = instance.getPackProfile();
	components->buildingFromScratch();

	struct ModLoaderMapping {
		const QString& version;
		const char* componentId;
		const char* platformName;
	};
	const ModLoaderMapping loaders[] = {
		{pack.forgeVersion, "net.minecraftforge", "forge"},
		{pack.fabricVersion, "net.fabricmc.fabric-loader", "fabric"},
		{pack.quiltVersion, "org.quiltmc.quilt-loader", "quilt"},
		{pack.neoForgeVersion, "net.neoforged", "neoforge"},
	};

	if (!pack.minecraftVersion.isEmpty()) {
		components->setComponentVersion("net.minecraft", pack.minecraftVersion,
										true);
	}
	for (const auto& loader : loaders) {
		if (!loader.version.isEmpty()) {
			components->setComponentVersion(loader.componentId, loader.version);
		}
	}

	if (m_instIcon != "default") {
		instance.setIconKey(m_instIcon);
	} else {
		instance.setIconKey("modrinth");
	}

	instance.setName(m_instName);

	/* Persist the pack source as in the Flame path; `name` stands in for the
	 * slug (they usually match, and the attach UI fixes mismatches). */
	PackSourceHint hint = m_packHint;
	if (hint.provider.isEmpty()) {
		hint.provider = QStringLiteral("modrinth");
	}
	if (hint.versionId.isEmpty() && !pack.versionId.isEmpty()) {
		hint.versionId = pack.versionId;
	}
	if (hint.versionLabel.isEmpty() && !pack.versionId.isEmpty()) {
		hint.versionLabel = pack.versionId;
	}
	if (hint.packSlug.isEmpty() && !pack.name.isEmpty()) {
		hint.packSlug = pack.name;
	}
	if (hint.packName.isEmpty() && !pack.name.isEmpty()) {
		hint.packName = pack.name;
	}
	writePackSourceToInstance(instance, hint);

	/* Tells Modrinth these downloads are for a pack ("modpack" or "update"),
	 * not individual mods, for the authors' download counts. */
	Net::ModrinthDownloadMeta downloadMeta;
	downloadMeta.reason = m_updateTarget.isEmpty() ? QStringLiteral("modpack")
												   : QStringLiteral("update");
	downloadMeta.gameVersion = pack.minecraftVersion;
	for (const auto& loader : loaders) {
		if (!loader.version.isEmpty()) {
			downloadMeta.loader = QLatin1String(loader.platformName);
			break;
		}
	}
	downloadMeta.dependentOn = m_updateTarget.versionId.isEmpty()
								   ? pack.versionId
								   : m_updateTarget.versionId;

	m_filesNetJob =
		new NetJob(tr("Modrinth mod download"), LAUNCHER->network());
	auto minecraftDir = FS::PathCombine(m_stagingPath, gameDirName());
	auto canonicalBase = QDir(minecraftDir).canonicalPath();
	QStringList contentPaths = m_packOverridePaths;

	/* On an update, skip files already installed with the right hash. */
	QDir installedGameDir;
	bool canReuseInstalled = false;
	if (!m_updateTarget.isEmpty()) {
		auto previous = LAUNCHER->instances()->getInstanceById(
			m_updateTarget.instanceId);
		if (auto minecraftPrevious =
				std::dynamic_pointer_cast<MinecraftInstance>(previous)) {
			installedGameDir = QDir(minecraftPrevious->gameRoot());
			canReuseInstalled = true;
			setStatus(tr("Checking files already installed..."));
		}
	}
	int reusedCount = 0;

	for (auto& file : pack.files) {
		if (file.path.contains("..") || QDir::isAbsolutePath(file.path)) {
			qWarning() << "Skipping potentially malicious file path:"
					   << file.path;
			continue;
		}
		if (!file.clientSupported) {
			/* Client-unsupported files are left out, not installed disabled: a
			 * server-only jar under ".disabled" is a file the user never asked
			 * for. */
			qDebug() << "Skipping file the pack marks unsupported on the "
						"client:"
					 << file.path;
			continue;
		}

		/* An optional file is installed disabled, as in the CurseForge path,
		 * so exported-then-imported instances keep their disabled mods. */
		const QString targetPath =
			file.required ? file.path
						  : file.path + QLatin1String(".disabled");

		auto path = FS::PathCombine(minecraftDir, targetPath);
		auto canonicalDir = QFileInfo(path).absolutePath();
		if (!canonicalDir.startsWith(canonicalBase)) {
			qWarning() << "Skipping file path that escapes staging directory:"
					   << file.path;
			continue;
		}
		if (!file.downloadUrl.isValid() || file.downloadUrl.isEmpty()) {
			logWarning(
				tr("Skipping file with no download URL: %1").arg(file.path));
			continue;
		}
		if (canReuseInstalled) {
			const QString keep = firstInstalledMatch(
				installedGameDir,
				{file.path, file.path + QLatin1String(".disabled")}, file.sha1,
				file.fileSize);
			if (!keep.isEmpty()) {
				/* Recorded under its on-disk name. */
				contentPaths.append(keep);
				reusedCount++;
				continue;
			}
		}

		qDebug() << "Will download" << file.downloadUrl << "to" << path;
		auto dl = Net::Download::makeFile(file.downloadUrl, path);
		dl->setModrinthDownloadMeta(downloadMeta);
		m_filesNetJob->addNetAction(dl);
		contentPaths.append(targetPath);
	}

	if (reusedCount > 0) {
		qDebug() << "Keeping" << reusedCount
				 << "file(s) already installed at the right version";
	}

	if (!recordPackContents(contentPaths)) {
		m_filesNetJob.reset();
		emitFailed(tr("Update cancelled."));
		return;
	}

	/* Copy of the pack files and the staged .minecraft path, so the
	 * post-download lambda can write sidecars without a stale stack frame;
	 * without sidecars update detection does nothing. */
	startModDownload([sidecarMcPath = minecraftDir,
					  sidecarFiles = pack.files]() {
		writeModrinthModSidecars(sidecarMcPath, sidecarFiles);
	});
}

void InstanceImportTask::processTechnic()
{
	shared_qobject_ptr<Technic::TechnicPackProcessor> packProcessor =
		new Technic::TechnicPackProcessor();

	/* The processor works inside run() and reports through these signals;
	 * handlers only record the outcome. It builds the staged instance.cfg
	 * through its own settings object, which rewrites the whole file on every
	 * change, so no second object may write to it while this one lives. */
	bool processed = false;
	bool reportedFailure = false;
	connect(packProcessor.get(), &Technic::TechnicPackProcessor::succeeded,
			this, [&processed]() { processed = true; });
	connect(packProcessor.get(), &Technic::TechnicPackProcessor::failed, this,
			[this, &reportedFailure](QString reason) {
				reportedFailure = true;
				emitFailed(reason);
			});
	packProcessor->run(m_globalSettings, m_instName, m_instIcon, m_stagingPath);

	if (!processed || reportedFailure) {
		/* Either the processor failed and said why, or it finished without
		 * reporting; neither counts as an install. */
		if (!processed && !reportedFailure) {
			emitFailed(tr("The Technic pack could not be processed."));
		}
		return;
	}

	/* A Technic pack has no manifest: the archive is the file list, so the
	 * game directory holds exactly what the pack shipped, which a later update
	 * needs. */
	if (!recordPackContents(listFilesRelative(stagedGameDir()))) {
		emitFailed(tr("Installation cancelled."));
		return;
	}

	/* As in the Kite path: reachable as an "update from file" of a managed
	 * instance, whose live instance.cfg the staged one replaces, so the pack
	 * identity must be written. The instance is reopened as a NullInstance
	 * (the processor's is gone); only its settings object is needed, and
	 * unregistered keys in the file are kept. */
	QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
	NullInstance instance(m_globalSettings, instanceSettings, m_stagingPath);
	writePackSourceToInstance(instance, m_packHint);

	emitSucceeded();
}

void InstanceImportTask::processKite()
{
	QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
	instanceSettings->registerSetting("InstanceType", "Legacy");

	NullInstance instance(m_globalSettings, instanceSettings, m_stagingPath);

	instance.resetTimePlayed();

	instance.setName(m_instName);

	if (m_instIcon != "default") {
		instance.setIconKey(m_instIcon);
	} else {
		m_instIcon = instance.iconKey();

		auto importIconPath =
			IconUtils::findBestIconIn(instance.instanceRoot(), m_instIcon);
		if (!importIconPath.isNull() && QFile::exists(importIconPath)) {
			auto iconList = LAUNCHER->icons();
			if (iconList->iconFileExists(m_instIcon)) {
				iconList->deleteIcon(m_instIcon);
			}
			iconList->installIcons({importIconPath});
		}
	}

	/* A Kite pack is its own file list. */
	if (!recordPackContents(listFilesRelative(stagedGameDir()))) {
		emitFailed(tr("Installation cancelled."));
		return;
	}

	/* A Kite archive carries no catalogue identity, but this is reachable as
	 * an "update from file", and the staged instance.cfg replaces the live
	 * one, so whatever the caller knew must be written. A plain import has no
	 * hint: a no-op. */
	writePackSourceToInstance(instance, m_packHint);

	emitSucceeded();
}

/* Hosts a pack's downloads may name without asking the user: only the two
 * catalogues' content networks. Anything else may be innocent, but the game
 * loads the file as code, so the user decides. */
static bool isKnownContentHost(const QUrl& url, ContentSource source)
{
	if (url.scheme() != QLatin1String("https")) {
		/* Plain http can be swapped in transit. */
		return false;
	}

	const QString host = url.host().toLower();
	switch (source) {
		case ContentSource::Modrinth:
			/* The mrpack format allows a few other hosts (e.g. a GitHub
			 * release); refusing them would warn on packs Modrinth accepts.
			 * See ModrinthApi::mrpackHosts(). */
			return ModrinthApi::mrpackHosts().contains(host);

		case ContentSource::CurseForge:
			/* CurseForge serves from numbered edge nodes under this domain:
			 * matched by suffix, anchored with the dot so "notforgecdn.net"
			 * cannot pass. */
			if (host == QLatin1String("forgecdn.net") ||
				host.endsWith(QLatin1String(".forgecdn.net"))) {
				return true;
			}
			/* The site's own download route, where files the API will not
			 * serve are sent. */
			return host == FlameApi::siteHost();
	}
	return false;
}

QStringList
InstanceImportTask::findBundledCode(const QString& minecraftDir) const
{
	QStringList found;

	/* Where the game loads code from. Resource and shader packs are data and
	 * would make the warning noise. */
	const QStringList codeFolders = {
		QStringLiteral("mods"), QStringLiteral("coremods"),
		QStringLiteral("jarmods"), QStringLiteral("plugins")};

	const QDir root(minecraftDir);
	for (const QString& folder : codeFolders) {
		const QString path = FS::PathCombine(minecraftDir, folder);
		if (!QFileInfo(path).isDir()) {
			continue;
		}

		/* Everything in "mods" is listed whatever its name: the loader, not
		 * the extension, decides what is loaded, so filtering on ".jar" would
		 * let a pack smuggle code past this list. The other folders only hold
		 * jars. */
		const bool listEverything = folder == QLatin1String("mods");

		QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const QString file = it.next();
			/* ".jar.disabled" counts: it is one rename from being loaded. */
			if (listEverything ||
				file.endsWith(QLatin1String(".jar"), Qt::CaseInsensitive) ||
				file.endsWith(QLatin1String(".jar.disabled"),
							  Qt::CaseInsensitive)) {
				found.append(root.relativeFilePath(file));
			}
		}
	}

	found.sort();
	return found;
}

MinecraftInstance* InstanceImportTask::openStagedInstance()
{
	const QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	if (!QFileInfo::exists(configPath)) {
		qWarning() << "No instance.cfg in" << m_stagingPath
				   << "- not pre-downloading game files";
		return nullptr;
	}

	/* Opened fresh: two INI settings objects over one instance.cfg lose keys
	 * (each rewrites the whole file), so the first was let go; that also
	 * flushed the pack profile read next. */
	auto settings = std::make_shared<INISettingsObject>(configPath);
	settings->registerSetting("InstanceType", "Legacy");
	m_gameFilesInstance = std::make_shared<MinecraftInstance>(
		m_globalSettings, settings, m_stagingPath);
	return m_gameFilesInstance.get();
}

bool InstanceImportTask::confirmUntrustedFiles(const QStringList& suspectPaths)
{
	if (m_trustedSource || suspectPaths.isEmpty()) {
		return true;
	}

	qWarning() << "Untrusted modpack carries" << suspectPaths.size()
			   << "file(s) we cannot vouch for";

	return LAUNCHER->uiHost()->confirmUntrustedMods(suspectPaths);
}

bool InstanceImportTask::resolveUpdateTargetFromCatalogue()
{
	if (!m_updateTarget.isEmpty()) {
		/* The pack page already named the instance to replace. */
		return true;
	}
	if (m_packHint.provider.isEmpty() || m_packHint.packId.isEmpty()) {
		/* Not a catalogue install: nothing to match on. */
		return true;
	}
	if (LAUNCHER->settings()->get("SkipModpackUpdatePrompt").toBool()) {
		/* Switched off: install as a second instance without asking. Checked
		 * before any lookup so it costs nothing. */
		return true;
	}

	auto existing = LAUNCHER->instances()->getInstanceByManagedPack(
		m_packHint.provider, m_packHint.packId);
	if (!existing) {
		return true;
	}

	const QString installedVersion = existing->managedPackVersionName();
	const QString versionSuffix =
		installedVersion.isEmpty()
			? QString()
			: tr(", at version %1").arg(installedVersion);

	const int choice = LAUNCHER->uiHost()->choose(
		tr("This modpack is already installed"),
		tr("The instance \"%1\" was installed from this modpack%2.\n\n"
		   "Updating it replaces the pack's own files and keeps everything "
		   "that is yours: worlds, screenshots, play time and the "
		   "instance's settings. Creating a separate instance leaves it "
		   "untouched.\n\n"
		   "Back up anything you cannot afford to lose before updating. An "
		   "update replaces the pack's configuration files, and a pack that "
		   "changes or removes mods can leave worlds made with the older "
		   "version unusable.")
			.arg(existing->name(), versionSuffix),
		UiHost::Severity::Question,
		{tr("Update existing instance"), tr("Create separate instance")});

	if (choice == 0) {
		/* The version fields are the catalogue entry the user picked: the
		 * instance must claim the version it now has. */
		UpdateTarget target;
		target.instanceId = existing->id();
		target.versionId = m_packHint.versionId;
		target.versionLabel = m_packHint.versionLabel;
		setUpdateTarget(target);
		qDebug() << "Installing over existing instance" << target.instanceId;
		return true;
	}
	if (choice == 1) {
		return true;
	}

	/* Cancel or a dismissed dialog means do nothing at all. */
	return false;
}

QString InstanceImportTask::gameDirName()
{
	if (!m_gameDirName.isEmpty()) {
		return m_gameDirName;
	}

	/* What the importer has always staged; also the answer for anything that
	 * is not an update. */
	m_gameDirName = QStringLiteral("minecraft");

	if (m_updateTarget.isEmpty()) {
		return m_gameDirName;
	}

	auto previous =
		LAUNCHER->instances()->getInstanceById(m_updateTarget.instanceId);
	auto minecraftPrevious =
		std::dynamic_pointer_cast<MinecraftInstance>(previous);
	if (!minecraftPrevious) {
		return m_gameDirName;
	}

	const QString existing =
		QFileInfo(minecraftPrevious->gameRoot()).fileName();
	if (!existing.isEmpty()) {
		m_gameDirName = existing;
	}
	return m_gameDirName;
}

QString InstanceImportTask::stagedGameDir()
{
	/* Same rule as MinecraftInstance::gameRoot(), so this reads what the
	 * instance will call its game directory. */
	const QString minecraft = FS::PathCombine(m_stagingPath, "minecraft");
	const QString dotMinecraft = FS::PathCombine(m_stagingPath, ".minecraft");

	if (QFileInfo(minecraft).isDir() && !QFileInfo::exists(dotMinecraft)) {
		return minecraft;
	}
	if (QFileInfo(dotMinecraft).isDir()) {
		return dotMinecraft;
	}
	/* Neither exists: nothing to enumerate. */
	return FS::PathCombine(m_stagingPath, gameDirName());
}

/* The sidecar recording where the mod at @p gameRelativePath came from, or
 * empty. @p indexes caches one loaded index per folder, since one file's
 * answer needs every sidecar in its folder. Only folders that can carry
 * sidecars are looked at, as in sidecarFolderForPath(). */
static QString sidecarPathForModFile(
	QHash<QString, std::shared_ptr<ModMetadataIndex>>& indexes,
	const QDir& gameDir, const QString& gameRelativePath)
{
	const QString folder =
		sidecarFolderForPath(gameDir.absolutePath(), gameRelativePath);
	if (folder.isEmpty()) {
		return {};
	}

	auto& index = indexes[folder];
	if (!index) {
		QDir folderDir(folder);
		index = std::make_shared<ModMetadataIndex>(folderDir);
		index->load();
	}
	return index->sidecarPathFor(QFileInfo(gameRelativePath).fileName());
}

/* Asked, not decided: a world cannot be downloaded again, and the pack cannot
 * know whether the copy on disk is still the shipped one. */
static bool askAboutDeletingSaves()
{
	return LAUNCHER->uiHost()->confirm(
		QObject::tr("Delete existing save files"),
		QObject::tr("The installed version of this modpack came with save "
					"files that the new version no longer includes.\n\n"
					"Would you like to remove them as part of this update? "
					"Keeping them is safe - they simply stay where they "
					"are, along with any progress made in them."),
		UiHost::Severity::Question, QObject::tr("Remove saves"),
		QObject::tr("Keep saves"));
}

bool InstanceImportTask::recordPackContents(
	const QStringList& gameRelativePaths)
{
	/* Written into staging so it travels with the files it describes: the
	 * commit's merge replaces the old list together with the old files. */
	if (!PackContents::write(m_stagingPath, gameRelativePaths)) {
		/* Not fatal: only the next update's cleanup is lost, which beats
		 * refusing an otherwise good pack. */
		qWarning() << "Could not record pack contents in" << m_stagingPath;
	}

	if (m_updateTarget.isEmpty()) {
		return true;
	}

	auto previous =
		LAUNCHER->instances()->getInstanceById(m_updateTarget.instanceId);
	if (!previous) {
		/* Gone since the page opened; nothing to clean up here. */
		qWarning() << "Cannot diff pack contents against missing instance"
				   << m_updateTarget.instanceId;
		return true;
	}

	auto minecraftPrevious =
		std::dynamic_pointer_cast<MinecraftInstance>(previous);
	if (!minecraftPrevious) {
		/* Recorded paths are relative to a game directory, which only a
		 * Minecraft instance has; do nothing rather than guess. */
		qWarning() << "Not diffing pack contents:" << previous->id()
				   << "has no game directory";
		return true;
	}

	QStringList installedPaths;
	if (!PackContents::read(previous->instanceRoot(), installedPaths)) {
		/* Without the list the pack's files cannot be told from the user's, so
		 * the update goes ahead without cleanup and says so. */
		return LAUNCHER->uiHost()->confirm(
			tr("No file list for the installed version"),
			tr("The launcher has no record of which files the installed "
			   "version of this modpack put into this instance, so it "
			   "cannot remove the ones the new version no longer "
			   "ships.\n\n"
			   "The update will work, but anything the new version dropped "
			   "stays behind - usually as an older copy of a mod sitting "
			   "next to the one that replaced it.\n\n"
			   "Instances installed before the launcher started keeping "
			   "that record have no list. This update writes one, so the "
			   "update after it will be able to clean up."),
			UiHost::Severity::Warning, tr("Update anyway"), tr("Cancel"));
	}

	const QStringList stale =
		PackContents::staleEntries(installedPaths, gameRelativePaths);
	if (stale.isEmpty()) {
		return true;
	}

	const QDir gameDir(minecraftPrevious->gameRoot());
	/* One index per folder for the whole loop: reading a folder's sidecars
	 * reads every file in its .index. */
	QHash<QString, std::shared_ptr<ModMetadataIndex>> indexes;
	for (const QString& relativePath : stale) {
		const QString absolutePath = gameDir.absoluteFilePath(relativePath);

		/* Checked before asking: most of this list is names never on disk
		 * (disabled counterparts, blocked downloads), and a prompt about an
		 * absent world would be pointless. */
		if (!QFileInfo::exists(absolutePath)) {
			continue;
		}

		/* Case-insensitive: this is a guard, so a differently spelled folder
		 * should still trip it. */
		if (relativePath.startsWith(QLatin1String("saves/"),
									Qt::CaseInsensitive)) {
			if (m_savesDeletion == SavesDeletion::NotAsked) {
				m_savesDeletion = askAboutDeletingSaves()
									  ? SavesDeletion::Allowed
									  : SavesDeletion::Refused;
			}
			if (m_savesDeletion == SavesDeletion::Refused) {
				qDebug() << "Keeping save file the update dropped:"
						 << relativePath;
				continue;
			}
		}

		scheduleForRemoval(absolutePath);

		/* And the sidecar for that file: it describes exactly one file, and
		 * left behind it would still claim an installed mod. */
		const QString sidecar =
			sidecarPathForModFile(indexes, gameDir, relativePath);
		if (!sidecar.isEmpty()) {
			scheduleForRemoval(sidecar);
		}
	}

	return true;
}

void InstanceImportTask::carryOverUserSettings(BaseInstance& instance)
{
	if (m_updateTarget.isEmpty()) {
		return;
	}

	auto previous =
		LAUNCHER->instances()->getInstanceById(m_updateTarget.instanceId);
	if (!previous) {
		/* The instance vanished since the page opened; the staging step fails
		 * too. */
		qWarning() << "Cannot carry settings over from missing instance"
				   << m_updateTarget.instanceId;
		return;
	}

	auto from = previous->settings();
	auto to = instance.settings();

	/* Settings that record what the user did with this instance, not what the
	 * pack is; an update must not reset them. Listed explicitly: copying
	 * wholesale would restore the old pack's component versions and undo the
	 * update. */
	static const char* const carried[] = {
		"notes",
		"lastLaunchTime",
		"totalTimePlayed",
		"lastTimePlayed",
		"shortcuts",
		"Profiler",
		/* Per-instance overrides: each "override this?" flag travels with the
		 * values it guards. */
		"OverrideCommands",
		"PreLaunchCommand",
		"WrapperCommand",
		"PostExitCommand",
		"OverrideConsole",
		"ShowConsole",
		"ShowConsoleOnError",
		"LogPrePostOutput",
	};

	for (const char* key : carried) {
		if (!from->contains(key) || !to->contains(key)) {
			/* Older config without the key, or a setting this instance type
			 * does not register: the staged default is right. */
			continue;
		}
		to->set(key, from->get(key));
	}
}

void InstanceImportTask::writePackSourceToInstance(BaseInstance& instance,
												   const PackSourceHint& hint)
{
	/* Carry user settings over once the updated instance is built. */
	carryOverUserSettings(instance);

	if (hint.isEmpty())
		return;

	/* These keys mirror what PackUpdater reads back. Empty fields are written
	 * explicitly: absent means "never wrote a hint", present-but-empty means
	 * "could not recover this one". */
	/* On an update, the installed version is more trustworthy than the
	 * archive's manifest, which can disagree with the catalogue about its own
	 * version id. Override only with a real value; an update from a local file
	 * has none. */
	QString versionId = hint.versionId;
	QString versionLabel = hint.versionLabel;
	if (!m_updateTarget.isEmpty()) {
		if (!m_updateTarget.versionId.isEmpty()) {
			versionId = m_updateTarget.versionId;
		}
		if (!m_updateTarget.versionLabel.isEmpty()) {
			versionLabel = m_updateTarget.versionLabel;
		}
	}

	auto s = instance.settings();
	s->set("PackProvider", hint.provider);
	s->set("PackId", hint.packId);
	s->set("PackSlug", hint.packSlug);
	s->set("PackName", hint.packName);
	s->set("PackVersionId", versionId);
	s->set("PackVersionLabel", versionLabel);
	s->set("PackIconUrl", hint.iconUrl);
	s->set("PackSourceUrl", hint.sourceUrl);
	s->set("PackInstalledAt",
		   QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
}
