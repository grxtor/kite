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

#include "ConcurrentTask.h"

#include <QDebug>

ConcurrentTask::ConcurrentTask(QObject* parent, QString task_name,
							   int max_concurrent)
	: Task(parent), m_max_concurrent(qMax(1, max_concurrent))
{
	if (!task_name.isEmpty()) {
		setObjectName(task_name);
	}
}

ConcurrentTask::~ConcurrentTask()
{
	// Children outlive us when someone else holds a reference; make sure none
	// calls back into a half destroyed object.
	for (auto& task : m_doing) {
		disconnect(task.get(), nullptr, this, nullptr);
	}
}

void ConcurrentTask::addTask(Task::Ptr task)
{
	if (!task) {
		qWarning() << "ConcurrentTask" << objectName()
				   << "was handed a null task, ignoring it";
		return;
	}
	if (isFinished()) {
		qWarning() << "ConcurrentTask" << objectName()
				   << "was handed a task after it had already finished,"
				   << "ignoring it";
		return;
	}

	m_all.append(task);
	m_queue.enqueue(task);

	if (isRunning()) {
		// Through the event loop: tasks are usually queued in a batch from
		// executeTask(), and reacting to each at once could conclude we are
		// finished halfway through building the list.
		QMetaObject::invokeMethod(this, &ConcurrentTask::startNext,
								  Qt::QueuedConnection);
	}
}

void ConcurrentTask::executeTask()
{
	startNext();
}

void ConcurrentTask::startNext()
{
	if (!isRunning()) {
		// Tasks can be queued long before anyone starts us, and children can
		// settle after we reached an end state.
		return;
	}

	if (m_aborted) {
		dropQueued();
	}

	while (m_doing.size() < m_max_concurrent && !m_queue.isEmpty()) {
		auto task = m_queue.dequeue();

		if (task->isFinished()) {
			// Somebody else already ran this one; take the result instead of
			// starting it twice.
			if (task->wasSuccessful()) {
				m_succeeded++;
			} else {
				m_failed++;
				m_fail_reasons.append(task->failReason());
			}
			continue;
		}

		startSubTask(task);
	}

	updateState();

	if (m_queue.isEmpty() && m_doing.isEmpty()) {
		finishTask();
	}
}

void ConcurrentTask::startSubTask(Task::Ptr task)
{
	m_doing.insert(task.get(), task);

	auto step = stepFor(task);
	step->state = TaskStepState::Running;
	step->status = task->getStatus();
	step->details = task->getDetails();
	step->current = task->getProgress();
	step->total = task->getTotalProgress();
	emit stepProgress(*step);

	connect(task.get(), &Task::succeeded, this,
			[this, task] { subTaskSucceeded(task); });
	connect(task.get(), &Task::failed, this,
			[this, task](QString reason) { subTaskFailed(task, reason); });
	connect(task.get(), &Task::status, this,
			[this, task](QString text) { subTaskStatus(task, text); });
	connect(task.get(), &Task::details, this,
			[this, task](QString text) { subTaskDetails(task, text); });
	connect(task.get(), &Task::progress, this,
			[this, task](qint64 current, qint64 total) {
				subTaskProgress(task, current, total);
			});
	// Steps of a child that is itself multi step belong in the same list, so
	// pass them straight to our listeners.
	connect(task.get(), &Task::stepProgress, this,
			&ConcurrentTask::propagateStepProgress);

	if (!task->isRunning()) {
		// Start from the event loop so a task that finishes right away does
		// not unwind into the middle of startNext().
		QMetaObject::invokeMethod(task.get(), &Task::start,
								  Qt::QueuedConnection);
	}
}

void ConcurrentTask::disposeOf(Task::Ptr task, TaskStepState state)
{
	disconnect(task.get(), nullptr, this, nullptr);
	m_doing.remove(task.get());

	auto step = m_steps.value(task->getUid());
	if (step) {
		step->state = state;
		emit stepProgress(*step);
	}
}

