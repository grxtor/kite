/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#include "MinecraftUpdate.h"
#include "MinecraftInstance.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDataStream>

#include "BaseInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/Library.h"
#include <FileSystem.h>

#include "update/FoldersTask.h"
#include "update/LibrariesTask.h"
#include "update/FMLLibrariesTask.h"
#include "update/AssetUpdateTask.h"

#include <meta/Index.h>
#include <meta/Version.h>

MinecraftUpdate::MinecraftUpdate(MinecraftInstance* inst, QObject* parent)
	: SequentialTask(parent, QStringLiteral("MinecraftUpdate")), m_inst(inst)
{
}

void MinecraftUpdate::executeTask()
{
	// create folders
	{
		addStage(Task::Ptr(new FoldersTask(m_inst)), tr("Preparing folders"));
	}

	// add metadata update task if necessary
	{
		auto components = m_inst->getPackProfile();
		components->reload(Net::Mode::Online);
		auto task = components->getCurrentTask();
		if (task) {
			addStage(task, tr("Loading version info"));
		}
	}

	// libraries download
	{
		addStage(Task::Ptr(new LibrariesTask(m_inst)),
				 tr("Downloading libraries"));
	}

	// FML libraries download and copy into the instance
	{
		addStage(Task::Ptr(new FMLLibrariesTask(m_inst)),
				 tr("Downloading Forge libraries"));
	}

	// assets update
	{
		addStage(Task::Ptr(new AssetUpdateTask(m_inst)),
				 tr("Downloading assets"));
	}

	SequentialTask::executeTask();
}

void MinecraftUpdate::addStage(Task::Ptr task, const QString& stage)
{
	m_stages.insert(task.get(), stage);
	addTask(task);
}

void MinecraftUpdate::updateState()
{
	// Between two steps, and once the last one is done, the step that just
	// ran keeps the word: there is no other step to name.
	if (m_doing.isEmpty()) {
		return;
	}
	// Sequential: the one step under way.
	Task* current = m_doing.cbegin().key();
	const QString stage = m_stages.value(current);
	setStatus(stage.isEmpty() ? current->getStatus() : stage);
	const qint64 total = current->getTotalProgress();
	setProgress(total > 0 ? current->getProgress() : 0, qMax<qint64>(total, 0));
}

void MinecraftUpdate::subTaskProgress(Task::Ptr task, qint64 current,
									  qint64 total)
{
	SequentialTask::subTaskProgress(task, current, total);
	setProgress(current, total);
}

bool MinecraftUpdate::abort()
{
	SequentialTask::abort();
	// A step that cannot be interrupted still has to run to completion, but
	// the update as a whole stops either way, so from the caller's point of
	// view this always works.
	return true;
}

bool MinecraftUpdate::canAbort() const
{
	return true;
}
