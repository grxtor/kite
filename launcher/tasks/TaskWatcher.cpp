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

#include "TaskWatcher.h"

TaskWatcher::TaskWatcher(Task::Ptr task, QObject* parent)
	: QObject(parent), m_task(std::move(task))
{
	m_progressFlushTimer.setSingleShot(true);
	connect(&m_progressFlushTimer, &QTimer::timeout, this,
			&TaskWatcher::flushProgress);

	if (!m_task) {
		return;
	}

	/* Picked up as they stand right now, in case the task was already
	 * doing something before this watcher was attached to it (started()
	 * only fires once, from inside start()). */
	m_status = m_task->getStatus();
	m_running = m_task->isRunning();
	m_succeeded = m_task->wasSuccessful();
	m_failed = m_task->isFinished() && !m_succeeded;
	if (m_task->getTotalProgress() > 0) {
		m_progress = double(m_task->getProgress()) /
					 double(m_task->getTotalProgress());
	}

	connect(m_task.get(), &Task::status, this, &TaskWatcher::onStatus);
	connect(m_task.get(), &Task::progress, this, &TaskWatcher::onProgress);
	connect(m_task.get(), &Task::succeeded, this,
			&TaskWatcher::onSucceeded);
	connect(m_task.get(), &Task::failed, this, &TaskWatcher::onFailed);
	connect(m_task.get(), &Task::started, this,
			[this] { setRunning(true); });
}

TaskWatcher::~TaskWatcher() {}

void TaskWatcher::setTitle(const QString& title)
{
	if (m_title == title) {
		return;
	}
	m_title = title;
	emit titleChanged();
}

void TaskWatcher::setInstanceId(const QString& instanceId)
{
	if (m_instanceId == instanceId) {
		return;
	}
	m_instanceId = instanceId;
	emit instanceIdChanged();
}

void TaskWatcher::setRunning(bool running)
{
	if (m_running == running) {
		return;
	}
	m_running = running;
	emit runningChanged();
}

void TaskWatcher::onStatus(const QString& status)
{
	if (m_status == status) {
		return;
	}
	m_status = status;
	emit statusChanged();
}

void TaskWatcher::onProgress(qint64 current, qint64 total)
{
	setProgressValue(total > 0 ? double(current) / double(total) : -1.0);
}

void TaskWatcher::setProgressValue(double value)
{
	m_pendingProgress = value;

	if (!m_progressThrottle.isValid() ||
		m_progressThrottle.elapsed() >= kProgressThrottleMs) {
		m_progressThrottle.restart();
		m_progressFlushPending = false;
		m_progress = value;
		emit progressChanged();
		return;
	}

	/* Within the throttle window: remember it and make sure a trailing
	 * flush is scheduled, but do not fire NOTIFY yet. */
	if (!m_progressFlushPending) {
		m_progressFlushPending = true;
		const int remaining =
			kProgressThrottleMs - int(m_progressThrottle.elapsed());
		m_progressFlushTimer.start(qMax(0, remaining));
	}
}

void TaskWatcher::flushProgress()
{
	if (!m_progressFlushPending) {
		return;
	}
	m_progressFlushPending = false;
	m_progressThrottle.restart();
	m_progress = m_pendingProgress;
	emit progressChanged();
}

void TaskWatcher::onSucceeded()
{
	setRunning(false);
	/* The final value is never dropped on the floor by the throttle,
	 * even if a flush was still pending. */
	m_progressFlushTimer.stop();
	m_progressFlushPending = false;
	m_progress = 1.0;
	emit progressChanged();

	m_succeeded = true;
	emit succeededChanged();
	emit finished(true);
}

void TaskWatcher::onFailed(const QString& reason)
{
	setRunning(false);
	m_progressFlushTimer.stop();
	m_progressFlushPending = false;

	m_error = reason;
	emit errorChanged();

	m_failed = true;
	emit failedChanged();
	emit finished(false);
}
