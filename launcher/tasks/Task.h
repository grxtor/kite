/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QUuid>
#include <memory>

#include "QObjectPtr.h"

/**
 * The lifecycle of a single step of a multi step task, as seen by the UI.
 */
enum class TaskStepState { Waiting, Running, Failed, Succeeded };

Q_DECLARE_METATYPE(TaskStepState)

/** A snapshot of one step of a multi step task. Multi step tasks
 * (Task::isMultiStep) report each step through Task::stepProgress so the UI
 * can show one line per step. A step keeps its uid for its whole lifetime, so
 * the UI updates the line it has instead of adding new ones. */
struct TaskStepProgress {
	QUuid uid;
	qint64 current = 0;
	qint64 total = -1;

	QString status;
	QString details;
	TaskStepState state = TaskStepState::Waiting;

	TaskStepProgress() : uid(QUuid::createUuid()) {}
	explicit TaskStepProgress(QUuid step_uid) : uid(step_uid) {}

	bool isDone() const
	{
		return state == TaskStepState::Failed ||
			   state == TaskStepState::Succeeded;
	}

	void update(qint64 new_current, qint64 new_total)
	{
		current = new_current;
		total = new_total;
		state = TaskStepState::Running;
	}
};

Q_DECLARE_METATYPE(TaskStepProgress)

using TaskStepProgressList = QList<std::shared_ptr<TaskStepProgress>>;

class Task : public QObject
{
	Q_OBJECT
  public:
	using Ptr = shared_qobject_ptr<Task>;

	enum class State { Inactive, Running, Succeeded, Failed, AbortedByUser };

  public:
	explicit Task(QObject* parent = 0);
	virtual ~Task() {};

	bool isRunning() const;
	bool isFinished() const;
	bool wasSuccessful() const;

	/* Whether the task stopped because someone called abort(). Stopping on
	 * request is reported through failed() like a genuine failure, so this is
	 * the only way to tell them apart (e.g. to avoid announcing "Aborted." as
	 * an error). */
	bool wasAborted() const;

	/* A multi step task is made up of several other tasks and reports each
	 * step through the stepProgress signal, on top of its own overall
	 * progress. */
	virtual bool isMultiStep() const
	{
		return false;
	}

	/* The state of every step this task is made of, for callers connecting to
	 * an already running task. */
	virtual TaskStepProgressList getStepProgress() const
	{
		return {};
	}

	/*!
	 * Identifies this task among the steps of its parent. Stable for the whole
	 * lifetime of the task.
	 */
	QUuid getUid() const
	{
		return m_uid;
	}

	/* The error message passed to emitFailed(); empty if the task has not
	 * failed. */
	QString failReason() const;

	virtual QStringList warnings() const;

	virtual bool canAbort() const
	{
		return false;
	}

	QString getStatus()
	{
		return m_status;
	}

	/* Secondary, usually shorter status shown next to the status line (a byte
	 * count, an item count). */
	QString getDetails()
	{
		return m_details;
	}

	qint64 getProgress()
	{
		return m_progress;
	}

	qint64 getTotalProgress()
	{
		return m_progressTotal;
	}

  protected:
	void logWarning(const QString& line);

	/* Announces whatever canAbort() currently answers; called where that
	 * answer changes. Read through the virtual so the button and abort()
	 * cannot disagree. */
	void reportAbortStatus()
	{
		emit abortStatusChanged(canAbort());
	}

	void setAbortButtonText(const QString& text)
	{
		emit abortButtonTextChanged(text);
	}

  private:
	QString describe();

  signals:
	void started();
	void progress(qint64 current, qint64 total);
	void finished();
	void succeeded();
	void failed(QString reason);
	void status(QString status);
	void details(QString details);
	void stepProgress(TaskStepProgress const& step_progress);

	/* The answer to canAbort() has changed. canAbort() is computed, so a task
	 * whose answer changes mid-run has to say so; a task that never emits this
	 * keeps whatever abort-button state the caller gave it. */
	void abortStatusChanged(bool abortable);

	/* The button offering abort() should be relabelled: pressing it does not
	 * always cancel (a task that has produced its result and runs an optional
	 * extra step merely skips it), and "Abort" would promise a rollback that
	 * will not happen. */
	void abortButtonTextChanged(QString text);

  public slots:
	virtual void start();
	virtual bool abort()
	{
		return false;
	};

  protected:
	virtual void executeTask() = 0;

  protected slots:
	virtual void emitSucceeded();
	virtual void emitAborted();
	virtual void emitFailed(QString reason);

	/* Passes a child task's step on to our own listeners so steps of nested
	 * multi step tasks stay visible all the way up. */
	virtual void propagateStepProgress(TaskStepProgress const& step_progress);

	/* Wires an inner task so its steps show up among ours; for a task that
	 * hands the work to a single task of its own, which would otherwise report
	 * its steps to nobody. */
	void propagateStepsFrom(Task* other);

  public slots:
	void setStatus(const QString& status);
	void setDetails(const QString& details);
	void setProgress(qint64 current, qint64 total);

  private:
	State m_state = State::Inactive;
	QStringList m_Warnings;
	QString m_failReason = "";
	QString m_status;
	QString m_details;
	int m_progress = 0;
	int m_progressTotal = 100;
	QUuid m_uid = QUuid::createUuid();
};
