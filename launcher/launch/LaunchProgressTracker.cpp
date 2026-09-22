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

#include "LaunchProgressTracker.h"

#include "tasks/Task.h"

#include <QTimer>

#include <algorithm>

LaunchProgressTracker::LaunchProgressTracker(QObject* parent) : QObject(parent) {}

void LaunchProgressTracker::watch(Task* task)
{
	if (task == m_task) {
		return;
	}
	if (!task) {
		clear();
		return;
	}
	stopWatching();
	m_task = task;
	m_connections << connect(task, &Task::status, this,
							&LaunchProgressTracker::onStatus);
	m_connections << connect(task, &Task::progress, this,
							&LaunchProgressTracker::onProgress);
	m_connections << connect(task, &Task::finished, this,
							&LaunchProgressTracker::onFinished);
	// In case whatever owns the task drops it without it ever finishing.
	m_connections << connect(task, &QObject::destroyed, this,
							&LaunchProgressTracker::onFinished);
}

void LaunchProgressTracker::clear()
{
	stopWatching();
	m_status.clear();
	m_progress = -1;
	emitNow();
}

void LaunchProgressTracker::stopWatching()
{
	for (const QMetaObject::Connection& connection : m_connections) {
		QObject::disconnect(connection);
	}
	m_connections.clear();
	m_task = nullptr;
}

void LaunchProgressTracker::onStatus(const QString& status)
{
	m_status = status;
	scheduleEmit();
}

void LaunchProgressTracker::onProgress(qint64 current, qint64 total)
{
	m_progress = total > 0 ? (double(current) / double(total)) : -1.0;
	scheduleEmit();
}

void LaunchProgressTracker::onFinished()
{
	/* The task we were watching succeeded, failed, was aborted, or was
	 * simply destroyed - either way there is nothing left to report.
	 * Reported immediately rather than coalesced: sitting on a stale
	 * "Downloading..." line after the game has already started would be
	 * worse than one extra redraw. */
	stopWatching();
	m_status.clear();
	m_progress = -1;
	emitNow();
}

void LaunchProgressTracker::scheduleEmit()
{
	if (!m_sinceLastEmit.isValid() || m_sinceLastEmit.elapsed() >= m_minIntervalMs) {
		emitNow();
		return;
	}
	if (!m_pendingTimer) {
		m_pendingTimer = new QTimer(this);
		m_pendingTimer->setSingleShot(true);
		connect(m_pendingTimer, &QTimer::timeout, this,
				&LaunchProgressTracker::emitNow);
	}
	if (!m_pendingTimer->isActive()) {
		const qint64 remaining = m_minIntervalMs - m_sinceLastEmit.elapsed();
		m_pendingTimer->start(static_cast<int>(std::max<qint64>(0, remaining)));
	}
}

void LaunchProgressTracker::emitNow()
{
	m_sinceLastEmit.start();
	emit changed();
}
