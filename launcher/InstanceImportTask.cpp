/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#include <QCryptographicHash>
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
		// Show the file being fetched as its own line in the dialog.
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
		/* Stopping on request arrives here too: a NetJob told to abort
		 * reports it as a failure. */
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

	// Run ALL archive scanning in a background thread so the UI stays
	// responsive. We also do a single listEntries() pass instead of
	// calling findFolderOfFileInZip() / entryExists() separately (each of
	// which would re-open the archive, and findFolderOfFileInZip is
	// recursive so it opens the archive once per directory level).
	const QString archivePath = m_archivePath;
	const QString stagingPath = m_stagingPath;

	m_detectFuture = QtConcurrent::run([archivePath, stagingPath]() {
		DetectResult result;
		result.extractTarget = stagingPath;

		// Single archive open + single linear scan over all entries.
		const QStringList entries = MMCZip::listEntries(archivePath);

		// Helper: find the folder prefix (e.g. "root/") for the first
		// entry whose filename component equals `needle`.
		// Returns "" (empty, non-null) if found at root.
		// Returns QString() (null) if not found.
		auto findFolder = [&entries](const QString& needle) -> QString {
			for (const QString& e : entries) {
				if (e == needle)
					return QLatin1String(""); // found at root
				if (e.endsWith(QLatin1Char('/') + needle)) {
					return e.left(e.lastIndexOf(QLatin1Char('/')) + 1);
				}
			}
			return {}; // null → not found
		};

		result.mmcRoot = findFolder(QStringLiteral("instance.cfg"));
		result.modrinthRoot = findFolder(QStringLiteral("modrinth.index.json"));
		result.flameRoot = findFolder(QStringLiteral("manifest.json"));
		result.technicFound =
			entries.contains(QStringLiteral("bin/modpack.jar")) ||
			entries.contains(QStringLiteral("bin/version.json")) ||
			entries.contains(QStringLiteral("bin/modpack.jar/")) ||
			entries.contains(QStringLiteral("bin/version.json/"));

		// For Technic, create the target subdir now (background thread is
		// fine for filesystem ops).
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
		/* Inspecting the archive cannot be interrupted, so an abort
		 * during it is reported here, once the scan has run itself
		 * out. */
		emitAborted();
		return;
	}

	const DetectResult r = m_detectFuture.result();

	// Determine pack type (same priority order as before).
	QString root;
	if (!r.mmcRoot.isNull()) {
		qDebug() << "MeshMC:" << r.mmcRoot;
		m_modpackType = ModpackType::MeshMC;
		root = r.mmcRoot;
	} else if (!r.modrinthRoot.isNull()) {
		qDebug() << "Modrinth:" << r.modrinthRoot;
		m_modpackType = ModpackType::Modrinth;
		root = r.modrinthRoot;
	} else if (r.technicFound) {
		qDebug() << "Technic";
		m_modpackType = ModpackType::Technic;
		/* root stays empty — an empty prefix means the whole archive,
		 * which is what a Technic pack needs: it ships its files at the
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

	/* There is no separate "aborted" signal to listen for: a task that
	 * stops on request reports it as a failure whose reason says so. The
	 * flag latched in abort() is what tells the two apart. */
	connect(extract, &Task::succeeded, this,
			&InstanceImportTask::extractFinished);
	connect(extract, &Task::failed, this, [this](const QString& reason) {
		m_extractTask.reset();
		if (m_aborted) {
			emitAborted();
			return;
		}
		/* Logged rather than shown: extractFailed() has a better thing
		 * to say to the user than "Unsupported ZIP compression method",
		 * and it also has to decide what to do with the archive. */
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
		/* Already stopping. The verdict is on its way from whichever
		 * step is in flight; a second one would be a second finished()
		 * for the same task. */
		return true;
	}
	m_aborted = true;

	/* Whichever step is running is told to stop, and its own handler
	 * reports the abort. Only one of them can be live at a time, but all
	 * are checked rather than funnelled through a single "current task"
	 * pointer: a step added later that forgot to register itself there
	 * would be a step during which aborting silently did nothing, which
	 * is the bug this whole change is here to fix. */
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
		/* Inspecting the archive is one pass over its entry list and
		 * cannot be interrupted, but it is seconds at worst.
		 * detectFinished() sees the flag and stops there. */
		return true;
	}

	emitAborted();
	return true;
}

/* The archive could not be unpacked. Not reachable by aborting - that is
 * handled where the flag is checked - so this is always a real failure. */
void InstanceImportTask::extractFailed()
{
	/* Only a download we cached is ours to throw away - a file the user
	 * dragged in belongs to them, and deleting it because we could not
	 * read it would be unforgivable. */
	if (m_archiveEntry) {
		qWarning() << "Discarding unusable cached archive" << m_archivePath;
		/* The file itself, not just the entry's freshness: a stale entry
		 * whose file still exists makes the next request conditional,
		 * the server answers 304, and we are back to the same damaged
		 * bytes. */
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
			// Folder +rwx for current user
			permissions |= QFileDevice::Permission::ReadUser |
						   QFileDevice::Permission::WriteUser |
						   QFileDevice::Permission::ExeUser;
		} else {
			// File +rw for current user
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
		case ModpackType::MeshMC:
			processMeshMC();
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
	/* Walk back from a Modrinth CDN download URL to what the catalogue
	 * calls the file. The CDN path shape is stable:
	 *
	 *   https://cdn.modrinth.com/data/{projectId}/versions/{number}/{file}
	 *
	 * The project ID is the bare base62 id, not the human slug. The
	 * segment after "versions" is the version *number* - the human
	 * "1.1.1+1.17" - and not the version id, which does not appear in the
	 * URL at all.
	 *
	 * Either string comes back empty when parsing fails, which makes the
	 * caller skip that part of the sidecar. */
	struct ModrinthIds {
		QString projectId;
		/* Named for what it is on purpose.
		 *
		 * Read as a version id, this string broke everything downstream
		 * that believed it: dependency resolution asked the version
		 * endpoint for a name it has never heard of and took the 404 for
		 * "this mod needs nothing", and the update check compared it
		 * against real version ids, never matched, and offered every
		 * modpack-installed mod an update to itself. */
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

	/* Map a pack-relative `path` (e.g. "mods/sodium.jar") to the
	 * containing folder of the file on disk. Returns empty when the
	 * path doesn't sit under one of the indexable mod-like folders
	 * — there's no sidecar story for arbitrary `config/foo.toml`
	 * files. */
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
				/* Never installed on a client - see processModrinth - so
				 * there is no file here for a sidecar to describe. Worth
				 * skipping explicitly rather than leaving to chance:
				 * orphaned `.pw.toml` files are deliberately never
				 * pruned, because in a packwiz pack the sidecar is the
				 * definition and the file beside it may simply not have
				 * been downloaded yet. One written here would stay for
				 * good, claiming a mod the instance does not have. */
				continue;
			}
			const QString folder = sidecarFolderForPath(minecraftDir, f.path);
			if (folder.isEmpty())
				continue;
			const ModrinthIds ids = parseModrinthCdnUrl(f.downloadUrl);
			if (ids.projectId.isEmpty())
				continue;

			/* `QDir folderDir(folder)` (instead of the would-be
			 * vexing-parse `ModMetadataIndex idx(QDir(folder))`).
			 * Same end result, no ambiguity for the compiler. */
			QDir folderDir(folder);
			ModMetadataIndex idx(folderDir);
			ModMetadataIndex::Entry e;
			e.fileName = QFileInfo(f.path).fileName();
			e.platform = QStringLiteral("modrinth");
			e.projectId = ids.projectId;
			/* No version id: an mrpack does not contain one and nothing
			 * here can invent it. What identifies this exact file is its
			 * hash, recorded below, and that is what the update check and
			 * the dependency resolver fall back to. The version *number*
			 * - all the URL offers - goes in the field that means it,
			 * where it can be shown to the user without being sent
			 * somewhere an id is expected. */
			e.versionNumber = ids.versionNumber;
			/* mrpack manifests don't carry per-file display names;
			 * the file name is what the user sees in the mod list
			 * anyway. */
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
			/* `targetFolder` is whatever the manifest declared —
			 * usually "mods" but resourcepacks/shaderpacks may show
			 * up too. */
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

	/* Whether the file at @p path is already exactly the file the pack
	 * wants there.
	 *
	 * Used to skip re-downloading what an update does not change, which
	 * for a modpack is most of it: two versions of a pack usually differ
	 * by a handful of mods out of hundreds, and the rest is megabytes we
	 * would fetch only to write the same bytes back.
	 *
	 * The digest is read off the disk rather than compared between the
	 * two manifests. It costs a pass over files we are keeping, which is
	 * still far cheaper than downloading them, and it answers the
	 * question that actually matters: is the file *there* the right one.
	 * A manifest-to-manifest comparison would happily keep a jar the user
	 * has since replaced, or one that a half-finished earlier update left
	 * truncated. */
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
		/* Cheap disqualifier first: a stat call rules out most changed
		 * files without reading them. */
		if (expectedSize > 0 && info.size() != expectedSize) {
			return false;
		}

		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return false;
		}
		QCryptographicHash hash(QCryptographicHash::Sha1);
		if (!hash.addData(&file)) {
			return false;
		}
		return QString::fromLatin1(hash.result().toHex())
				   .compare(sha1, Qt::CaseInsensitive) == 0;
	}

	/* Every file under `dir`, as paths relative to it. */
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
	/* First, because everything below depends on whether this install is
	 * replacing an instance - starting with the name of the directory the
	 * overrides are about to be moved into. */
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

	/* The pack's overrides *are* the game directory at this point - the
	 * folder was renamed into place above and nothing has been downloaded
	 * into it yet - so this is the one moment where they can be told
	 * apart from the manifest's files. Read after configuring the
	 * instance, which is what moves any bundled jar mods out into the
	 * component patches, so files that are no longer there are not
	 * recorded as if they were. */
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

/* Which catalogue a set of downloads is supposed to be coming from.
 *
 * The two are kept apart on purpose: a Modrinth pack naming a CurseForge
 * CDN (or the other way round) is not a normal thing for a pack to do,
 * and treating either catalogue's network as trusted everywhere would
 * hand any pack a host it can use without being questioned. */
enum class ContentSource { Modrinth, CurseForge };

/* Defined further down, next to the reasoning about which hosts are
 * trusted; declared here because both manifest processors ask. */
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
	// default to something other than the MeshMC default to distinguish these
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
	// Hack to correct some 'special sauce'...
	if (mcVersion.endsWith('.')) {
		mcVersion.remove(QRegularExpression("[.]+$"));
		logWarning(tr("Mysterious trailing dots removed from Minecraft version "
					  "while importing pack."));
	}
	auto components = instance.getPackProfile();
	components->buildingFromScratch();
	components->setComponentVersion("net.minecraft", mcVersion, true);

	// Handle Forge "recommended" version mapping
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
		// install all the jar mods
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
		// nuke the original files
		FS::deletePath(jarmodsPath);
	}
	instance.setName(m_instName);

	/* Persist the pack-source hint so PackUpdater (and any other
	 * plugin) can read it back through instance_setting_get without
	 * sniffing the manifest. Prefer the hint set by the browser UI;
	 * fall back to whatever we can recover from the manifest. */
	PackSourceHint hint = m_packHint;
	if (hint.provider.isEmpty()) {
		hint.provider = QStringLiteral("curseforge");
	}
	if (hint.versionLabel.isEmpty() && !pack.version.isEmpty()) {
		hint.versionLabel = pack.version;
	}
	if (hint.packSlug.isEmpty() && !pack.name.isEmpty()) {
		/* CurseForge manifests don't carry the URL slug; the pack
		 * name is the best we have for display until the user
		 * (re)attaches through the plugin's UI. */
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

	/* Same check as the Modrinth path, at the equivalent moment: the
	 * resolver has just turned the manifest's project/file ids into real
	 * download URLs, so this is the first point at which we know where
	 * every mod would come from - and it is still before anything has
	 * been fetched.
	 *
	 * Files the resolver could not resolve are left out: they have no URL
	 * to judge, and they are handled separately as blocked mods, where
	 * the user fetches them from CurseForge by hand and therefore sees
	 * exactly what they are getting. */
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

	// Collect restricted mods that need browser download
	QList<BlockedMod> blockedMods;

	/* What this version is responsible for, game-relative. Built from
	 * this loop rather than from the manifest, so that it says what the
	 * install actually does - including the ".disabled" suffix an
	 * optional file gets, and excluding the entries we skip. */
	QStringList contentPaths = m_packOverridePaths;

	/* On an update, what is already installed is worth looking at before
	 * fetching anything - see the Modrinth path, which does the same for
	 * the same reason. A CurseForge pack is where it matters most: they
	 * are the large ones, and re-fetching a two-gigabyte pack to change
	 * three mods is twenty minutes of somebody's connection spent
	 * producing bytes they already had.
	 *
	 * Skipping a download leaves that path empty in staging, which is
	 * what makes this safe: the commit step merges staging over the live
	 * instance, so a path nothing was staged for keeps the file that is
	 * already there. */
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
					/* Recorded even though we are not the ones fetching
					 * it: the pack asked for this file, and if the user
					 * does hand it over it belongs to the pack exactly
					 * like the rest. A path that ends up with no file
					 * behind it costs nothing - the removal step skips
					 * what is not there. */
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
					/* Both names the file could be under. A mod the user
					 * turned off is still the file the pack asked for,
					 * and fetching the enabled copy next to it would
					 * both waste the download and quietly turn the mod
					 * back on. The name the pack wants is tried first,
					 * so an unchanged install stays byte-for-byte as it
					 * was. */
					const QString otherRel =
						result.required
							? gameRelPath + QLatin1String(".disabled")
							: FS::PathCombine(result.targetFolder,
											  result.fileName);
					QString keep;
					for (const QString& candidate : {gameRelPath, otherRel}) {
						if (fileMatchesHash(
								installedGameDir.absoluteFilePath(candidate),
								result.sha1, result.fileSize)) {
							keep = candidate;
							break;
						}
					}
					if (!keep.isEmpty()) {
						/* Recorded under the name it actually has on
						 * disk, since that is the file this version is
						 * responsible for. */
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
		/* Worth saying out loud rather than just being slow: without a
		 * digest from the API there is no way to prove a file on disk is
		 * the one the pack wants, so every file is fetched again. That is
		 * correct but expensive, and if it ever becomes the normal case
		 * the log is where it will show up. */
		qWarning() << "CurseForge returned no SHA-1 for any file in this "
					  "version; the whole pack will be downloaded again";
	}

	// Handle restricted mods via dialog
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

	/* Same rationale as the Modrinth path: stash file metadata so
	 * the post-download lambda can drop ModMetadataIndex sidecars
	 * into `mods/.index/` etc. Without these, the launcher's
	 * "Check for Updates" pass under the Mods tab silently skips
	 * every CurseForge-installed mod. */
	const QString sidecarMcPath = FS::PathCombine(m_stagingPath, gameDirName());
	const QVector<Flame::File> sidecarFiles = results.files;

	connect(m_filesNetJob.get(), &NetJob::succeeded, this,
			[this, sidecarMcPath, sidecarFiles]() {
				writeFlameModSidecars(sidecarMcPath, sidecarFiles);
				m_filesNetJob.reset();
				/* The pack is complete; the only thing left is the
				 * optional head start on the game's own files, which
				 * finishes the task either way. */
				downloadFiles(openStagedInstance());
				return;
			});
	connect(m_filesNetJob.get(), &NetJob::failed, [&](QString reason) {
		emitFailed(reason);
		m_filesNetJob.reset();
	});
	connect(m_filesNetJob.get(), &NetJob::progress,
			[&](qint64 current, qint64 total) { setProgress(current, total); });
	// One line per mod being downloaded.
	propagateStepsFrom(m_filesNetJob.get());
	setStatus(tr("Downloading mods..."));
	m_filesNetJob->start();
}

