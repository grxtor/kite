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
	 * instance is already built, and only its optional head start is
	 * being given up. So the task ends the way it would have ended
	 * anyway - the "finished" handler below sees a task that did not
	 * succeed, says so as a warning, and succeeds. */
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
		/* Nothing of the sort exists for this instance type. */
		emitSucceeded();
		return;
	}

	/* Held as a member for two reasons: the download outlives this
	 * function, and canAbort() reads it to know whether the abort button
	 * means anything at this moment. */
	m_gameFilesTask = task;

	/* Now that there is something to abort, say so - and say what
	 * pressing it actually does.
	 *
	 * "Skip", not "Abort": by this point the instance exists and is
	 * complete. The button gives up the head start on the game's files,
	 * nothing else, and the task goes on to succeed either way. A button
	 * still labelled "Abort" here would be offering to undo an install
	 * that is already finished. */
	reportAbortStatus();
	setAbortButtonText(tr("Skip"));

	connect(task.get(), &Task::finished, this, [this]() {
		if (!isRunning()) {
			/* Already over. Something else has finished this task while
			 * the download was in flight, and a second ending would have
			 * the dialog react twice to one install. */
			return;
		}

		auto finished = m_gameFilesTask;
		/* Cleared before finishing, so canAbort() stops claiming there
		 * is something left to abort. */
		m_gameFilesTask.reset();
		/* And the button stops offering it. Left enabled it would be a
		 * button that does nothing, during the commit that follows. */
		reportAbortStatus();

		if (finished && !finished->wasSuccessful()) {
			/* A warning rather than a failure - see the header. The pack
			 * is installed either way and the next launch fetches what
			 * is missing, exactly as it did before this step existed. */
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
	/* Keeps the individual downloads visible in the dialog rather than
	 * collapsing them into one opaque bar. */
	propagateStepsFrom(task.get());

	setStatus(tr("Downloading the game's files..."));
	task->start();
}

void InstanceTask::downloadFiles(std::shared_ptr<MinecraftInstance> instance)
{
	/* Taken before the work starts, and deliberately not released when it
	 * ends: the task itself is destroyed shortly after it finishes, and
	 * letting the instance go with it is one less order-of-teardown rule
	 * to get right. */
	m_ownedInstance = std::move(instance);
	downloadFiles(m_ownedInstance.get());
}
