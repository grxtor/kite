/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2020-2021 Jamie Mansfield <jmansfield@cadixdev.org>
 * Copyright 2020-2021 Petr Mrazek <peterix@gmail.com>
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

#include "FTBPackInstallTask.h"

#include "FileSystem.h"
#include "Json.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "net/ChecksumValidator.h"
#include "settings/INISettingsObject.h"

#include "BuildConfig.h"
#include "core/LauncherContext.h"
#include "core/UiHost.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include <QDateTime>

namespace ModpacksCH
{
	void classifyFailedDownloads(const QStringList& failedUrls,
								 const QHash<QString, VersionFile>& files,
								 const QHash<QString, QString>& targets,
								 QList<BlockedMod>& blocked,
								 QStringList& unrecoverable)
	{
		for (const QString& url : failedUrls) {
			const auto it = files.constFind(url);
			if (it == files.constEnd()) {
				unrecoverable.append(url);
				continue;
			}
			if (it->curseforgeProject > 0 && it->curseforgeFile > 0) {
				BlockedMod mod;
				mod.projectId = it->curseforgeProject;
				mod.fileId = it->curseforgeFile;
				mod.fileName = it->name;
				mod.targetPath = targets.value(url);
				blocked.append(mod);
			} else {
				unrecoverable.append(it->name);
			}
		}
	}

	QString undownloadableMessage(const QStringList& names)
	{
		static constexpr int kNamedFiles = 3;
		const QString named =
			names.mid(0, kNamedFiles).join(QStringLiteral(", "));
		const int rest = int(names.size()) - kNamedFiles;
		if (rest <= 0) {
			return PackInstallTask::tr("Could not download %1.").arg(named);
		}
		if (rest == 1) {
			return PackInstallTask::tr("Could not download %1 and 1 more file.")
				.arg(named);
		}
		return PackInstallTask::tr("Could not download %1 and %2 more files.")
			.arg(named)
			.arg(rest);
	}


	PackInstallTask::PackInstallTask(Modpack pack, QString version)
	{
		m_pack = pack;
		m_version_name = version;
	}

	bool PackInstallTask::abort()
	{
		if (abortable) {
			return jobPtr->abort();
		}

		/* One abortable phase remains after the pack downloads: the optional
		 * game-file download the base class runs. It returns false if idle. */
		return InstanceTask::abort();
	}

	void PackInstallTask::executeTask()
	{
		bool found = false;
		VersionInfo version;

		for (auto vInfo : m_pack.versions) {
			if (vInfo.name == m_version_name) {
				found = true;
				version = vInfo;
				break;
			}
		}

		if (!found) {
			emitFailed(
				tr("Failed to find pack version %1").arg(m_version_name));
			return;
		}

		auto* netJob =
			new NetJob("ModpacksCH::VersionFetch", LAUNCHER->network());
		auto searchUrl = QString(BuildConfig.MODPACKSCH_API_BASE_URL +
								 "public/modpack/%1/%2")
							 .arg(m_pack.id)
							 .arg(version.id);
		netJob->addNetAction(
			Net::Download::makeByteArray(QUrl(searchUrl), &response));
		jobPtr = netJob;
		jobPtr->start();

		QObject::connect(netJob, &NetJob::succeeded, this,
						 &PackInstallTask::onDownloadSucceeded);
		QObject::connect(netJob, &NetJob::failed, this,
						 &PackInstallTask::onDownloadFailed);
	}

	void PackInstallTask::onDownloadSucceeded()
	{
		QJsonParseError parse_error;
		QJsonDocument doc = QJsonDocument::fromJson(response, &parse_error);
		if (parse_error.error != QJsonParseError::NoError) {
			qWarning() << "Error while parsing JSON response from FTB at "
					   << parse_error.offset
					   << " reason: " << parse_error.errorString();
			qWarning() << response;
			jobPtr.reset();
			emitFailed(tr("FTB sent back a pack version that could not be read."));
			return;
		}

		auto obj = doc.object();

		ModpacksCH::Version version;
		try {
			ModpacksCH::loadVersion(version, obj);
		} catch (const JSONValidationError& e) {
			/* Reset first: once a task has reported, its owner may delete it. */
			jobPtr.reset();
			emitFailed(tr("Could not understand pack manifest:\n") + e.cause());
			return;
		}
		m_version = version;

		downloadPack();
	}

