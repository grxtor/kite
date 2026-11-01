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

#include <QDir>
#include <QFuture>
#include <QFutureWatcher>
#include <QString>
#include <QStringList>

#include <atomic>
#include <optional>

#include "tasks/Task.h"

namespace MMCZip
{
	/* Unpacks an archive off the UI thread, with progress and abort.
	 *
	 * `subdirectory` is a prefix inside the archive; only entries under it
	 * are written, with the prefix removed. Empty means the whole archive
	 * (for formats that ship files at the root). */
	class ExtractZipTask : public Task
	{
		Q_OBJECT
	  public:
		ExtractZipTask(QString input, QDir outputDir,
					   QString subdirectory = QString(),
					   QObject* parent = nullptr);
		~ExtractZipTask() override = default;

		/* Absolute paths of everything written; only meaningful after success. */
		QStringList extractedFiles() const
		{
			return m_extracted;
		}

		bool canAbort() const override
		{
			return true;
		}
		bool abort() override;

	  protected:
		void executeTask() override;

	  private:
		using ZipResult = std::optional<QString>;

		ZipResult extractZip();
		void finish();

		/* Task state may only be touched on the task's thread. */
		void reportStatus(const QString& status);
		void reportProgress(qint64 current, qint64 total);

		QString m_input;
		QDir m_outputDir;
		QString m_subdirectory;
		QStringList m_extracted;

		/* The QtConcurrent future cannot be cancelled. */
		std::atomic<bool> m_cancelled{false};

		QFuture<ZipResult> m_future;
		QFutureWatcher<ZipResult> m_watcher;
	};
} // namespace MMCZip
