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

#include "backup/BackupTask.h"

#include <QDebug>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <utility>

BackupTask::BackupTask(QString instanceId, QString instanceRoot, QString label,
					   QObject* parent)
	: Task(parent), m_instanceId(std::move(instanceId)),
	  m_instanceRoot(std::move(instanceRoot)), m_label(std::move(label))
{
	setObjectName(QStringLiteral("BackupTask"));
	setStatus(tr("Preparing backup..."));
	// The file count is unknown until compressDir() has walked the instance.
	setProgress(0, 0);
}

BackupTask::~BackupTask()
{
	// The worker holds a bare pointer to us. This only blocks if the task is
	// torn down mid-backup, which the normal flow never does.
	disconnect(&m_watcher, nullptr, this, nullptr);
	if (m_future.isRunning()) {
		qWarning() << "BackupTask destroyed while the backup was still "
					  "running - waiting for it to finish";
		m_future.waitForFinished();
	}
}

void BackupTask::executeTask()
{
	if (m_instanceRoot.isEmpty()) {
		emitFailed(tr("The instance has no directory to back up."));
		return;
	}

	connect(&m_watcher, &QFutureWatcher<BackupEntry>::finished, this,
			&BackupTask::backupFinished);

	m_future = QtConcurrent::run(QThreadPool::globalInstance(), [this] {
		BackupManager manager(m_instanceId, m_instanceRoot);
		return manager.createBackup(
			m_label, [this](const QString& status, const QString& details,
							qint64 current, qint64 total) {
				reportProgress(status, details, current, total);
			});
	});
	m_watcher.setFuture(m_future);
}

void BackupTask::reportProgress(const QString& status, const QString& details,
								qint64 current, qint64 total)
{
	QMetaObject::invokeMethod(
		this,
		[this, status, details, current, total] {
			if (!status.isEmpty()) {
				setStatus(status);
			}
			if (!details.isEmpty()) {
				setDetails(details);
			}
			setProgress(current, total);
		},
		Qt::QueuedConnection);
}

void BackupTask::backupFinished()
{
	m_result = m_future.result();

	if (!m_result.isValid()) {
		emitFailed(
			tr("Could not create the backup. See the launcher log for "
			   "details."));
		return;
	}

	setProgress(1, 1);
	emitSucceeded();
}
