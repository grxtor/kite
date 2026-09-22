/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#include <QDateTime>

namespace ModpacksCH
{

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

		/* Past the pack's own downloads there is still one abortable
		 * phase: the optional game-file download the base class runs
		 * after the instance is built. It knows whether that is
		 * happening, and returns false when it is not - which is the
		 * answer this used to give unconditionally. */
		return InstanceTask::abort();
	}

	void PackInstallTask::executeTask()
	{
		// Find pack version
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
			return;
		}

		auto obj = doc.object();

		ModpacksCH::Version version;
		try {
			ModpacksCH::loadVersion(version, obj);
		} catch (const JSONValidationError& e) {
			emitFailed(tr("Could not understand pack manifest:\n") + e.cause());
			jobPtr.reset();
			return;
		}
		m_version = version;

		downloadPack();
	}

	void PackInstallTask::onDownloadFailed(QString reason)
	{
		emitFailed(reason);
		jobPtr.reset();
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

			auto dl = Net::Download::makeCached(file.url, entry);
			if (!file.sha1.isEmpty()) {
				auto rawSha1 = QByteArray::fromHex(file.sha1.toLatin1());
				dl->addValidator(new Net::ChecksumValidator(
					QCryptographicHash::Sha1, rawSha1));
			}
			jobPtr->addNetAction(dl);
		}

		connect(jobPtr.get(), &NetJob::succeeded, this, [&]() {
			abortable = false;
			install();
			jobPtr.reset();
		});
		connect(jobPtr.get(), &NetJob::failed, [&](QString reason) {
			abortable = false;
			emitFailed(reason);
			jobPtr.reset();
		});
		connect(jobPtr.get(), &NetJob::progress,
				[&](qint64 current, qint64 total) {
					abortable = true;
					setProgress(current, total);
				});
		// One line per file being downloaded.
		propagateStepsFrom(jobPtr.get());

		jobPtr->start();
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

		/* Held behind a shared_ptr, and by reference below so that the
		 * rest of this function reads as it did. The pointer is what
		 * matters: this function ends by handing the instance to
		 * downloadFiles(), which runs against it after we return, and a
		 * local object would be gone by then. */
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

		// install any jar mods
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

		/* Record where this instance came from.
		 *
		 * These are the same keys the Modrinth and CurseForge importers
		 * write. FTB has no page for changing versions - there is no
		 * MeshMC UI that reads these back for an FTB pack today - but
		 * recording them costs one INI write and is the difference
		 * between an instance that knows it is version 1.4.0 of a
		 * specific pack and one that only knows its own name. Anything
		 * that later wants to notice "you already have this pack" or
		 * offer an update needs it to have been written at install
		 * time, because it cannot be recovered afterwards. */
		instanceSettings->set("PackProvider", QStringLiteral("modpacksch"));
		instanceSettings->set("PackId", QString::number(m_pack.id));
		instanceSettings->set("PackName", m_pack.name);
		instanceSettings->set("PackVersionId", QString::number(m_version.id));
		instanceSettings->set("PackVersionLabel", m_version.name);
		instanceSettings->set(
			"PackInstalledAt",
			QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

		instanceSettings->resumeSave();

		/* Finishes the task, whether or not it downloads anything. */
		downloadFiles(instancePtr);
	}

} // namespace ModpacksCH