void ConcurrentTask::dropQueued()
{
	m_skipped += m_queue.size();
	m_queue.clear();
}

std::shared_ptr<TaskStepProgress> ConcurrentTask::stepFor(
	const Task::Ptr& task)
{
	auto uid = task->getUid();
	auto existing = m_steps.value(uid);
	if (existing) {
		return existing;
	}

	auto step = std::make_shared<TaskStepProgress>(uid);
	m_steps.insert(uid, step);
	m_step_order.append(step);
	return step;
}

void ConcurrentTask::updateState()
{
	if (totalSize() > 1) {
		setProgress(finishedSize(), totalSize());
		setStatus(tr("Executing %1 task(s) (%2 out of %3 are done)")
					  .arg(QString::number(m_doing.size()),
						   QString::number(finishedSize()),
						   QString::number(totalSize())));
		return;
	}

	// A lone task gets no list of its own, so we step aside and let it speak
	// through us (see subTaskStatus and friends).
	if (!m_queue.isEmpty()) {
		setStatus(tr("Waiting for a task to start..."));
	} else if (!m_doing.isEmpty()) {
		setStatus(tr("Executing 1 task:"));
	} else if (finishedSize() > 0) {
		setStatus(tr("Task finished."));
	} else {
		setStatus(tr("Please wait..."));
	}
}

void ConcurrentTask::finishTask()
{
	if (!isRunning()) {
		return;
	}

	if (m_aborted) {
		emitAborted();
	} else if (m_fail_reasons.isEmpty()) {
		emitSucceeded();
	} else {
		emitFailed(m_fail_reasons.join(QStringLiteral("\n")));
	}
}

void ConcurrentTask::subTaskSucceeded(Task::Ptr task)
{
	m_succeeded++;
	disposeOf(task, TaskStepState::Succeeded);
	startNext();
}

void ConcurrentTask::subTaskFailed(Task::Ptr task, const QString& reason)
{
	m_failed++;
	if (!reason.isEmpty()) {
		m_fail_reasons.append(reason);
	}
	disposeOf(task, TaskStepState::Failed);
	startNext();
}

void ConcurrentTask::subTaskStatus(Task::Ptr task, const QString& status)
{
	auto step = m_steps.value(task->getUid());
	if (!step) {
		return;
	}
	step->status = status;
	step->state = TaskStepState::Running;
	emit stepProgress(*step);

	// With one task there is no list on screen, so its status becomes ours.
	if (totalSize() == 1) {
		setStatus(status);
	}
}

void ConcurrentTask::subTaskDetails(Task::Ptr task, const QString& details)
{
	auto step = m_steps.value(task->getUid());
	if (!step) {
		return;
	}
	step->details = details;
	step->state = TaskStepState::Running;
	emit stepProgress(*step);

	if (totalSize() == 1) {
		setDetails(details);
	}
}

void ConcurrentTask::subTaskProgress(Task::Ptr task, qint64 current,
									 qint64 total)
{
	auto step = m_steps.value(task->getUid());
	if (!step) {
		return;
	}
	step->update(current, total);
	emit stepProgress(*step);

	if (totalSize() == 1) {
		setProgress(current, total);
	}
}

bool ConcurrentTask::canAbort() const
{
	// Anything still queued can simply be dropped, so only tasks already on
	// their way have a say.
	for (auto& task : m_doing) {
		if (!task->canAbort()) {
			return false;
		}
	}
	return true;
}

bool ConcurrentTask::abort()
{
	m_aborted = true;
	dropQueued();

	bool fully_aborted = true;
	// Copy: aborting a child can finish it right away and change the container
	// under us.
	const auto running = m_doing.values();
	for (auto& task : running) {
		if (task->canAbort()) {
			fully_aborted &= task->abort();
		} else {
			fully_aborted = false;
		}
	}

	if (m_doing.isEmpty()) {
		finishTask();
	}

	return fully_aborted;
}
