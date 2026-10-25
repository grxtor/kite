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

#include <launch/LaunchStep.h>
#include <QObjectPtr.h>
#include <LoggedProcess.h>
#include <java/JavaChecker.h>
#include <net/Mode.h>

#include <functional>

// FIXME: stupid. should be defined by the instance type? or even completely
// abstracted away...
class Update : public LaunchStep
{
	Q_OBJECT
  public:
	explicit Update(LaunchTask* parent, Net::Mode mode)
		: LaunchStep(parent), m_mode(mode) {};
	/// Runs the task @p makeTask makes instead of the instance's whole update.
	explicit Update(LaunchTask* parent, std::function<Task::Ptr()> makeTask)
		: LaunchStep(parent), m_makeTask(std::move(makeTask)) {};
	virtual ~Update() {};

	void executeTask() override;
	bool canAbort() const override;
	void proceed() override;

	// We are a stand-in for the update task, so answer for it. Without this
	// a progress dialog opened partway through has no steps to catch up on.
	bool isMultiStep() const override
	{
		return m_updateTask && m_updateTask->isMultiStep();
	}
	TaskStepProgressList getStepProgress() const override
	{
		if (!m_updateTask) {
			return {};
		}
		return m_updateTask->getStepProgress();
	}

  public slots:
	bool abort() override;

  private slots:
	void updateFinished();

  private:
	Task::Ptr m_updateTask;
	bool m_aborted = false;
	Net::Mode m_mode = Net::Mode::Offline;
	std::function<Task::Ptr()> m_makeTask;
};
