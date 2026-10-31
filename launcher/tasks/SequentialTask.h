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

#include "ConcurrentTask.h"

/** Runs its tasks one at a time, in the order added, and gives up as soon as
 * one fails; for steps that only make sense if the one before worked. For
 * independent tasks use ConcurrentTask. */
class SequentialTask : public ConcurrentTask
{
	Q_OBJECT
  public:
	explicit SequentialTask(QObject* parent = 0,
							QString task_name = QString());
	virtual ~SequentialTask() {};

  protected:
	void updateState() override;
	void subTaskFailed(Task::Ptr task, const QString& reason) override;
};