	void PackInstallTask::onDownloadFailed(QString reason)
	{
		jobPtr.reset();
		emitFailed(reason);
	}

	void PackInstallTask::downloadPack()
	{
		setStatus(tr("Downloading mods..."));

		jobPtr = new NetJob(tr("Mod download"), LAUNCHER->network());
		for (auto file : m_version.files) {
			if (file.serverOnly)
				continue;
			if (file.url.isEmpty()) {
				qWarning() << "Skipping" << file.name
						   << "- no download URL available";
				continue;
			}

			QFileInfo fileName(file.name);
			auto cacheName = fileName.completeBaseName() + "-" + file.sha1 +
							 "." + fileName.suffix();

			auto entry = LAUNCHER->metacache()->resolveEntry(
				"ModpacksCHPacks", cacheName);
			entry->setStale(true);

			auto relpath = FS::PathCombine("minecraft", file.path, file.name);
			auto path = FS::PathCombine(m_stagingPath, relpath);

			if (filesToCopy.contains(path)) {
				qWarning() << "Ignoring" << file.url
						   << "as a file of that path is already downloading.";
				continue;
			}
			qDebug() << "Will download" << file.url << "to" << path;
			filesToCopy[path] = entry->getFullPath();
			const QString key = QUrl(file.url).toString();
			m_filesByUrl.insert(key, file);
			m_targetByUrl.insert(key, path);

			auto dl = Net::Download::makeCached(file.url, entry);
			if (!file.sha1.isEmpty()) {
				auto rawSha1 = QByteArray::fromHex(file.sha1.toLatin1());
				dl->addValidator(new Net::ChecksumValidator(
					QCryptographicHash::Sha1, rawSha1));
			}
			jobPtr->addNetAction(dl);
		}

		/* `this` as the connection context, and the job released before the
		 * task reports: its owner may delete the task afterwards, and a handler
		 * touching it crashed the launcher on a failed file. */
		connect(jobPtr.get(), &NetJob::succeeded, this, [this]() {
			abortable = false;
			jobPtr.reset();
			install();
		});
		connect(jobPtr.get(), &NetJob::failed, this, [this](QString reason) {
			abortable = false;
			onModsDownloadFailed(reason);
		});
		connect(jobPtr.get(), &NetJob::progress, this,
				[this](qint64 current, qint64 total) {
					abortable = true;
					setProgress(current, total);
				});
		propagateStepsFrom(jobPtr.get());

		jobPtr->start();
	}

	void PackInstallTask::onModsDownloadFailed(const QString& reason)
	{
		/* An abort marks stopped files as failed; that is the user calling the
		 * install off, not files to fetch by hand. */
		const bool aborted = jobPtr && jobPtr->wasAborted();
		const QStringList failed =
			jobPtr && !aborted ? jobPtr->getFailedFiles() : QStringList();
		jobPtr.reset();
		if (aborted) {
			emitAborted();
			return;
		}

		QList<BlockedMod> blocked;
		QStringList unrecoverable;
		classifyFailedDownloads(failed, m_filesByUrl, m_targetByUrl, blocked,
								unrecoverable);
		if (failed.isEmpty()) {
			emitFailed(reason);
			return;
		}
		if (!unrecoverable.isEmpty()) {
			emitFailed(undownloadableMessage(unrecoverable));
			return;
		}

		/* CurseForge files their authors keep off third-party downloads; handled
		 * as for a CurseForge pack (InstanceImportTask). */
		for (const auto& mod : blocked) {
			filesToCopy.remove(mod.targetPath);
		}
		UiHost* host = LAUNCHER->uiHost();
		if (host && host->resolveBlockedMods(
						tr("Restricted Mods"),
						tr("The following mods have restricted downloads "
						   "and are not available through the API.\n"
						   "Click the Download button next to each mod "
						   "to open its download page in your browser.\n"
						   "Once all files appear in your Downloads "
						   "folder, click Continue."),
						blocked)) {
			const QString downloadDir = QStandardPaths::writableLocation(
				QStandardPaths::DownloadLocation);
			for (const auto& mod : blocked) {
				if (!mod.found) {
					continue;
				}
				QDir().mkpath(QFileInfo(mod.targetPath).absolutePath());
				if (!QFile::copy(FS::PathCombine(downloadDir, mod.fileName),
								 mod.targetPath)) {
					logWarning(tr("Failed to copy %1 from downloads folder")
								   .arg(mod.fileName));
				}
			}
		} else {
			logWarning(tr("User cancelled restricted mod downloads - %1 "
						  "mod(s) will be missing")
						   .arg(blocked.size()));
		}
		install();
	}

