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

#include "ContentDownloadTask.h"
#include "core/LauncherContext.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "net/Download.h"
#include "net/ChecksumValidator.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QSet>

ContentDownloadTask::ContentDownloadTask(
	const QList<ModPlatform::DownloadItem>& items, const QString& targetDir,
	QObject* parent)
	: Task(parent), m_items(items), m_targetDir(targetDir)
{
}

void ContentDownloadTask::setMetadataIndex(
	std::shared_ptr<ModMetadataIndex> index)
{
	m_metadata = std::move(index);
}

bool ContentDownloadTask::abort()
{
	if (m_aborted) {
		return true;
	}
	m_aborted = true;

	if (m_netJob && m_netJob->isRunning()) {
		m_netJob->abort();
	}

	/* Only a running task may report a verdict - emitAborted() complains
	 * loudly otherwise - but the latch above still holds, so a task
	 * called off before it started will simply do nothing. */
	if (isRunning()) {
		emitAborted();
	}
	return true;
}

void ContentDownloadTask::executeTask()
{
	if (m_items.isEmpty()) {
		emitSucceeded();
		return;
	}

	QDir dir(m_targetDir);
	if (!dir.exists()) {
		dir.mkpath(".");
	}

	setStatus(tr("Downloading %1 file(s)...").arg(m_items.size()));

	m_netJob = new NetJob("ContentDownload", LAUNCHER->network());

	int skipped = 0;
	// Last-resort safety net: even if an upstream stage (dependency
	// resolver, conflict analyzer) failed to catch it, never let the same
	// target file name or the same platform+project be queued twice in one
	// batch. Without this, two versions of the same mod (e.g. one pulled in
	// as a dependency, one explicitly selected) could download side by side.
	QSet<QString> queuedFileNames;
	QSet<QString> queuedProjectKeys;
	for (const auto& item : m_items) {
		QString targetPath = dir.filePath(item.fileName);

		if (!item.fileName.isEmpty() &&
			queuedFileNames.contains(item.fileName)) {
			qWarning() << "ContentDownload: skipping duplicate file name in "
						  "batch:"
					   << item.fileName;
			skipped++;
			continue;
		}
		QString projectKey;
		if (!item.platform.isEmpty() && !item.projectId.isEmpty()) {
			projectKey = item.platform + ":" + item.projectId;
			if (queuedProjectKeys.contains(projectKey)) {
				qWarning() << "ContentDownload: skipping duplicate "
							  "project in batch:"
						   << item.name << "(" << projectKey << ")";
				skipped++;
				continue;
			}
		}
		if (!item.fileName.isEmpty()) {
			queuedFileNames.insert(item.fileName);
		}
		if (!projectKey.isEmpty()) {
			queuedProjectKeys.insert(projectKey);
		}

		// Updates explicitly target a different on-disk file ("foo-1.0.jar"
		// -> "foo-1.1.jar"). Drop the old file (and its sidecar) before
		// downloading so the directory never holds both versions at once.
		if (!item.replacesFileName.isEmpty() &&
			item.replacesFileName != item.fileName) {
			const QString oldEnabled = dir.filePath(item.replacesFileName);
			const QString oldDisabled =
				oldEnabled + QStringLiteral(".disabled");
			for (const QString& candidate : {oldEnabled, oldDisabled}) {
				if (QFile::exists(candidate)) {
					if (QFile::remove(candidate)) {
						qDebug() << "ContentDownload: removed superseded"
								 << candidate;
					} else {
						qWarning()
							<< "ContentDownload: failed to remove" << candidate;
					}
				}
			}
			if (m_metadata) {
				m_metadata->remove(item.replacesFileName);
			}
		}

		// Skip if file already exists with matching SHA1 checksum and the
		// caller did not request a forced replace.
		if (!item.replaceExisting && QFileInfo::exists(targetPath) &&
			!item.sha1.isEmpty()) {
			QFile existingFile(targetPath);
			if (existingFile.open(QIODevice::ReadOnly)) {
				QCryptographicHash hash(QCryptographicHash::Sha1);
				hash.addData(&existingFile);
				existingFile.close();
				if (hash.result().toHex() == item.sha1.toLatin1().toLower()) {
					qDebug() << "Skipping" << item.fileName
							 << "- already exists with matching SHA1";
					skipped++;
					continue;
				}
			}
		}

		// A forced replace must clear the destination first so the
		// downloader writes a fresh file rather than appending or failing.
		if (item.replaceExisting && QFileInfo::exists(targetPath)) {
			if (!QFile::remove(targetPath)) {
				qWarning() << "ContentDownload: cannot replace" << targetPath;
			}
			if (m_metadata) {
				m_metadata->remove(item.fileName);
			}
		}

		auto dl = Net::Download::makeFile(QUrl(item.downloadUrl), targetPath);
		if (!item.sha1.isEmpty()) {
			dl->addValidator(new Net::ChecksumValidator(
				QCryptographicHash::Sha1,
				QByteArray::fromHex(item.sha1.toLatin1())));
		}
		m_netJob->addNetAction(dl);
	}

	if (skipped > 0) {
		qDebug() << "Skipped" << skipped << "already-existing file(s)";
	}

	// If all files were skipped, nothing to download
	if (m_netJob->size() == 0) {
		m_netJob.reset();
		writeSidecars();
		emitSucceeded();
		return;
	}

	connect(m_netJob.get(), &NetJob::succeeded, this,
			&ContentDownloadTask::onDownloadSucceeded);
	connect(m_netJob.get(), &NetJob::failed, this,
			&ContentDownloadTask::onDownloadFailed);
	connect(m_netJob.get(), &NetJob::progress, this,
			&ContentDownloadTask::onDownloadProgress);
	// One line per file being downloaded.
	propagateStepsFrom(m_netJob.get());

	m_netJob->start();
}

