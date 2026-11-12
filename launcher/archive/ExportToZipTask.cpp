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

#include "archive/ExportToZipTask.h"

#include <QDebug>
#include <QFileInfo>
#include <QThreadPool>
#include <QtConcurrent>

#include "FileSystem.h"
#include "MMCZip.h"

namespace MMCZip
{
	ExportToZipTask::ExportToZipTask(QString outputPath, QDir dir,
									QFileInfoList files,
									QString destinationPrefix,
									bool followSymlinks, QObject* parent)
		: Task(parent), m_outputPath(std::move(outputPath)),
		  m_dir(std::move(dir)), m_files(std::move(files)),
		  m_destinationPrefix(std::move(destinationPrefix)),
		  m_followSymlinks(followSymlinks)
	{
	}

	ExportToZipTask::ExportToZipTask(QString outputPath, const QString& dir,
									QFileInfoList files,
									QString destinationPrefix,
									bool followSymlinks, QObject* parent)
		: ExportToZipTask(std::move(outputPath), QDir(dir), std::move(files),
						  std::move(destinationPrefix), followSymlinks, parent)
	{
	}

	void ExportToZipTask::executeTask()
	{
		setStatus(tr("Adding files..."));
		setProgress(0, m_files.length() + m_extraFiles.size());

		m_future = QtConcurrent::run(QThreadPool::globalInstance(),
									 [this]() { return exportZip(); });
		connect(&m_watcher, &QFutureWatcher<ZipResult>::finished, this,
				&ExportToZipTask::finish);
		m_watcher.setFuture(m_future);
	}

	bool ExportToZipTask::abort()
	{
		if (m_future.isRunning()) {
			m_cancelled.store(true);
			/* No emitAborted(): the worker finishes the file it is on and closes the
			 * archive; finish() reports the abort. */
			return true;
		}
		return false;
	}

	void ExportToZipTask::reportStatus(const QString& status)
	{
		QMetaObject::invokeMethod(
			this, [this, status] { setStatus(status); }, Qt::QueuedConnection);
	}

	void ExportToZipTask::reportProgress(qint64 current, qint64 total)
	{
		QMetaObject::invokeMethod(
			this, [this, current, total] { setProgress(current, total); },
			Qt::QueuedConnection);
	}

	auto ExportToZipTask::exportZip() -> ZipResult
	{
		if (!m_dir.exists()) {
			return ZipResult(tr("The folder to export does not exist."));
		}

		ZipWriter zip(m_outputPath);
		if (!zip.open()) {
			return ZipResult(tr("Could not create '%1': %2")
								 .arg(m_outputPath, zip.errorString()));
		}

		qint64 done = 0;
		const qint64 total =
			m_files.length() + m_extraFiles.size() + m_mappedFiles.size();

		/* Generated entries first, so a truncated archive still has the
		 * manifest. */
		for (auto it = m_extraFiles.constBegin(); it != m_extraFiles.constEnd();
			 ++it) {
			if (m_cancelled.load())
				return ZipResult();
			if (!zip.addFile(it.key(), it.value())) {
				return ZipResult(tr("Could not add %1: %2")
									 .arg(it.key(), zip.errorString()));
			}
			reportProgress(++done, total);
		}

		for (const QFileInfo& file : m_files) {
			if (m_cancelled.load())
				return ZipResult();

			QString absolute = file.absoluteFilePath();
			const QString relative = m_dir.relativeFilePath(absolute);

			if (m_excludeFiles.contains(relative)) {
				reportProgress(++done, total);
				continue;
			}

			reportStatus(tr("Compressing: %1").arg(relative));

			if (m_followSymlinks) {
				/* A mods folder is often a tree of links into a shared folder; storing
				 * a link would export dangling paths. */
				if (file.isSymLink()) {
					absolute = file.symLinkTarget();
				} else {
					const QString canonical = file.canonicalFilePath();
					if (!canonical.isEmpty()) {
						absolute = canonical;
					}
				}
			}

			if (!zip.addFile(absolute, m_destinationPrefix + relative)) {
				return ZipResult(tr("Could not read and compress %1: %2")
									 .arg(relative, zip.errorString()));
			}
			reportProgress(++done, total);
		}

		for (const auto& [absolute, entry] : m_mappedFiles) {
			if (m_cancelled.load())
				return ZipResult();
			reportStatus(tr("Compressing: %1").arg(entry));
			if (!zip.addFile(absolute, m_destinationPrefix + entry)) {
				return ZipResult(tr("Could not read and compress %1: %2")
									 .arg(entry, zip.errorString()));
			}
			reportProgress(++done, total);
		}

		if (!zip.close()) {
			return ZipResult(tr("Could not finish '%1': %2")
								 .arg(m_outputPath, zip.errorString()));
		}
		return ZipResult();
	}

	void ExportToZipTask::finish()
	{
		/* Nothing half-written is left to be mistaken for a finished export. */
		if (m_cancelled.load()) {
			FS::deletePath(m_outputPath);
			emitAborted();
			return;
		}

		const ZipResult result = m_future.result();
		if (result.has_value()) {
			FS::deletePath(m_outputPath);
			emitFailed(result.value());
			return;
		}
		emitSucceeded();
	}
} // namespace MMCZip
