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

#include <QFutureWatcher>
#include <QTimer>

#include <atomic>
#include <memory>

#include "ImportBuild.h"
#include "InstanceTask.h"

/*
 * One instance brought over from another launcher, as an InstanceTask: the
 * instance list stages it, and moves the finished folder into place only when
 * it succeeds (a cancelled or failed import leaves nothing behind). The work
 * runs on a worker thread; progress is polled from it, since the copy reports
 * in bytes many times a second.
 */
class LauncherImportTask : public InstanceTask
{
	Q_OBJECT
  public:
	explicit LauncherImportTask(const LauncherImport::BuildRequest& request);
	~LauncherImportTask() override;

	/* Stoppable throughout: nothing reaches the instance list until it ends. */
	bool canAbort() const override
	{
		return true;
	}

	LauncherImport::BuildResult result() const
	{
		return m_result;
	}

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

  private:
	void reportProgress();
	void built();

	LauncherImport::BuildRequest m_request;
	LauncherImport::BuildResult m_result;
	/* Shared with the worker, which may still be stopping when this task is
	 * destroyed. */
	std::shared_ptr<std::atomic_bool> m_cancel;
	std::shared_ptr<LauncherImport::BuildProgress> m_progress;
	QFutureWatcher<LauncherImport::BuildResult> m_watcher;
	QTimer m_progressTimer;
};
