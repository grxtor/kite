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

#include "backup/BackupTask.h"
#include "launch/LaunchStep.h"

/*
 * Takes a "pre-launch" snapshot of the instance before the game starts. Only
 * prepended by LaunchController when BackupBeforeLaunch is on. A step because
 * BackupTask writes the archive on a worker thread and a step puts its progress
 * in front of the user while the launch waits.
 */
class CreateBackup : public LaunchStep
{
	Q_OBJECT
  public:
	explicit CreateBackup(LaunchTask* parent) : LaunchStep(parent) {}
	~CreateBackup() override = default;

	void executeTask() override;
	void proceed() override;

	bool canAbort() const override
	{
		return false;
	}

  private slots:
	void backupFinished();

  private:
	shared_qobject_ptr<BackupTask> m_backupTask;
};
