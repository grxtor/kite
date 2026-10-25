/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include "QObjectPtr.h"

class PerformanceSetupTask;

/*
 * The launch step for Kite Performance: runs PerformanceSetupTask before the
 * update, so the performance mods are in place when the update resolves
 * the components. An instance without a mod loader is left as it is. Says
 * in the launch log what it did. It never fails the launch.
 */
class PreparePerformance : public LaunchStep
{
	Q_OBJECT
  public:
	explicit PreparePerformance(LaunchTask* parent) : LaunchStep(parent) {}
	~PreparePerformance() override = default;

	void executeTask() override;
	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  private:
	void setupFinished();

	shared_qobject_ptr<PerformanceSetupTask> m_setup;
};