/* Returns what the overrides put into the game directory, as paths
 * relative to it - the pack is as responsible for these as it is for the
 * mods its manifest names, so an update has to account for both. Taken
 * from the override folders before they are copied, which is the last
 * moment they are still distinguishable from what is already there. */
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
	/* Same reason as the Flame path: asked before anything that depends
	 * on the answer. */
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

	// Move overrides folder contents to the game directory
	QString mcPath = FS::PathCombine(m_stagingPath, gameDirName());

	QDir mcDir(mcPath);
	if (!mcDir.exists()) {
		mcDir.mkpath(".");
	}

	m_packOverridePaths = applyModrinthOverrides(m_stagingPath, mcPath);

	/* Everything that will end up being executed is now knowable: the
	 * manifest says where each mod comes from, and the overrides have
	 * already been unpacked, so any jars the pack simply carried are on
	 * disk. Ask before fetching anything.
	 *
	 * Deliberately before the instance is configured - refusing here
	 * leaves nothing behind but the staging directory, which the staging
	 * step removes on failure. */
	{
		QStringList suspect;
		for (const auto& file : pack.files) {
			/* Files the pack marks unsupported on the client are never
			 * fetched, so they are nothing to warn about: asking the
			 * user to vouch for a download that is not going to happen
			 * teaches them to wave the prompt through. */
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

	// Create instance config
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
		/* The same loader as the content platforms name it. Kept next to
		 * the component id so the two cannot drift apart. */
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

	/* Persist pack source — same logic as the Flame path. The
	 * Modrinth manifest carries name + versionId, so even drag-drop
	 * imports record enough to drive update checks (we use
	 * `name` as the slug since Modrinth slugs and pack names align
	 * for most published packs; the attach UI lets the user fix
	 * mismatches). */
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

	/* What to tell Modrinth these downloads are for.
	 *
	 * Authors see their download counts, and a pack install shows up in
	 * them either way; without this it shows up as though each mod had
	 * been installed on its own. "update" and "modpack" are the two
	 * things this task can honestly be doing. */
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
	/* Which pack version pulled them in, when we know it. */
	downloadMeta.dependentOn = m_updateTarget.versionId.isEmpty()
								   ? pack.versionId
								   : m_updateTarget.versionId;

	// Download all mod files
	m_filesNetJob =
		new NetJob(tr("Modrinth mod download"), LAUNCHER->network());
	auto minecraftDir = FS::PathCombine(m_stagingPath, gameDirName());
	auto canonicalBase = QDir(minecraftDir).canonicalPath();
	/* What this version is responsible for. Built from the same loop that
	 * queues the downloads, so a file we decided not to install does not
	 * get recorded as if we had. */
	QStringList contentPaths = m_packOverridePaths;

	/* On an update, the files already sitting in the instance are worth
	 * looking at before fetching anything: most of a pack does not change
	 * between two versions, and re-downloading a file we can prove we
	 * already have is minutes of somebody's connection spent producing
	 * identical bytes.
	 *
	 * Skipping a download leaves that path empty in the staged instance,
	 * which is exactly right - the commit step merges staging over the
	 * live instance, so a path nothing was staged for keeps the file that
	 * is already there. */
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
			/* The pack says this file has no business on a client, and a
			 * client is what we are installing. Left out entirely rather
			 * than installed disabled: a server-only jar sitting in
			 * mods/ under a ".disabled" name is a file the user never
			 * asked for and would have to work out for themselves. */
			qDebug() << "Skipping file the pack marks unsupported on the "
						"client:"
					 << file.path;
			continue;
		}

		/* A file the pack offers rather than requires is installed
		 * turned off, under the name this launcher uses to mean that.
		 * The same treatment the CurseForge path gives a file its
		 * manifest marks not-required, so a pack that travels through
		 * either importer arrives the same way - and so an instance
		 * exported with disabled mods and imported again still has them
		 * disabled. */
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
			/* Both names the file could be under. A mod the user turned
			 * off is still the file the pack asked for, and re-fetching
			 * the enabled copy next to it would both waste the download
			 * and quietly turn the mod back on. */
			const QString installedPath =
				installedGameDir.absoluteFilePath(file.path);
			QString keep;
			if (fileMatchesHash(installedPath, file.sha1, file.fileSize)) {
				keep = file.path;
			} else if (fileMatchesHash(installedPath +
										   QLatin1String(".disabled"),
									   file.sha1, file.fileSize)) {
				keep = file.path + QLatin1String(".disabled");
			}
			if (!keep.isEmpty()) {
				/* Recorded under the name it actually has on disk, since
				 * that is the file this version is responsible for. */
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

	/* Stash a copy of pack files + the staging-instance .minecraft
	 * dir so the post-download lambda can write mod-metadata
	 * sidecars without dereferencing a stale stack frame. The
	 * sidecars are what ModUpdateCheckTask uses to ask Modrinth
	 * "is there a newer version?" later — without them every mod
	 * looks like a hand-dropped jar and update detection silently
	 * does nothing. */
	const QString sidecarMcPath = minecraftDir;
	const QVector<Modrinth::File> sidecarFiles = pack.files;

	connect(m_filesNetJob.get(), &NetJob::succeeded, this,
			[this, sidecarMcPath, sidecarFiles]() {
				writeModrinthModSidecars(sidecarMcPath, sidecarFiles);
				m_filesNetJob.reset();
				/* Same as the CurseForge path: the pack is complete, and
				 * this finishes the task whether or not it does
				 * anything. */
				downloadFiles(openStagedInstance());
			});
	connect(m_filesNetJob.get(), &NetJob::failed, [&](QString reason) {
		emitFailed(reason);
		m_filesNetJob.reset();
	});
	connect(m_filesNetJob.get(), &NetJob::progress,
			[&](qint64 current, qint64 total) { setProgress(current, total); });
	// One line per mod being downloaded.
	propagateStepsFrom(m_filesNetJob.get());

	setStatus(tr("Downloading mods..."));
	m_filesNetJob->start();
}

void InstanceImportTask::processTechnic()
{
	shared_qobject_ptr<Technic::TechnicPackProcessor> packProcessor =
		new Technic::TechnicPackProcessor();

	/* The processor does all of its work inside run() and reports the
	 * outcome through these signals, so the handlers only record what
	 * happened and the rest is done once run() has returned.
	 *
	 * That ordering is the point, not a style choice: the processor
	 * builds the staged instance.cfg through a settings object of its
	 * own that lives until run() returns, and an INI settings object
	 * rewrites the whole file from its in-memory copy on every change.
	 * Writing to that same file from a second object while the first is
	 * still alive means whichever saves last silently drops the other's
	 * keys. */
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
		/* Either the processor failed - and has already said why - or it
		 * finished without reporting anything at all, which we cannot
		 * treat as an install. */
		if (!processed && !reportedFailure) {
			emitFailed(tr("The Technic pack could not be processed."));
		}
		return;
	}

	/* A Technic pack has no manifest naming its files: the archive *is*
	 * the file list, and by now the processor has unpacked all of it. So
	 * whatever is in the game directory is precisely what this pack
	 * shipped, which is what a later update needs to know to clean up
	 * after it. */
	if (!recordPackContents(listFilesRelative(stagedGameDir()))) {
		emitFailed(tr("Installation cancelled."));
		return;
	}

	/* Same reasoning as the MeshMC path: a Technic archive carries no
	 * catalogue identity, but this path is reachable as an "update from
	 * file" of a managed instance, and the staged instance.cfg replaces
	 * the live one. Without this the instance comes back from a
	 * successful update no longer recognised as a managed pack.
	 *
	 * The instance is reopened here rather than reusing the processor's
	 * own - that one is gone, by design, see above. A NullInstance is
	 * enough: only the settings object is needed, and the keys written
	 * are registered by BaseInstance. Keys already in the file that this
	 * object never registers are preserved, because the settings object
	 * saves the copy of the file it loaded. */
	QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
	NullInstance instance(m_globalSettings, instanceSettings, m_stagingPath);
	writePackSourceToInstance(instance, m_packHint);

	emitSucceeded();
}

