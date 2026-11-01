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

#include "SequentialTask.h"

SequentialTask::SequentialTask(QObject* parent, QString task_name)
	: ConcurrentTask(parent, task_name, 1)
{
}

void SequentialTask::updateState()
{
	if (totalSize() <= 1) {
		// One step is not worth counting out loud; let it speak for itself
		// like a plain ConcurrentTask.
		ConcurrentTask::updateState();
		return;
	}

	setProgress(finishedSize(), totalSize());
	setStatus(tr("Executing task %1 out of %2")
				  .arg(QString::number(m_doing.size() + finishedSize()),
					   QString::number(totalSize())));
}

void SequentialTask::subTaskFailed(Task::Ptr task, const QString& reason)
{
	// Every step builds on the one before, so nothing is left worth running.
	dropQueued();
	ConcurrentTask::subTaskFailed(task, reason);
}
