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

#include "Update.h"
#include <launch/LaunchTask.h>

void Update::executeTask()
{
	if (m_aborted) {
		emitFailed(tr("Task aborted."));
		return;
	}
	m_updateTask = m_makeTask
					   ? m_makeTask()
					   : m_parent->instance()->createUpdateTask(m_mode);
	if (m_updateTask) {
		connect(m_updateTask.get(), &Task::finished, this,
				&Update::updateFinished);
		connect(m_updateTask.get(), &Task::progress, this, &Task::setProgress);
		connect(m_updateTask.get(), &Task::status, this, &Task::setStatus);
		connect(m_updateTask.get(), &Task::details, this, &Task::setDetails);
		// We are only a wrapper around the update itself, so its steps have
		// to carry on through us.
		propagateStepsFrom(m_updateTask.get());
		emit progressReportingRequest();
		return;
	}
	emitSucceeded();
}

void Update::proceed()
{
	m_updateTask->start();
}

void Update::updateFinished()
{
	if (m_updateTask->wasSuccessful()) {
		emitSucceeded();
		m_updateTask.reset();
	} else {
		QString reason = tr("Instance update failed because: %1\n\n")
							 .arg(m_updateTask->failReason());
		emit logLine(reason, MessageLevel::Fatal);
		emitFailed(reason);
		m_updateTask.reset();
	}
}

bool Update::canAbort() const
{
	if (m_updateTask) {
		return m_updateTask->canAbort();
	}
	return true;
}

bool Update::abort()
{
	m_aborted = true;
	if (m_updateTask) {
		if (m_updateTask->canAbort()) {
			return m_updateTask->abort();
		}
	}
	return true;
}
