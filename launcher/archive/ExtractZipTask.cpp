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

#include "archive/ExtractZipTask.h"

#include <QDebug>
#include <QFileInfo>
#include <QThreadPool>
#include <QtConcurrent>

#include "MMCZip.h"

namespace MMCZip
{
	ExtractZipTask::ExtractZipTask(QString input, QDir outputDir,
								   QString subdirectory, QObject* parent)
		: Task(parent), m_input(std::move(input)),
		  m_outputDir(std::move(outputDir)),
		  m_subdirectory(std::move(subdirectory))
	{
	}

	void ExtractZipTask::executeTask()
	{
		setStatus(tr("Extracting files..."));
		setProgress(0, 0);

		m_future = QtConcurrent::run(QThreadPool::globalInstance(),
									 [this]() { return extractZip(); });
		connect(&m_watcher, &QFutureWatcher<ZipResult>::finished, this,
				&ExtractZipTask::finish);
		m_watcher.setFuture(m_future);
	}

	bool ExtractZipTask::abort()
	{
		if (m_future.isRunning()) {
			m_cancelled.store(true);
			/* The worker cleans up what it wrote and returns; finish() turns that
			 * into an abort. */
			return true;
		}
		return false;
	}

	void ExtractZipTask::reportStatus(const QString& status)
	{
		QMetaObject::invokeMethod(
			this, [this, status] { setStatus(status); }, Qt::QueuedConnection);
	}

	void ExtractZipTask::reportProgress(qint64 current, qint64 total)
	{
		QMetaObject::invokeMethod(
			this, [this, current, total] { setProgress(current, total); },
			Qt::QueuedConnection);
	}

	auto ExtractZipTask::extractZip() -> ZipResult
	{
		/* Both reports come from the extracting thread, so neither may touch
		 * task state directly. */
		ExtractReporting reporting;
		reporting.progress = [this](qint64 current, qint64 total) {
			reportProgress(current, total);
		};
		reporting.isCancelled = [this]() { return m_cancelled.load(); };
		reporting.entryStarted = [this](const QString& name) {
			if (!name.isEmpty()) {
				reportStatus(tr("Unpacking: %1").arg(name));
			}
		};

		const auto extracted =
			extractSubDir(m_input, m_subdirectory, m_outputDir.absolutePath(),
						  reporting);
		if (!extracted.has_value()) {
			/* Cancellation returns the same "no result" as a damaged archive; only
			 * we know which. */
			if (m_cancelled.load()) {
				return ZipResult();
			}
			return ZipResult(
				tr("Failed to unpack %1. The archive appears to be damaged.")
					.arg(QFileInfo(m_input).fileName()));
		}

		m_extracted = extracted.value();
		return ZipResult();
	}

	void ExtractZipTask::finish()
	{
		if (m_cancelled.load()) {
			emitAborted();
			return;
		}

		const ZipResult result = m_future.result();
		if (result.has_value()) {
			emitFailed(result.value());
			return;
		}
		emitSucceeded();
	}
} // namespace MMCZip
