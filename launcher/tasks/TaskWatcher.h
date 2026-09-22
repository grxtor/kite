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

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

#include "tasks/Task.h"

/* A QML-facing observer for one running Task.
 *
 * QML has no idea what a Task is - it is a plain QObject with plain
 * signals, not a QML type - so a page that wants to show progress for an
 * instance install (or any other core Task) needs something with
 * Q_PROPERTYs it can bind to instead. This is that adapter: it watches one
 * Task for its whole run and mirrors what it says into properties, plus a
 * single finished(bool) signal for "the whole thing is over, here is
 * whether it worked".
 *
 * Deliberately generic - nothing here mentions instances or Modrinth. A
 * model that starts a Task (ModrinthModpackModel::install(), for
 * instance) wraps it in one of these and hands the result back to QML;
 * the Task itself is never exposed.
 */
class TaskWatcher : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString title READ title WRITE setTitle NOTIFY titleChanged)
	Q_PROPERTY(QString status READ status NOTIFY statusChanged)
	Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
	Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
	Q_PROPERTY(bool succeeded READ succeeded NOTIFY succeededChanged)
	Q_PROPERTY(bool failed READ failed NOTIFY failedChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	Q_PROPERTY(
		QString instanceId READ instanceId NOTIFY instanceIdChanged)

  public:
	/* @p task is the Task to watch - already constructed, not necessarily
	 * started yet. Connections are made here, in the constructor, so that
	 * nothing the task does before the caller starts it is missed. */
	explicit TaskWatcher(Task::Ptr task, QObject* parent = nullptr);
	~TaskWatcher() override;

	QString title() const
	{
		return m_title;
	}
	/* Set by whoever creates the watcher - a Task has no notion of a
	 * user-facing title of its own. */
	void setTitle(const QString& title);

	QString status() const
	{
		return m_status;
	}
	/* 0..1, or -1 while the task has not reported a total yet (an
	 * indeterminate/"busy" state, same convention as Task itself:
	 * progress(current, total) with total <= 0). */
	double progress() const
	{
		return m_progress;
	}
	bool isRunning() const
	{
		return m_running;
	}
	bool succeeded() const
	{
		return m_succeeded;
	}
	bool failed() const
	{
		return m_failed;
	}
	/* The failure reason, if any. Empty while running or on success. */
	QString error() const
	{
		return m_error;
	}
	/* The id the new instance will have, when that is knowable ahead of
	 * time. It usually is not: InstanceList only settles on a final,
	 * deduplicated instance id once the staged directory is committed,
	 * after the wrapped task has already succeeded - see
	 * InstanceList::commitStagedInstance(). So this stays empty unless
	 * the creator calls setInstanceId() with something it already knows. */
	QString instanceId() const
	{
		return m_instanceId;
	}
	void setInstanceId(const QString& instanceId);

	/* The task being watched, for a caller that needs to reach it
	 * directly (to abort it, for instance). May be null if this watcher
	 * was never given one. */
	Task* task() const
	{
		return m_task.get();
	}

  signals:
	void titleChanged();
	void statusChanged();
	void progressChanged();
	void runningChanged();
	void succeededChanged();
	void failedChanged();
	void errorChanged();
	void instanceIdChanged();

	/* The task is over, one way or the other. Fired exactly once, right
	 * after succeeded/failed settle to their final values. */
	void finished(bool ok);

  private slots:
	void onStatus(const QString& status);
	void onProgress(qint64 current, qint64 total);
	void onSucceeded();
	void onFailed(const QString& reason);

  private:
	void setRunning(bool running);
	void setProgressValue(double value);
	void flushProgress();

  private:
	Task::Ptr m_task;

	QString m_title;
	QString m_status;
	double m_progress = -1.0;
	bool m_running = false;
	bool m_succeeded = false;
	bool m_failed = false;
	QString m_error;
	QString m_instanceId;

	/* Progress notifications are throttled to ~10/s: a download can call
	 * Task::setProgress() far faster than any QML binding needs to
	 * repaint, and every NOTIFY firing means every binding using it
	 * re-evaluates. Leading value is applied immediately; anything that
	 * arrives before the window is up is coalesced into one trailing
	 * emit, so the last value is never lost. */
	static constexpr int kProgressThrottleMs = 100;
	QElapsedTimer m_progressThrottle;
	QTimer m_progressFlushTimer;
	double m_pendingProgress = -1.0;
	bool m_progressFlushPending = false;
};
