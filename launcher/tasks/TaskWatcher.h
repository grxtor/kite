/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

/* A QML-facing observer for one running Task. QML cannot use a Task, so this
 * adapter mirrors its status into Q_PROPERTYs and emits finished(bool) when
 * the whole thing is over. Generic: a model that starts a Task wraps it in one
 * of these and hands it to QML; the Task itself is never exposed. */
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
	/* @p task is already constructed, not necessarily started. Connections are
	 * made in the constructor so nothing the task does before the caller
	 * starts it is missed. */
	explicit TaskWatcher(Task::Ptr task, QObject* parent = nullptr);
	~TaskWatcher() override;

	QString title() const
	{
		return m_title;
	}
	/* Set by whoever creates the watcher; a Task has no user-facing title. */
	void setTitle(const QString& title);

	QString status() const
	{
		return m_status;
	}
	/* 0..1, or -1 while the task has not reported a total yet (indeterminate,
	 * as Task itself: progress(current, total) with total <= 0). */
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
	QString error() const
	{
		return m_error;
	}
	/* The id the new instance will have, when knowable ahead of time. Usually
	 * it is not: InstanceList settles on a final id only when the staged
	 * directory is committed (InstanceList::commitStagedInstance()), so this
	 * stays empty unless the creator calls setInstanceId(). */
	QString instanceId() const
	{
		return m_instanceId;
	}
	void setInstanceId(const QString& instanceId);

	/* The task being watched, e.g. to abort it. Null if the watcher was never
	 * given one, and once the task has finished. */
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

	/* The task is over, one way or the other; fired once, right after
	 * succeeded/failed settle. */
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
	/* Finished tasks are not kept alive by the watcher; the mirrored values stay. */
	void releaseTask();

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
	 * Task::setProgress() far faster than QML needs, and every NOTIFY
	 * re-evaluates its bindings. The leading value applies at once; later ones
	 * in the window coalesce into one trailing emit, so the last is never
	 * lost. */
	static constexpr int kProgressThrottleMs = 100;
	QElapsedTimer m_progressThrottle;
	QTimer m_progressFlushTimer;
	double m_pendingProgress = -1.0;
	bool m_progressFlushPending = false;
};
