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

#include "PrepareKiteMod.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QUrl>

#include <launch/LaunchTask.h>

#include "BuildConfig.h"
#include "core/LauncherContext.h"
#include "minecraft/MinecraftInstance.h"
#include "net/ChecksumValidator.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"

namespace
{
	const QString kGeneralCache = QStringLiteral("general");
}

void PrepareKiteMod::executeTask()
{
	m_instance = std::dynamic_pointer_cast<MinecraftInstance>(
		m_parent->instance());
	auto* context = LAUNCHER;
	if (!m_instance || !context) {
		emitSucceeded();
		return;
	}
	m_cacheRoot = context->metacache()->getBasePath(kGeneralCache);
	m_mcVersion = m_instance->minecraftVersion();
	m_loaderVersion = m_instance->kiteModLoaderVersion();

	setStatus(tr("Checking the Kite mod"));
	auto entry =
		context->metacache()->resolveEntry(kGeneralCache, KiteMod::kManifestPath);
	/* Asked again every launch, so a new build is found without waiting. */
	entry->setStale(true);
	m_job.reset(new NetJob(tr("Kite mod"), context->network()));
	m_job->addNetAction(Net::Download::makeCached(
		QUrl(BuildConfig.META_URL + KiteMod::kManifestPath), entry));
	/* Failed or not, the cached manifest is what decides next: the server
	 * may be down or have no manifest yet. */
	connect(m_job.get(), &Task::finished, this,
			&PrepareKiteMod::manifestFetched);
	m_job->start();
}

void PrepareKiteMod::manifestFetched()
{
	if (m_aborted) {
		return;
	}
	const auto build =
		KiteMod::cachedBuild(m_cacheRoot, m_mcVersion, m_loaderVersion);
	if (!build) {
		finish(tr("Kite mod: no version for Minecraft %1").arg(m_mcVersion));
		return;
	}
	if (KiteMod::isDownloaded(m_cacheRoot, *build)) {
		finish(tr("Kite mod: %1 ready").arg(build->version));
		return;
	}
	download(*build);
}

void PrepareKiteMod::download(const KiteModBuild& build)
{
	QDir().mkpath(KiteMod::cacheFolder(m_cacheRoot));
	setStatus(tr("Downloading the Kite mod"));
	auto file = Net::Download::makeFile(QUrl(build.url),
										KiteMod::jarPath(m_cacheRoot, build));
	file->addValidator(new Net::ChecksumValidator(
		QCryptographicHash::Sha256, QByteArray::fromHex(build.sha256.toLatin1())));
	m_job.reset(new NetJob(tr("Kite mod"), LAUNCHER->network()));
	m_job->addNetAction(file);
	connect(m_job.get(), &NetJob::progress, this, &Task::setProgress);
	connect(m_job.get(), &Task::finished, this, [this, build] {
		if (m_aborted) {
			return;
		}
		if (m_job->wasSuccessful()) {
			downloaded(build);
		} else {
			downloadFailed(m_job->failReason());
		}
	});
	m_job->start();
}

void PrepareKiteMod::downloaded(const KiteModBuild& build)
{
	/* Whatever the download left has to be the right file before the game is
	 * pointed at it. */
	if (!KiteMod::isDownloaded(m_cacheRoot, build)) {
		QFile::remove(KiteMod::jarPath(m_cacheRoot, build));
		downloadFailed(tr("the file is not the expected size"));
		return;
	}
	const QDir folder(KiteMod::cacheFolder(m_cacheRoot));
	for (const QString& old :
		 KiteMod::outdatedJars(folder.entryList(QDir::Files), build.file)) {
		QFile::remove(folder.filePath(old));
	}
	finish(tr("Kite mod: %1 ready").arg(build.version));
}

void PrepareKiteMod::downloadFailed(const QString& reason)
{
	finish(tr("Kite mod could not be downloaded: %1").arg(reason));
}

void PrepareKiteMod::finish(const QString& line)
{
	emit logLine(line + QStringLiteral("\n"), MessageLevel::Kite);
	/* Deliberately not resetting m_job here: we may be inside its own
	 * finished() emission. It dies with this step. */
	emitSucceeded();
}

bool PrepareKiteMod::abort()
{
	if (!isRunning()) {
		return true;
	}
	m_aborted = true;
	if (m_job && m_job->isRunning()) {
		m_job->abort();
	}
	emitFailed(tr("Task aborted."));
	return true;
}