void InstanceImportTask::processMeshMC()
{
	QString configPath = FS::PathCombine(m_stagingPath, "instance.cfg");
	auto instanceSettings = std::make_shared<INISettingsObject>(configPath);
	instanceSettings->registerSetting("InstanceType", "Legacy");

	NullInstance instance(m_globalSettings, instanceSettings, m_stagingPath);

	// reset time played on import... because packs.
	instance.resetTimePlayed();

	// Set a new name for the imported instance
	instance.setName(m_instName);

	// Use user-specified icon if available, otherwise import from the pack
	if (m_instIcon != "default") {
		instance.setIconKey(m_instIcon);
	} else {
		m_instIcon = instance.iconKey();

		auto importIconPath =
			IconUtils::findBestIconIn(instance.instanceRoot(), m_instIcon);
		if (!importIconPath.isNull() && QFile::exists(importIconPath)) {
			// import icon
			auto iconList = LAUNCHER->icons();
			if (iconList->iconFileExists(m_instIcon)) {
				iconList->deleteIcon(m_instIcon);
			}
			iconList->installIcons({importIconPath});
		}
	}

	/* Same as the Technic path: a MeshMC pack is its own file list, so
	 * what was unpacked is what the pack consists of. */
	if (!recordPackContents(listFilesRelative(stagedGameDir()))) {
		emitFailed(tr("Installation cancelled."));
		return;
	}

	/* A MeshMC archive ships its own instance.cfg and says nothing about
	 * which catalogue pack it is, so there is nothing here to recover a
	 * hint from - whatever the caller knew is all there is.
	 *
	 * It still has to be written, because this path is reachable as an
	 * update: "update from file" on a managed instance hands us an
	 * archive that happens to be in this format. The staged instance.cfg
	 * replaces the live one on commit, so anything not written into it
	 * is gone - the pack's identity, and with it the page that offered
	 * the update in the first place. A plain import has no hint and no
	 * update target, which leaves this a no-op. */
	writePackSourceToInstance(instance, m_packHint);

	emitSucceeded();
}

