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

#include "SingleZipPackInstallTask.h"

#include <QtConcurrent>

#include "MMCZip.h"
#include "TechnicPackProcessor.h"
#include "FileSystem.h"

#include "core/LauncherContext.h"

Technic::SingleZipPackInstallTask::SingleZipPackInstallTask(
	const QUrl& sourceUrl, const QString& minecraftVersion)
{
	m_sourceUrl = sourceUrl;
	m_minecraftVersion = minecraftVersion;
}

bool Technic::SingleZipPackInstallTask::abort()
{
	if (m_abortable) {
		return m_filesNetJob->abort();
	}
	return false;
}

void Technic::SingleZipPackInstallTask::executeTask()
{
	setStatus(tr("Downloading modpack:\n%1").arg(m_sourceUrl.toString()));

	const QString path = m_sourceUrl.host() + '/' + m_sourceUrl.path();
	auto entry = LAUNCHER->metacache()->resolveEntry("general", path);
	entry->setStale(true);
	m_archiveEntry = entry;
	m_filesNetJob = new NetJob(tr("Modpack download"), LAUNCHER->network());
	m_filesNetJob->addNetAction(Net::Download::makeCached(m_sourceUrl, entry));
	m_archivePath = entry->getFullPath();
	auto job = m_filesNetJob.get();
	connect(job, &NetJob::succeeded, this,
			&Technic::SingleZipPackInstallTask::downloadSucceeded);
	connect(job, &NetJob::progress, this,
			&Technic::SingleZipPackInstallTask::downloadProgressChanged);
	connect(job, &NetJob::failed, this,
			&Technic::SingleZipPackInstallTask::downloadFailed);
	// Show the file being fetched as its own line in the dialog.
	propagateStepsFrom(job);
	m_filesNetJob->start();
}

void Technic::SingleZipPackInstallTask::downloadSucceeded()
{
	m_abortable = false;

	setStatus(tr("Extracting modpack"));

	/* Create the destination before extracting into it.
	 *
	 * QDir(path) only wraps a path - it does not make the directory
	 * exist - so this used to hand extractSubDir a target that was not
	 * there. The Solder path next door has always called
	 * ensureFolderPathExists() here, and the drag-and-drop importer does
	 * its own mkpath, which is why those two work and this one was the
	 * odd one out. */
	const QString extractPath = FS::PathCombine(m_stagingPath, ".minecraft");
	if (!FS::ensureFolderPathExists(extractPath)) {
		emitFailed(tr("Could not create the instance's minecraft folder:\n%1")
					   .arg(extractPath));
		return;
	}

	qDebug() << "Attempting to create instance from" << m_archivePath;

	QString archivePath = m_archivePath;
	m_extractFuture = QtConcurrent::run(
		QThreadPool::globalInstance(), [archivePath, extractPath]() {
			return MMCZip::extractSubDir(archivePath, QString(""), extractPath);
		});
	connect(&m_extractFutureWatcher, &QFutureWatcher<QStringList>::finished,
			this, &Technic::SingleZipPackInstallTask::extractFinished);
	connect(&m_extractFutureWatcher, &QFutureWatcher<QStringList>::canceled,
			this, &Technic::SingleZipPackInstallTask::extractAborted);
	m_extractFutureWatcher.setFuture(m_extractFuture);
	m_filesNetJob.reset();
}

void Technic::SingleZipPackInstallTask::downloadFailed(QString reason)
{
	m_abortable = false;
	emitFailed(reason);
	m_filesNetJob.reset();
}

void Technic::SingleZipPackInstallTask::downloadProgressChanged(qint64 current,
																qint64 total)
{
	m_abortable = true;
	setProgress(current / 2, total);
}

void Technic::SingleZipPackInstallTask::extractFinished()
{
	if (!m_extractFuture.result()) {
		/* Drop the cached archive.
		 *
		 * An archive we cannot unpack is of no use to anyone, and while
		 * it sits in the cache marked fresh every retry reads it back
		 * and fails in exactly the same place - which is what turned one
		 * bad download into a pack that could never be installed again.
		 * Evicting it means "try again" actually re-downloads. */
		if (m_archiveEntry) {
			qWarning() << "Discarding unusable cached archive"
					   << m_archivePath;
			/* The file has to go, not just the entry's freshness.
			 * Evicting only marks the entry stale, and a stale entry
			 * whose file is still on disk makes the next request a
			 * conditional one - so the server answers 304 and we go
			 * right back to the same damaged bytes. With the file gone
			 * MetaCacheSink sends no If-None-Match and we get a real
			 * download. */
			if (!QFile::remove(m_archivePath)) {
				qWarning() << "Could not remove" << m_archivePath;
			}
			LAUNCHER->metacache()->evictEntry(m_archiveEntry);
			m_archiveEntry.reset();
		}
		emitFailed(tr("Failed to extract modpack. The downloaded archive is "
					  "damaged; it has been discarded, so trying again will "
					  "download it afresh."));
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

	shared_qobject_ptr<Technic::TechnicPackProcessor> packProcessor =
		new Technic::TechnicPackProcessor();
	connect(packProcessor.get(), &Technic::TechnicPackProcessor::succeeded,
			this, &Technic::SingleZipPackInstallTask::emitSucceeded);
	connect(packProcessor.get(), &Technic::TechnicPackProcessor::failed, this,
			&Technic::SingleZipPackInstallTask::emitFailed);
	packProcessor->run(m_globalSettings, m_instName, m_instIcon, m_stagingPath,
					   m_minecraftVersion);
}

void Technic::SingleZipPackInstallTask::extractAborted()
{
	emitFailed(tr("Instance import has been aborted."));
}
