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

#include "LauncherImportTask.h"

#include <QtConcurrentRun>

namespace
{
	/* How often the progress bar follows the copy. */
	constexpr int kProgressIntervalMs = 100;
} // namespace

LauncherImportTask::LauncherImportTask(const LauncherImport::BuildRequest& request)
	: m_request(request), m_cancel(std::make_shared<std::atomic_bool>(false)),
	  m_progress(std::make_shared<LauncherImport::BuildProgress>())
{
	m_progressTimer.setInterval(kProgressIntervalMs);
	connect(&m_progressTimer, &QTimer::timeout, this,
			&LauncherImportTask::reportProgress);
	connect(&m_watcher, &QFutureWatcher<LauncherImport::BuildResult>::finished,
			this, &LauncherImportTask::built);
}

LauncherImportTask::~LauncherImportTask()
{
	/* The worker holds its own references; it only needs telling. */
	m_cancel->store(true);
}

void LauncherImportTask::executeTask()
{
	setStatus(m_request.mode == LauncherImport::Mode::Link
				  ? tr("Linking %1").arg(m_request.name)
				  : tr("Copying %1").arg(m_request.name));
	setProgress(0, 0);
	const LauncherImport::BuildRequest request = m_request;
	const QString staging = m_stagingPath;
	auto cancel = m_cancel;
	auto progress = m_progress;
	m_watcher.setFuture(QtConcurrent::run([request, staging, cancel, progress]() {
		return LauncherImport::buildInstance(request, staging, *cancel, progress.get());
	}));
	m_progressTimer.start();
}

void LauncherImportTask::reportProgress()
{
	const qint64 total = m_progress->total.load();
	if (total > 0) {
		setProgress(m_progress->done.load(), total);
	}
}

bool LauncherImportTask::abort()
{
	if (!isRunning()) {
		return false;
	}
	m_cancel->store(true);
	return true;
}

void LauncherImportTask::built()
{
	m_progressTimer.stop();
	m_result = m_watcher.result();
	if (m_result.cancelled || m_cancel->load()) {
		emitAborted();
		return;
	}
	if (!m_result.ok) {
		emitFailed(m_result.error.isEmpty() ? tr("The instance could not be imported.")
											: m_result.error);
		return;
	}
	reportProgress();
	emitSucceeded();
}