	void PackInstallTask::install()
	{
		setStatus(tr("Copying modpack files"));

		for (auto iter = filesToCopy.begin(); iter != filesToCopy.end();
			 iter++) {
			auto& to = iter.key();
			auto& from = iter.value();
			FS::copy fileCopyOperation(from, to);
			if (!fileCopyOperation()) {
				qWarning() << "Failed to copy" << from << "to" << to;
				emitFailed(tr("Failed to copy files"));
				return;
			}
		}

		setStatus(tr("Installing modpack"));

		auto instanceConfigPath =
			FS::PathCombine(m_stagingPath, "instance.cfg");
		auto instanceSettings =
			std::make_shared<INISettingsObject>(instanceConfigPath);
		instanceSettings->suspendSave();
		instanceSettings->registerSetting("InstanceType", "Legacy");
		instanceSettings->set("InstanceType", "OneSix");

		/* Behind a shared_ptr: downloadFiles() runs against the instance after
		 * this function returns. */
		auto instancePtr = std::make_shared<MinecraftInstance>(
			m_globalSettings, instanceSettings, m_stagingPath);
		MinecraftInstance& instance = *instancePtr;
		auto components = instance.getPackProfile();
		components->buildingFromScratch();

		for (auto target : m_version.targets) {
			if (target.type == "game" && target.name == "minecraft") {
				components->setComponentVersion("net.minecraft", target.version,
												true);
				break;
			}
		}

		for (auto target : m_version.targets) {
			if (target.type != "modloader")
				continue;

			if (target.name == "forge") {
				components->setComponentVersion("net.minecraftforge",
												target.version, true);
			} else if (target.name == "neoforge") {
				components->setComponentVersion("net.neoforged", target.version,
												true);
			} else if (target.name == "fabric") {
				components->setComponentVersion("net.fabricmc.fabric-loader",
												target.version, true);
			} else if (target.name == "quilt-loader") {
				components->setComponentVersion("org.quiltmc.quilt-loader",
												target.version, true);
			}
		}

		QDir jarModsDir(FS::PathCombine(m_stagingPath, "minecraft", "jarmods"));
		if (jarModsDir.exists()) {
			QStringList jarMods;

			for (const auto& info :
				 jarModsDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files)) {
				jarMods.push_back(info.absoluteFilePath());
			}

			components->installJarMods(jarMods);
		}

		components->saveNow();

		instance.setName(m_instName);
		instance.setIconKey(m_instIcon);

		/* Record the origin with the keys the Modrinth and CurseForge importers
		 * use, since it cannot be recovered after install. */
		instanceSettings->set("PackProvider", QStringLiteral("modpacksch"));
		instanceSettings->set("PackId", QString::number(m_pack.id));
		instanceSettings->set("PackName", m_pack.name);
		instanceSettings->set("PackVersionId", QString::number(m_version.id));
		instanceSettings->set("PackVersionLabel", m_version.name);
		instanceSettings->set(
			"PackInstalledAt",
			QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

		instanceSettings->resumeSave();

		downloadFiles(instancePtr);
	}

} // namespace ModpacksCH
