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

#include "plugin/PluginHooks.h"
#include "tasks/Task.h"

/** Runs one plugin hook callback that opted into MMCO_HOOK_FLAG_BACKGROUND on a worker thread, as an
 * ordinary task. PluginManager builds one per background callback and hands them to a
 * SequentialTask, behind one busy indication (per-plugin progress is not shown). A non-zero return is a veto: the
 * task fails, which stops the rest of the sequence, as an inline veto stops the hook chain. */
class PluginHookTask : public Task
{
	Q_OBJECT
  public:
	PluginHookTask(QString module_name, void* module_handle, uint32_t hook_id,
				   MMCOHookCallback callback, void* payload, void* user_data,
				   QObject* parent = nullptr);
	~PluginHookTask() override;

	/*! A C callback cannot be interrupted, so there is nothing to abort. */
	bool canAbort() const override
	{
		return false;
	}

	/*! True when the callback returned non-zero (the plugin asked for the hook's action to be called
	 * off); meaningful only once the task has finished. */
	bool cancelRequested() const
	{
		return m_cancelRequested;
	}

	void* moduleHandle() const
	{
		return m_moduleHandle;
	}

	/*! The task whose callback runs on the calling thread, or nullptr outside a background hook
	 * callback. This is how MMCOContext::progress_report finds its row without a progress handle in
	 * the ABI. */
	static PluginHookTask* currentOnThisThread();

	/*! Publishes a progress update from the plugin. Called on the worker thread; marshalled onto the
	 * task's thread. Empty status/details keep the current text. */
	void reportProgress(const QString& status, const QString& details,
						qint64 current, qint64 total);

  protected:
	void executeTask() override;

  private:
	void callbackFinished();

	QString m_moduleName;
	void* m_moduleHandle;
	uint32_t m_hookId;
	MMCOHookCallback m_callback;
	void* m_payload;
	void* m_userData;

	bool m_cancelRequested = false;

	QFuture<int> m_future;
	QFutureWatcher<int> m_watcher;
};
