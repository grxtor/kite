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

#pragma once

#include <QByteArray>
#include <QDir>
#include <QFileInfoList>
#include <QFuture>
#include <QFutureWatcher>
#include <QHash>
#include <QString>
#include <QStringList>

#include <atomic>
#include <optional>

#include "tasks/Task.h"

namespace MMCZip
{
	/* Writes a list of files into a zip off the UI thread, as a task with
	 * progress and abort. The caller decides the file list (see
	 * MMCZip::collectFileListRecursively()); a pack export needs it for its
	 * manifest too.
	 *
	 * `destinationPrefix` is prepended to every entry (`overrides/` for packs).
	 * `excludeFiles` names entries, relative to `dir`, to skip. `extraFiles`
	 * are generated entries (the manifest), written first so a truncated
	 * archive still has something identifiable. */
	class ExportToZipTask : public Task
	{
		Q_OBJECT
	  public:
		ExportToZipTask(QString outputPath, QDir dir, QFileInfoList files,
						QString destinationPrefix = QString(),
						bool followSymlinks = false, QObject* parent = nullptr);
		ExportToZipTask(QString outputPath, const QString& dir,
						QFileInfoList files,
						QString destinationPrefix = QString(),
						bool followSymlinks = false, QObject* parent = nullptr);
		~ExportToZipTask() override = default;

		void setExcludeFiles(QStringList excludeFiles)
		{
			m_excludeFiles = std::move(excludeFiles);
		}
		void addExtraFile(const QString& fileName, const QByteArray& data)
		{
			m_extraFiles.insert(fileName, data);
		}

		bool canAbort() const override
		{
			return true;
		}
		bool abort() override;

	  protected:
		void executeTask() override;

	  private:
		/* Empty on success, the reason on failure. */
		using ZipResult = std::optional<QString>;

		ZipResult exportZip();
		void finish();

		/* setStatus() / setProgress() from the worker thread: they write plain
		 * members (m_status is a QString) read off the task, a race over a
		 * refcounted string. The report hops to the task's thread first, as in
		 * BackupTask::reportProgress(). */
		void reportStatus(const QString& status);
		void reportProgress(qint64 current, qint64 total);

		QString m_outputPath;
		QDir m_dir;
		QFileInfoList m_files;
		QString m_destinationPrefix;
		bool m_followSymlinks;
		QStringList m_excludeFiles;
		QHash<QString, QByteArray> m_extraFiles;

		/* Not QFuture::cancel(): the future QtConcurrent::run() returns does not
		 * support cancelling, so isCanceled() stays false and abort would be
		 * ignored. */
		std::atomic<bool> m_cancelled{false};

		QFuture<ZipResult> m_future;
		QFutureWatcher<ZipResult> m_watcher;
	};
} // namespace MMCZip
