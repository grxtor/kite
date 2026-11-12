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

#include "InstanceCopyTask.h"
#include "settings/INISettingsObject.h"
#include "FileSystem.h"
#include "NullInstance.h"
#include "pathmatcher/RegexpMatcher.h"
#include "launcherimport/ImportBuild.h"
#include "launcherimport/ImportCopy.h"
#include "launcherimport/LinkedGame.h"
#include <QtConcurrentRun>

InstanceCopyTask::InstanceCopyTask(InstancePtr origInstance, bool copySaves,
								   bool keepPlaytime)
{
	m_origInstance = origInstance;
	m_copySaves = copySaves;
	m_keepPlaytime = keepPlaytime;

	if (!copySaves) {
		// FIXME: get this from the original instance type...
		auto matcherReal = new RegexpMatcher("[.]?minecraft/saves");
		matcherReal->caseSensitive(false);
		m_matcher.reset(matcherReal);
	}
}

void InstanceCopyTask::executeTask()
{
	setStatus(tr("Copying instance %1").arg(m_origInstance->name()));

	FS::copy folderCopy(m_origInstance->instanceRoot(), m_stagingPath);
	folderCopy.followSymlinks(false).blacklist(m_matcher.get());

	/* An instance imported with Link plays in another launcher's folder; its
	 * duplicate gets those files as its own (what a Copy import takes) instead
	 * of a second link to the same folder. */
	const QString linkedGame = LauncherImport::outsideGameRoot(*m_origInstance);
	const QStringList leaveOut =
		m_copySaves ? QStringList() : QStringList{QStringLiteral("saves")};
	const QString ownGame = FS::PathCombine(m_stagingPath, "minecraft");

	m_copyFuture = QtConcurrent::run(
		QThreadPool::globalInstance(),
		[folderCopy, linkedGame, leaveOut, ownGame]() mutable {
			if (!folderCopy()) {
				return false;
			}
			if (linkedGame.isEmpty()) {
				return true;
			}
			const std::atomic_bool never{false};
			const LauncherImport::CopyPlan plan =
				LauncherImport::planCopy(linkedGame, nullptr, leaveOut);
			QString error;
			return !plan.tooLarge &&
				   LauncherImport::runCopy(plan, ownGame, never, nullptr, &error);
		});
	connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::finished, this,
			&InstanceCopyTask::copyFinished);
	connect(&m_copyFutureWatcher, &QFutureWatcher<bool>::canceled, this,
			&InstanceCopyTask::copyAborted);
	m_copyFutureWatcher.setFuture(m_copyFuture);
}

void InstanceCopyTask::copyFinished()
{
	auto successful = m_copyFuture.result();
	if (!successful) {
		emitFailed(tr("Instance folder copy failed."));
		return;
	}
	// FIXME: shouldn't this be able to report errors?
	auto instanceSettings = std::make_shared<INISettingsObject>(
		FS::PathCombine(m_stagingPath, "instance.cfg"));
	instanceSettings->registerSetting("InstanceType", "Legacy");
	/* A duplicate stands on its own, linked or not. */
	instanceSettings->registerSetting(LauncherImport::kLinkedGameDirKey, "");
	instanceSettings->reset(LauncherImport::kLinkedGameDirKey);

	InstancePtr inst(
		new NullInstance(m_globalSettings, instanceSettings, m_stagingPath));
	inst->setName(m_instName);
	inst->setIconKey(m_instIcon);
	if (!m_keepPlaytime) {
		inst->resetTimePlayed();
	}
	emitSucceeded();
}

void InstanceCopyTask::copyAborted()
{
	emitFailed(tr("Instance folder copy has been aborted."));
	return;
}
