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

#include <QHash>
#include <QList>
#include <QQueue>
#include <QStringList>
#include <QUuid>
#include <memory>

#include "Task.h"
#include "QObjectPtr.h"

/** Runs a bag of tasks, up to a given number at the same time, reporting each
 * child (and the steps of nested multi step children) as a separate step. A
 * failing child does not stop the others; the task fails once all have
 * settled, with all reasons collected. SequentialTask stops at the first
 * failure. */
class ConcurrentTask : public Task
{
	Q_OBJECT
  public:
	explicit ConcurrentTask(QObject* parent = nullptr,
							QString task_name = QString(),
							int max_concurrent = 6);
	virtual ~ConcurrentTask();

	/// Queues a task. Tasks added while this one runs are picked up as slots
	/// free up; tasks added after it finished are ignored.
	void addTask(Task::Ptr task);

	int totalSize() const
	{
		return m_all.size();
	}

	int finishedSize() const
	{
		return m_succeeded + m_failed + m_skipped;
	}

	bool isMultiStep() const override
	{
		return totalSize() > 1;
	}

	TaskStepProgressList getStepProgress() const override
	{
		return m_step_order;
	}

	bool canAbort() const override;

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

	void startNext();

	void dropQueued();

	virtual void updateState();

	virtual void finishTask();

	virtual void subTaskSucceeded(Task::Ptr task);
	virtual void subTaskFailed(Task::Ptr task, const QString& reason);
	virtual void subTaskStatus(Task::Ptr task, const QString& status);
	virtual void subTaskDetails(Task::Ptr task, const QString& details);
	virtual void subTaskProgress(Task::Ptr task, qint64 current, qint64 total);

	void startSubTask(Task::Ptr task);

	void disposeOf(Task::Ptr task, TaskStepState state);

	std::shared_ptr<TaskStepProgress> stepFor(const Task::Ptr& task);

  protected:
	QList<Task::Ptr> m_all;
	QQueue<Task::Ptr> m_queue;
	QHash<Task*, Task::Ptr> m_doing;

	int m_succeeded = 0;
	int m_failed = 0;
	int m_skipped = 0;

	QStringList m_fail_reasons;

	QHash<QUuid, std::shared_ptr<TaskStepProgress>> m_steps;
	TaskStepProgressList m_step_order;

	int m_max_concurrent;
	bool m_aborted = false;
};
