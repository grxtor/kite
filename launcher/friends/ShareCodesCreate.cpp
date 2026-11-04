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

/* ShareCodesController: a new instance from the code lookUp() found. */

#include "ShareCodesController.h"

#include "ShareCodeInstanceTask.h"
#include "core/LauncherContext.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/Component.h"
#include "models/NewInstanceController.h"
#include "net/Mode.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/* Missing content named in the toast; more is "and N more". */
	constexpr int kNamedMissing = 3;
} // namespace

QString ShareCodesController::loaderUid(const QString& loader) const
{
	for (const ModLoaderInfo& info : knownModLoaders()) {
		if (!info.platformId.isEmpty() && info.platformId == loader) {
			return info.uid;
		}
	}
	return QString();
}

QObject* ShareCodesController::createInstance(const QString& name,
											  const QString& group,
											  const QString& iconKey)
{
	if (!m_lookedUp) {
		finishAction("createInstance", false, tr("Look up a code first"));
		return nullptr;
	}
	const ShareCodes::Manifest manifest = m_lookedUp->manifest;
	auto list = LAUNCHER->metadataIndex()->get(QStringLiteral("net.minecraft"));
	BaseVersionPtr version = list ? list->findVersion(manifest.minecraftVersion) : nullptr;
	if (!version) {
		if (list && !list->isLoaded()) {
			list->load(Net::Mode::Online);
			finishAction("createInstance", false,
						 tr("Kite is still loading the Minecraft versions; try again in a moment"));
		} else {
			finishAction("createInstance", false,
						 tr("Minecraft %1 can't be installed").arg(manifest.minecraftVersion));
		}
		return nullptr;
	}
	auto* task = new ShareCodeInstanceTask(version, loaderUid(manifest.loader),
										   manifest.loaderVersion, manifest);
	task->setName(name);
	task->setGroup(group);
	task->setIcon(iconKey);
	/* Read as the task ends: the instance list's wrapper outlives it. */
	m_lastMissing.clear();
	connect(task, &Task::succeeded, this,
			[this, task]() { m_lastMissing = task->missing(); });
	TaskWatcher* watcher = startInstanceCreation(task, this);
	connect(watcher, &TaskWatcher::finished, this, [this, name](bool ok) {
		const QStringList missing = m_lastMissing;
		if (!ok || missing.isEmpty()) {
			return;
		}
		const QString named = missing.mid(0, kNamedMissing).join(QStringLiteral(", "));
		const int more = int(missing.size()) - kNamedMissing;
		finishAction("createInstance", true,
					 more > 0 ? tr("%1 is ready without %2 and %3 more: they are gone "
								   "from Modrinth or CurseForge")
									.arg(name, named)
									.arg(more)
							  : tr("%1 is ready without %2: gone from Modrinth or "
								   "CurseForge")
									.arg(name, named));
	});
	return watcher;
}
