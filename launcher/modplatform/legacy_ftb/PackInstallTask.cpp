/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
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

#include "PackInstallTask.h"

#include <QtConcurrent>

#include "MMCZip.h"
#include "BaseInstance.h"
#include "FileSystem.h"
#include "settings/INISettingsObject.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/GradleSpecifier.h"

#include "BuildConfig.h"
#include "core/LauncherContext.h"

namespace LegacyFTB
{

	PackInstallTask::PackInstallTask(
		shared_qobject_ptr<QNetworkAccessManager> network, Modpack pack,
		QString version)
	{
		m_pack = pack;
		m_version = version;
		m_network = network;
	}

	void PackInstallTask::executeTask()
	{
		downloadPack();
	}

	void PackInstallTask::downloadPack()
	{
		setStatus(tr("Downloading zip for %1").arg(m_pack.name));

		auto packoffset =
			QString("%1/%2/%3")
				.arg(m_pack.dir, m_version.replace(".", "_"), m_pack.file);
		auto entry =
			LAUNCHER->metacache()->resolveEntry("FTBPacks", packoffset);
		netJobContainer = new NetJob("Download FTB Pack", m_network);

		entry->setStale(true);
		QString url;
		if (m_pack.type == PackType::Private) {
			url =
				QString(BuildConfig.LEGACY_FTB_CDN_BASE_URL + "privatepacks/%1")
					.arg(packoffset);
		} else {
			url = QString(BuildConfig.LEGACY_FTB_CDN_BASE_URL + "modpacks/%1")
					  .arg(packoffset);
		}
		netJobContainer->addNetAction(Net::Download::makeCached(url, entry));
		archivePath = entry->getFullPath();

		connect(netJobContainer.get(), &NetJob::succeeded, this,
				&PackInstallTask::onDownloadSucceeded);
		connect(netJobContainer.get(), &NetJob::failed, this,
				&PackInstallTask::onDownloadFailed);
		connect(netJobContainer.get(), &NetJob::progress, this,
				&PackInstallTask::onDownloadProgress);
		// One line per file being downloaded.
		propagateStepsFrom(netJobContainer.get());
		netJobContainer->start();

		progress(1, 4);
	}

	void PackInstallTask::onDownloadSucceeded()
	{
		abortable = false;
		unzip();
	}

	void PackInstallTask::onDownloadFailed(QString reason)
	{
		abortable = false;
		emitFailed(reason);
	}

	void PackInstallTask::onDownloadProgress(qint64 current, qint64 total)
	{
		abortable = true;
		progress(current, total * 4);
		setStatus(tr("Downloading zip for %1 (%2%)")
					  .arg(m_pack.name)
					  .arg(current / 10));
	}

	void PackInstallTask::unzip()
	{
		progress(2, 4);
		setStatus(tr("Extracting modpack"));
		QDir extractDir(m_stagingPath);

		QString extractPath = extractDir.absolutePath() + "/unzip";
		QString archivePathCopy = archivePath;
		m_extractFuture = QtConcurrent::run(
			QThreadPool::globalInstance(), [archivePathCopy, extractPath]() {
				return MMCZip::extractDir(archivePathCopy, extractPath);
			});
		connect(&m_extractFutureWatcher, &QFutureWatcher<QStringList>::finished,
				this, &PackInstallTask::onUnzipFinished);
		connect(&m_extractFutureWatcher, &QFutureWatcher<QStringList>::canceled,
				this, &PackInstallTask::onUnzipCanceled);
		m_extractFutureWatcher.setFuture(m_extractFuture);
	}

	void PackInstallTask::onUnzipFinished()
	{
		install();
	}

	void PackInstallTask::onUnzipCanceled()
	{
		emitAborted();
	}

	void PackInstallTask::install()
	{
		progress(3, 4);
		setStatus(tr("Installing modpack"));
		QDir unzipMcDir(m_stagingPath + "/unzip/minecraft");
		if (unzipMcDir.exists()) {
			// ok, found minecraft dir, move contents to instance dir
			if (!QDir().rename(m_stagingPath + "/unzip/minecraft",
							   m_stagingPath + "/.minecraft")) {
				emitFailed(tr("Failed to move unzipped minecraft!"));
				return;
			}
		}

		QString instanceConfigPath =
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
		components->setComponentVersion("net.minecraft", m_pack.mcVersion,
										true);

		bool fallback = true;

		// handle different versions
		QFile packJson(m_stagingPath + "/.minecraft/pack.json");
		QDir jarmodDir = QDir(m_stagingPath + "/unzip/instMods");
		if (packJson.exists()) {
			if (!packJson.open(QIODevice::ReadOnly | QIODevice::Text)) {
				emitFailed(tr("Failed to open pack.json!"));
				return;
			}
			QJsonDocument doc = QJsonDocument::fromJson(packJson.readAll());
			packJson.close();

			// we only care about the libs
			QJsonArray libs = doc.object().value("libraries").toArray();

			foreach (const QJsonValue& value, libs) {
				QString nameValue = value.toObject().value("name").toString();
				if (!nameValue.startsWith("net.minecraftforge")) {
					continue;
				}

				GradleSpecifier forgeVersion(nameValue);

				components->setComponentVersion(
					"net.minecraftforge", forgeVersion.version()
											  .replace(m_pack.mcVersion, "")
											  .replace("-", ""));
				packJson.remove();
				fallback = false;
				break;
			}
		}

		if (jarmodDir.exists()) {
			qDebug() << "Found jarmods, installing...";

			QStringList jarmods;
			for (auto info :
				 jarmodDir.entryInfoList(QDir::NoDotAndDotDot | QDir::Files)) {
				qDebug() << "Jarmod:" << info.fileName();
				jarmods.push_back(info.absoluteFilePath());
			}

			components->installJarMods(jarmods);
			fallback = false;
		}

		// just nuke unzip directory, it s not needed anymore
		FS::deletePath(m_stagingPath + "/unzip");

		if (fallback) {
			// TODO: Some fallback mechanism... or just keep failing!
			emitFailed(tr("No installation method found!"));
			return;
		}

		components->saveNow();

		progress(4, 4);

		instance.setName(m_instName);
		if (m_instIcon == "default") {
			m_instIcon = "ftb_logo";
		}
		instance.setIconKey(m_instIcon);
		instanceSettings->resumeSave();

		/* Finishes the task, whether or not it downloads anything. */
		downloadFiles(instancePtr);
	}

	bool PackInstallTask::abort()
	{
		if (abortable) {
			return netJobContainer->abort();
		}

		/* Past the pack's own download there is still one abortable
		 * phase: the optional game-file download the base class runs
		 * after the instance is built. It knows whether that is
		 * happening, and returns false when it is not - which is the
		 * answer this used to give unconditionally. */
		return InstanceTask::abort();
	}

} // namespace LegacyFTB