void ContentDownloadTask::writeSidecars()
{
	if (!m_metadata) {
		return;
	}
	if (m_aborted) {
		/* A cancelled batch leaves files in whatever state the transfer
		 * reached, and a sidecar records a size and a SHA1 as fact. One
		 * written for a half-finished file would make the update checker
		 * and the conflict analyzer trust a hash that is simply wrong,
		 * which is worse than the file having no record at all. */
		return;
	}
	const QDir dir(m_targetDir);
	for (const auto& item : m_items) {
		// We only persist sidecars for items that finished on disk and that
		// we can reason about later. Pure-local installs are recorded by
		// ModFolderModel::installMod directly.
		const QString path = dir.filePath(item.fileName);
		if (!QFile::exists(path)) {
			continue;
		}

		ModMetadataIndex::Entry e;
		e.fileName = item.fileName;
		e.platform = item.platform;
		e.projectId = item.projectId;
		e.versionId = item.versionId;
		e.name = item.name;
		/* Recorded rather than thrown away: the slug is how the same
		 * project is recognised across the two providers, and it is what
		 * a packwiz sidecar is named after. Clearing it here left every
		 * entry in the index unable to answer either question. */
		e.slug = item.slug;
		e.downloadUrl = item.downloadUrl;
		e.sha1 = item.sha1.toLower();
		e.fileSize = item.fileSize;
		e.isDependency = item.isDependency;
		e.installedAt = QDateTime::currentDateTimeUtc();
		m_metadata->put(e);
	}
}

void ContentDownloadTask::onDownloadSucceeded()
{
	if (m_aborted) {
		return;
	}
	m_netJob.reset();
	writeSidecars();
	emitSucceeded();
}

void ContentDownloadTask::onDownloadFailed(QString reason)
{
	if (m_aborted) {
		/* This is the aborted job unwinding, not a transfer that went
		 * wrong. The verdict has already been given. */
		return;
	}
	m_netJob.reset();
	emitFailed(reason);
}

void ContentDownloadTask::onDownloadProgress(qint64 current, qint64 total)
{
	if (m_aborted) {
		return;
	}
	setProgress(current, total);
}
