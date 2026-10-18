/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "CreateBackup.h"

#include "BaseInstance.h"
#include "launch/LaunchTask.h"

void CreateBackup::executeTask()
{
	auto instance = m_parent->instance();
	if (!instance) {
		emitSucceeded();
		return;
	}

	emit logLine(tr("Creating a pre-launch backup of the instance...") +
					 QStringLiteral("\n"),
				 MessageLevel::Kite);

	m_backupTask.reset(new BackupTask(instance->id(), instance->instanceRoot(),
									  QStringLiteral("pre-launch")));

	connect(m_backupTask.get(), &Task::finished, this,
			&CreateBackup::backupFinished);
	connect(m_backupTask.get(), &Task::progress, this, &Task::setProgress);
	connect(m_backupTask.get(), &Task::status, this, &Task::setStatus);
	connect(m_backupTask.get(), &Task::details, this, &Task::setDetails);

	// We are only a wrapper around the backup itself, so its steps have
	// to carry on through us.
	propagateStepsFrom(m_backupTask.get());
	emit progressReportingRequest();
}

void CreateBackup::proceed()
{
	m_backupTask->start();
}

void CreateBackup::backupFinished()
{
	if (m_backupTask->wasSuccessful()) {
		emit logLine(tr("Pre-launch backup created: %1")
						 .arg(m_backupTask->result().fileName) +
						 QStringLiteral("\n"),
					 MessageLevel::Kite);
	} else {
		// Refusing to start the game because a convenience snapshot
		// could not be written would be the worse failure of the two,
		// and it is what the plugin this replaces did as well: warn and
		// carry on.
		emit logLine(tr("Pre-launch backup failed: %1")
						 .arg(m_backupTask->failReason()) +
						 QStringLiteral("\n"),
					 MessageLevel::Warning);
	}

	// Deliberately not resetting m_backupTask here: we are inside its
	// own finished() emission. It dies with this step.
	emitSucceeded();
}
