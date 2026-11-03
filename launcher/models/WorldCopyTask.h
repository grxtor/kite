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

#include <QFuture>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <functional>

#include "tasks/Task.h"

/* Runs World::install() off the GUI thread (world files can run into gigabytes). */
class WorldCopyTask : public Task
{
	Q_OBJECT
  public:
	using Fn = std::function<bool()>;

	WorldCopyTask(QString status, Fn fn, QObject* parent = nullptr)
		: Task(parent), m_fn(std::move(fn))
	{
		setObjectName(QStringLiteral("WorldCopyTask"));
		setStatus(status);
		setProgress(0, 0);
	}

	~WorldCopyTask() override
	{
		disconnect(&m_watcher, nullptr, this, nullptr);
		if (m_future.isRunning()) {
			// The worker only owns copies of what it needs; let it finish unwatched.
			auto* holder = new QFutureWatcher<bool>();
			connect(holder, &QFutureWatcher<bool>::finished, holder,
					&QObject::deleteLater);
			holder->setFuture(m_future);
		}
	}

  protected:
	void executeTask() override
	{
		connect(&m_watcher, &QFutureWatcher<bool>::finished, this, [this] {
			if (m_future.result()) {
				setProgress(1, 1);
				emitSucceeded();
			} else {
				emitFailed(tr("The operation failed. See the launcher log "
							  "for details."));
			}
		});
		m_future = QtConcurrent::run(QThreadPool::globalInstance(), m_fn);
		m_watcher.setFuture(m_future);
	}

  private:
	Fn m_fn;
	QFuture<bool> m_future;
	QFutureWatcher<bool> m_watcher;
};
