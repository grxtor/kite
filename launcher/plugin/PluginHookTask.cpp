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

#include "plugin/PluginHookTask.h"

#include <QDebug>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <utility>

namespace
{
	/* The background callback running on this thread, if any. progress_report receives only the
	 * module handle and the hook callback signature cannot carry a progress handle, but exactly one
	 * PluginHookTask is on the stack of a worker thread at a time, so the thread identifies it. */
	thread_local PluginHookTask* t_currentHookTask = nullptr;

	struct CurrentHookTaskGuard {
		explicit CurrentHookTaskGuard(PluginHookTask* task)
			: previous(t_currentHookTask)
		{
			t_currentHookTask = task;
		}
		~CurrentHookTaskGuard()
		{
			t_currentHookTask = previous;
		}

		PluginHookTask* previous;
	};
} // namespace

PluginHookTask::PluginHookTask(QString module_name, void* module_handle,
							   uint32_t hook_id, MMCOHookCallback callback,
							   void* payload, void* user_data, QObject* parent)
	: Task(parent), m_moduleName(std::move(module_name)),
	  m_moduleHandle(module_handle), m_hookId(hook_id), m_callback(callback),
	  m_payload(payload), m_userData(user_data)
{
	setObjectName(m_moduleName);
	setStatus(tr("Running %1...").arg(m_moduleName));
	// Duration unknown: sweep until the plugin says otherwise.
	setProgress(0, 0);
}

PluginHookTask::~PluginHookTask()
{
	// The worker holds a bare pointer to us, so it must not outlive us. This blocks only if the
	// task is torn down while its callback runs, which the normal flow never does.
	disconnect(&m_watcher, nullptr, this, nullptr);
	if (m_future.isRunning()) {
		qWarning() << "PluginHookTask" << m_moduleName
				   << "destroyed while its callback was still running -"
				   << "waiting for it to return";
		m_future.waitForFinished();
	}
}

PluginHookTask* PluginHookTask::currentOnThisThread()
{
	return t_currentHookTask;
}

void PluginHookTask::executeTask()
{
	if (!m_callback) {
		emitFailed(tr("%1 registered a hook without a callback.")
					   .arg(m_moduleName));
		return;
	}

	connect(&m_watcher, &QFutureWatcher<int>::finished, this,
			&PluginHookTask::callbackFinished);

	m_future = QtConcurrent::run(QThreadPool::globalInstance(), [this] {
		CurrentHookTaskGuard guard(this);
		return m_callback(m_moduleHandle, m_hookId, m_payload, m_userData);
	});
	m_watcher.setFuture(m_future);
}

void PluginHookTask::reportProgress(const QString& status,
									const QString& details, qint64 current,
									qint64 total)
{
	// Called from the worker thread: hop to the task's thread before touching anything the UI is
	// connected to.
	QMetaObject::invokeMethod(
		this,
		[this, status, details, current, total] {
			if (!status.isEmpty()) {
				setStatus(status);
			}
			if (!details.isEmpty()) {
				setDetails(details);
			}
			setProgress(current, total);
		},
		Qt::QueuedConnection);
}

void PluginHookTask::callbackFinished()
{
	const int rc = m_future.result();
	m_cancelRequested = rc != 0;

	if (m_cancelRequested) {
		// Non-zero is the ABI's veto, not a crash. Failing the task stops the rest of the sequence,
		// as an inline veto stops the hook chain.
		emitFailed(tr("%1 called off the operation.").arg(m_moduleName));
		return;
	}

	setProgress(1, 1);
	emitSucceeded();
}
