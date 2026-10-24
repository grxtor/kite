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

#include <QMetaObject>
#include <QString>

#include "QObjectPtr.h"
#include "tasks/Task.h"

class PerformanceSetupTask;

/*
 * "Speed up" for a vanilla instance: the instance itself is never changed.
 * This makes a separate one instead - empty (no worlds, nothing copied),
 * on the same Minecraft version, with the newest Fabric loader - gives it
 * the Performance preset's memory and options.txt with the mode on, and
 * installs the performance mods into it (PerformanceSetupTask).
 *
 * One task for the whole chain, so a single TaskWatcher can follow it. The
 * new instance's id is announced by instanceCreated() as soon as it exists,
 * before the task succeeds; the final status is the summary sentence.
 * Failing to create the instance fails the task; the mods are best effort,
 * like everywhere in Kite Performance.
 */
class PerformanceInstanceTask : public Task
{
	Q_OBJECT

  public:
	/// @p name and @p group are the new instance's.
	PerformanceInstanceTask(const QString& mcVersion, const QString& name,
							const QString& group, QObject* parent = nullptr);
	~PerformanceInstanceTask() override = default;

	/// The new instance's id; empty until instanceCreated().
	QString instanceId() const
	{
		return m_instanceId;
	}

	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  signals:
	void instanceCreated(const QString& id);

  protected:
	void executeTask() override;

  private:
	/// Loads the metadata list @p uid if it is not loaded yet, then calls
	/// @p next.
	void loadThen(const QString& uid, void (PerformanceInstanceTask::*next)());
	void onMinecraftListReady();
	void onFabricListReady();
	void createInstance();
	void onCreated();
	void onSetupFinished();

	QString m_mcVersion;
	QString m_name;
	QString m_group;
	QString m_loaderVersion;
	QString m_instanceId;
	bool m_aborted = false;

	Task::Ptr m_load;
	Task::Ptr m_creation;
	shared_qobject_ptr<PerformanceSetupTask> m_setup;
	QMetaObject::Connection m_idCapture;
};