/* Hosts a modpack's mod downloads are allowed to name without the user
 * being asked about it.
 *
 * Only the two catalogues' own content networks. Anything else may be
 * perfectly innocent - plenty of mods are hosted by their authors - but
 * "innocent" is not something the launcher can establish, and the file
 * is going to be loaded as code by the game. So the answer is to name it
 * and let the user decide, not to guess. */
static bool isKnownContentHost(const QUrl& url, ContentSource source)
{
	if (url.scheme() != QLatin1String("https")) {
		/* Plain http means the file can be swapped in transit whatever
		 * the host says. */
		return false;
	}

	const QString host = url.host().toLower();
	switch (source) {
		case ContentSource::Modrinth:
			/* Not just Modrinth's own CDN: the mrpack format allows a
			 * manifest to name downloads from a small set of hosts, and
			 * a pack that points at a GitHub release is conformant
			 * rather than suspect. Refusing them here would raise an
			 * untrusted-content warning on packs Modrinth itself
			 * accepts, which teaches the user that the warning means
			 * nothing. See ModrinthApi::mrpackHosts(). */
			return ModrinthApi::mrpackHosts().contains(host);

		case ContentSource::CurseForge:
			/* CurseForge serves files from several numbered edge nodes
			 * under this domain, so it is matched by suffix - anchored
			 * with the dot so that "notforgecdn.net" cannot pass. */
			if (host == QLatin1String("forgecdn.net") ||
				host.endsWith(QLatin1String(".forgecdn.net"))) {
				return true;
			}
			/* The site's own download route, which is where we are sent
			 * for files the API will not hand out directly. */
			return host == FlameApi::siteHost();
	}
	return false;
}

