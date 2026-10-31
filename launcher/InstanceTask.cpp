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

#include "InstanceTask.h"

#include "minecraft/MinecraftInstance.h"
#include "net/Mode.h"

#include <QDebug>

#include <utility>

InstanceTask::InstanceTask() {}

InstanceTask::~InstanceTask() {}

bool InstanceTask::abort()
{
	if (!m_gameFilesTask) {
		return false;
	}

	/* Stopping the game-file download is not cancelling the install: the
	 * instance is built and only its optional head start is given up. The
	 * "finished" handler below sees a task that did not succeed, warns, and
	 * succeeds. */
	return m_gameFilesTask->abort();
}

void InstanceTask::downloadFiles(MinecraftInstance* instance)
{
	if (!instance || !m_downloadGameFiles ||
		!m_globalSettings->get("DownloadGameFilesDuringInstanceCreation")
			 .toBool()) {
		emitSucceeded();
		return;
	}

	auto task = instance->createUpdateTask(Net::Mode::Online);
	if (!task) {
		emitSucceeded();
		return;
	}

	/* Held as a member because the download outlives this function and
	 * canAbort() reads it. */
	m_gameFilesTask = task;

	/* Now that there is something to abort, say what pressing it does: "Skip",
	 * not "Abort", since the instance is complete and the button only gives up
	 * the head start on the game's files. */
	reportAbortStatus();
	setAbortButtonText(tr("Skip"));

	connect(task.get(), &Task::finished, this, [this]() {
		if (!isRunning()) {
			/* Already over: something else finished this task while the
			 * download was in flight, and a second ending would make the
			 * dialog react twice. */
			return;
		}

		auto finished = m_gameFilesTask;
		/* Cleared before finishing, so canAbort() stops claiming there is
		 * something to abort. */
		m_gameFilesTask.reset();
		/* And the button stops offering it; left enabled it would do nothing
		 * during the commit that follows. */
		reportAbortStatus();

		if (finished && !finished->wasSuccessful()) {
			/* A warning, not a failure: the pack is installed either way and
			 * the next launch fetches what is missing. */
			const QString reason = finished->failReason();
			qWarning() << "Could not pre-download game files:" << reason;
			logWarning(tr("The instance was installed, but its game files "
						  "could not be downloaded yet: %1\n"
						  "They will be downloaded when you launch it.")
						   .arg(reason));
		}
		emitSucceeded();
	});
	connect(task.get(), &Task::status, this, &InstanceTask::setStatus);
	connect(task.get(), &Task::progress, this,
			[this](qint64 current, qint64 total) {
				setProgress(current, total);
			});
	propagateStepsFrom(task.get());

	setStatus(tr("Downloading the game's files..."));
	task->start();
}

void InstanceTask::downloadFiles(std::shared_ptr<MinecraftInstance> instance)
{
	/* Taken before the work starts and deliberately not released when it ends:
	 * the task is destroyed shortly after finishing, and letting the instance
	 * go with it is one less teardown-order rule. */
	m_ownedInstance = std::move(instance);
	downloadFiles(m_ownedInstance.get());
}
