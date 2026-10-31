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

#include <QFuture>
#include <QFutureWatcher>
#include <QString>

#include "backup/BackupManager.h"
#include "tasks/Task.h"

/* Creates one instance backup on a worker thread, so compressing a whole
 * instance does not freeze the GUI. QtConcurrent::run() plus a
 * QFutureWatcher, as in PluginHookTask / InstanceCopyTask, with progress
 * marshalled back onto the task's thread. */
class BackupTask : public Task
{
	Q_OBJECT
  public:
	/*! \param label Optional label folded into the backup file name, e.g.
	 * "pre-launch". */
	BackupTask(QString instanceId, QString instanceRoot, QString label = {},
			   QObject* parent = nullptr);
	~BackupTask() override;

	/*! MMCZip::compressDir() has no interruption point, and a partial archive
	 * is useless. */
	bool canAbort() const override
	{
		return false;
	}

	/*! The backup created; only meaningful after the task succeeded. */
	const BackupEntry& result() const
	{
		return m_result;
	}

  protected:
	void executeTask() override;

  private:
	/* Called on the worker thread; hops to our thread before touching what
	 * the UI is connected to. */
	void reportProgress(const QString& status, const QString& details,
						qint64 current, qint64 total);
	void backupFinished();

	QString m_instanceId;
	QString m_instanceRoot;
	QString m_label;
	BackupEntry m_result;

	QFuture<BackupEntry> m_future;
	QFutureWatcher<BackupEntry> m_watcher;
};