QStringList
InstanceImportTask::findBundledCode(const QString& minecraftDir) const
{
	QStringList found;

	/* Where the game loads code from. Resource and shader packs are left
	 * out: they are data, and listing every texture pack in a warning
	 * about executable content would train the user to click through
	 * it. */
	const QStringList codeFolders = {
		QStringLiteral("mods"), QStringLiteral("coremods"),
		QStringLiteral("jarmods"), QStringLiteral("plugins")};

	const QDir root(minecraftDir);
	for (const QString& folder : codeFolders) {
		const QString path = FS::PathCombine(minecraftDir, folder);
		if (!QFileInfo(path).isDir()) {
			continue;
		}

		/* Everything in "mods" is reported, whatever it is called.
		 *
		 * The extension is not what decides whether a file gets loaded -
		 * the loader is, and a pack is free to point it at a file named
		 * anything at all. Filtering on ".jar" here would mean a pack
		 * could smuggle code past this list by choosing a different
		 * name, which is precisely the case the list exists for. The
		 * other three folders only ever hold jars, so filtering there
		 * costs nothing and keeps the odd stray text file out of a
		 * security prompt. */
		const bool listEverything = folder == QLatin1String("mods");

		QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			const QString file = it.next();
			/* ".jar.disabled" counts too - it is one rename away from
			 * being loaded. */
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

	/* Opened fresh rather than handed down from the code that configured
	 * the pack. That object is long gone by now, on purpose: an INI
	 * settings object rewrites the whole file from its in-memory copy on
	 * every change, so two of them over one instance.cfg means the last
	 * to write wins and the other's keys vanish. Letting the first one go
	 * before opening the second is what keeps that from happening - and
	 * it also means the pack profile it built has been flushed to disk,
	 * which is what we are about to read. */
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

	/* A surface of its own, with the files listed on it and consent as a
	 * separate deliberate act. */
	return LAUNCHER->uiHost()->confirmUntrustedMods(suspectPaths);
}

bool InstanceImportTask::resolveUpdateTargetFromCatalogue()
{
	if (!m_updateTarget.isEmpty()) {
		/* The pack page already named the instance to replace. Asking
		 * again would be asking the user to repeat themselves. */
		return true;
	}
	if (m_packHint.provider.isEmpty() || m_packHint.packId.isEmpty()) {
		/* Not a catalogue install, so there is no id to match on. */
		return true;
	}
	if (LAUNCHER->settings()->get("SkipModpackUpdatePrompt").toBool()) {
		/* Turned off, so installing means installing: a second instance,
		 * without the question. Checked before looking anything up so
		 * that the answer costs nothing when nobody wants it. */
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

	/* Named actions rather than yes/no: there are three answers here and
	 * two of them install something. */
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
		/* The version fields are the catalogue entry the user picked -
		 * the same thing the pack page would pass - because the instance
		 * has to end up claiming the version it now actually has. */
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

	/* Cancel, or the dialog dismissed without an answer. Treated as "do
	 * nothing at all", which is the only reading that cannot surprise
	 * anyone. */
	return false;
}

QString InstanceImportTask::gameDirName()
{
	if (!m_gameDirName.isEmpty()) {
		return m_gameDirName;
	}

	/* What the importer has always staged, and what every instance it
	 * created is therefore called. Also the answer for anything that is
	 * not an update, where there is no existing name to respect. */
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
	/* The same rule MinecraftInstance::gameRoot() applies, so that what
	 * we read here is what the instance will call its game directory. */
	const QString minecraft = FS::PathCombine(m_stagingPath, "minecraft");
	const QString dotMinecraft = FS::PathCombine(m_stagingPath, ".minecraft");

	if (QFileInfo(minecraft).isDir() && !QFileInfo::exists(dotMinecraft)) {
		return minecraft;
	}
	if (QFileInfo(dotMinecraft).isDir()) {
		return dotMinecraft;
	}
	/* Neither is there, so there is nothing to enumerate and the answer
	 * only has to be harmless. */
	return FS::PathCombine(m_stagingPath, gameDirName());
}

/* The sidecar recording where the mod at @p gameRelativePath came from,
 * or an empty string when there is none to speak of.
 *
 * @p indexes caches one loaded index per folder, because the answer for
 * one file requires reading every sidecar in its folder.
 *
 * Only the folders that can carry sidecars are looked at, which is the
 * same question sidecarFolderForPath() answers when the sidecars are
 * being written; anything else in a pack - a config file, a script - has
 * no provenance record to clean up. */
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

/* Whether the user is willing to let a pack update delete save files it
 * no longer ships.
 *
 * Asked, rather than decided: a world is the one thing in an instance
 * that cannot be downloaded again, and a pack that shipped one has no
 * way of knowing whether the copy on disk is still the one it shipped or
 * a hundred hours of somebody's game. */
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
	/* Written into the staging directory, so that it travels with the
	 * files it describes: the commit step merges the staged instance over
	 * the live one, which replaces the old list at the same moment it
	 * replaces the old files. */
	if (!PackContents::write(m_stagingPath, gameRelativePaths)) {
		/* Not fatal. What is lost is the *next* update's ability to clean
		 * up after this one; refusing to install a pack that is otherwise
		 * fine over that would be the worse trade. */
		qWarning() << "Could not record pack contents in" << m_stagingPath;
	}

	if (m_updateTarget.isEmpty()) {
		/* A fresh install replaces nothing, so nothing is stale. */
		return true;
	}

	auto previous =
		LAUNCHER->instances()->getInstanceById(m_updateTarget.instanceId);
	if (!previous) {
		/* Gone between the page opening and the update running. The
		 * commit step will have its own opinion about that; there is
		 * nothing to clean up here either way. */
		qWarning() << "Cannot diff pack contents against missing instance"
				   << m_updateTarget.instanceId;
		return true;
	}

	auto minecraftPrevious =
		std::dynamic_pointer_cast<MinecraftInstance>(previous);
	if (!minecraftPrevious) {
		/* The recorded paths are relative to a game directory, and only a
		 * Minecraft instance has one. Rather than guess at a directory to
		 * delete files in, do nothing. */
		qWarning() << "Not diffing pack contents:" << previous->id()
				   << "has no game directory";
		return true;
	}

	QStringList installedPaths;
	if (!PackContents::read(previous->instanceRoot(), installedPaths)) {
		/* Without the list there is no way to tell the pack's files from
		 * the user's, and the difference decides what gets deleted. So
		 * the update goes ahead without cleaning up, and says so - the
		 * leftovers are visible to the user as duplicated mods, and being
		 * surprised by that is worse than being told. */
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
	/* One index per folder, kept for the length of the loop: reading a
	 * folder's sidecars means reading every file in its .index, and a
	 * pack that drops fifty mods drops them out of the same folder. */
	QHash<QString, std::shared_ptr<ModMetadataIndex>> indexes;
	for (const QString& relativePath : stale) {
		const QString absolutePath = gameDir.absoluteFilePath(relativePath);

		/* Checked before anything is asked about it. Most of this list is
		 * names that were never on disk - the enabled/disabled
		 * counterpart of every entry, files a blocked download never
		 * produced - and asking the user about a world that is not there
		 * would be a prompt about nothing. Safe to decide now: the merge
		 * cannot create a stale path, because a path the new version
		 * ships is by definition not stale. */
		if (!QFileInfo::exists(absolutePath)) {
			continue;
		}

		/* Case-insensitively, because this is a guard: a pack that spells
		 * the folder differently should still trip it. */
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

		/* And the record of where that file came from.
		 *
		 * A sidecar describes exactly one file, so once the file is gone
		 * it describes nothing. Left behind it is mostly inert - the
		 * index skips a sidecar whose file is missing - but it is still a
		 * claim that a mod is installed when it is not, and the next
		 * thing to write a sidecar for that project would have to work
		 * around it. */
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
		/* A fresh install has nothing to carry over from. */
		return;
	}

	auto previous =
		LAUNCHER->instances()->getInstanceById(m_updateTarget.instanceId);
	if (!previous) {
		/* The instance vanished between the page opening and the update
		 * running. The staging step will fail to find it too; nothing
		 * useful to do here beyond not crashing. */
		qWarning() << "Cannot carry settings over from missing instance"
				   << m_updateTarget.instanceId;
		return;
	}

	auto from = previous->settings();
	auto to = instance.settings();

	/* Settings that describe what the *user* did with this instance, as
	 * opposed to what the pack is. The pack has no opinion about any of
	 * them, so an update must not reset them.
	 *
	 * Listed explicitly rather than copied wholesale: the staged config
	 * legitimately owns the pack's own choices - the component versions,
	 * the icon the pack ships, InstanceType - and blanket-copying would
	 * put the old pack's versions back and undo the update. */
	static const char* const carried[] = {
		"notes",
		"lastLaunchTime",
		"totalTimePlayed",
		"lastTimePlayed",
		"shortcuts",
		"Profiler",
		/* Per-instance overrides. Each pair is an "override this?" flag
		 * plus the values it guards; the flag alone is meaningless
		 * without them, so they travel together. */
		"OverrideCommands",
		"PreLaunchCommand",
		"WrapperCommand",
		"PostExitCommand",
		"OverrideConsole",
		"ShowConsole",
		"AutoCloseConsole",
		"ShowConsoleOnError",
		"LogPrePostOutput",
	};

	for (const char* key : carried) {
		if (!from->contains(key) || !to->contains(key)) {
			/* Either an older config that never had the key, or a
			 * setting this instance type does not register. Skipping is
			 * correct: the staged default is then the right answer. */
			continue;
		}
		to->set(key, from->get(key));
	}
}

void InstanceImportTask::writePackSourceToInstance(BaseInstance& instance,
												   const PackSourceHint& hint)
{
	/* Carrying user settings over is part of finishing an updated
	 * instance, and every path that writes pack provenance is exactly a
	 * path that has just finished building one. */
	carryOverUserSettings(instance);

	if (hint.isEmpty())
		return;

	/* These keys mirror what PackUpdater reads back. They were
	 * pre-registered with sensible defaults in BaseInstance so
	 * `set()` here uses the correct type and survives a round
	 * trip through INI serialisation. Empty fields are still
	 * written explicitly — that way an absent value means "we
	 * never wrote a hint", while a present-but-empty value means
	 * "we tried and couldn't recover this one". */
	/* On an update the version we just installed is the one the page
	 * asked for, which is more trustworthy than anything recovered from
	 * the archive: the manifest inside a pack has been known to disagree
	 * with the catalogue about its own version id. Only override when we
	 * actually have a value - an update from a local file carries none,
	 * and there the manifest is all we have. */
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
