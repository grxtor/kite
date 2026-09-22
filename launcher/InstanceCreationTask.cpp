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

#include "InstanceCreationTask.h"
#include "settings/INISettingsObject.h"
#include "FileSystem.h"

// FIXME: remove this
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"

InstanceCreationTask::InstanceCreationTask(BaseVersionPtr version,
										   const QString& loaderUid,
										   const QString& loaderVersion)
{
	m_version = version;
	m_loaderUid = loaderUid;
	m_loaderVersion = loaderVersion;
}

void InstanceCreationTask::executeTask()
{
	setStatus(tr("Creating instance from version %1").arg(m_version->name()));

	/* Outlives the scope below on purpose: this function ends by handing
	 * the instance to downloadFiles(), which runs asynchronously against
	 * it. The scope is still what it was - everything that writes to
	 * instance.cfg is released before we finish - the instance is simply
	 * no longer part of what the scope owns. */
	std::shared_ptr<MinecraftInstance> instance;
	{
		auto instanceSettings = std::make_shared<INISettingsObject>(
			FS::PathCombine(m_stagingPath, "instance.cfg"));
		instanceSettings->suspendSave();
		instanceSettings->registerSetting("InstanceType", "Legacy");
		instanceSettings->set("InstanceType", "OneSix");
		instance = std::make_shared<MinecraftInstance>(
			m_globalSettings, instanceSettings, m_stagingPath);
		MinecraftInstance& inst = *instance;
		auto components = inst.getPackProfile();
		components->buildingFromScratch();
		components->setComponentVersion("net.minecraft",
										m_version->descriptor(), true);
		if (!m_loaderUid.isEmpty()) {
			/* Same call InstallLoaderDialog::applySelection() makes
			 * (ui/dialogs/InstallLoaderDialog.cpp) - not marked
			 * "important", so the component is exactly as removable and
			 * disableable as one installed after the fact would be. */
			components->setComponentVersion(m_loaderUid, m_loaderVersion);
			components->resolve(Net::Mode::Online);
		}
		inst.setName(m_instName);
		inst.setIconKey(m_instIcon);
		instanceSettings->resumeSave();
	}

	/* Finishes the task, whether or not it downloads anything. */
	downloadFiles(instance);
}
