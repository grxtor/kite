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

#include "tasks/Task.h"
#include "net/NetJob.h"
#include <QUrl>
#include <QFuture>
#include <QFutureWatcher>
#include "settings/SettingsObject.h"
#include "BaseVersion.h"
#include "BaseInstance.h"
#include "InstanceTask.h"

class InstanceCopyTask : public InstanceTask
{
	Q_OBJECT
  public:
	explicit InstanceCopyTask(InstancePtr origInstance, bool copySaves,
							  bool keepPlaytime);

  protected:
	virtual void executeTask() override;
	void copyFinished();
	void copyAborted();

  private:
	InstancePtr m_origInstance;
	QFuture<bool> m_copyFuture;
	QFutureWatcher<bool> m_copyFutureWatcher;
	std::unique_ptr<IPathMatcher> m_matcher;
	bool m_keepPlaytime;
};
