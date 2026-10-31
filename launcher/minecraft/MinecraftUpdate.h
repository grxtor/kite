/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include <QObject>
#include <QList>
#include <QHash>
#include <QUrl>

#include "net/NetJob.h"
#include "tasks/SequentialTask.h"
#include "minecraft/VersionFilterData.h"

class MinecraftVersion;
class MinecraftInstance;

/**
 * Brings an instance up to date: folders, metadata, libraries, FML libraries
 * and assets, in that order, each its own step.
 *
 * Its status and progress describe the step under way (its stage, e.g.
 * "Downloading assets", and how far it is) rather than "Executing task 4 out
 * of 5"; a launch relays them as they are (LaunchTask, InstanceList) to the
 * play bar.
 */
class MinecraftUpdate : public SequentialTask
{
	Q_OBJECT
  public:
	/// What to bring up to date. A launch does it in two goes: version info
	/// first, then the files once the Java is settled (the version info says
	/// which Java a version needs, which decides the natives).
	enum class Part { Everything, VersionInfo, GameFiles };

	explicit MinecraftUpdate(MinecraftInstance* inst,
							 Part part = Part::Everything, QObject* parent = 0);
	virtual ~MinecraftUpdate() {};

	bool canAbort() const override;

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;
	void updateState() override;
	void subTaskProgress(Task::Ptr task, qint64 current,
						 qint64 total) override;

  private:
	/// Queues @p task as one stage of the update, named @p stage.
	void addStage(Task::Ptr task, const QString& stage);

	MinecraftInstance* m_inst = nullptr;
	Part m_part = Part::Everything;
	QHash<Task*, QString> m_stages;
};
