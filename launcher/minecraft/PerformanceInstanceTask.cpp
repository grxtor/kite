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

#include "PerformanceInstanceTask.h"

#include "BaseVersion.h"
#include "BaseVersionList.h"
#include "InstanceCreationTask.h"
#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PerformanceSetupTask.h"
#include "modplatform/BundleCatalog.h"

namespace
{
	const QString kMinecraftUid = QStringLiteral("net.minecraft");
	const QString kFabricUid = QStringLiteral("net.fabricmc.fabric-loader");
	const QString kPerformancePreset = QStringLiteral("performance");
} // namespace

PerformanceInstanceTask::PerformanceInstanceTask(const QString& mcVersion,
												 const QString& name,
												 const QString& group,
												 QObject* parent)
	: Task(parent), m_mcVersion(mcVersion), m_name(name), m_group(group)
{
}

void PerformanceInstanceTask::executeTask()
{
	setStatus(tr("Creating %1").arg(m_name));
	setProgress(0, 0);
	loadThen(kMinecraftUid, &PerformanceInstanceTask::onMinecraftListReady);
}

void PerformanceInstanceTask::loadThen(
	const QString& uid, void (PerformanceInstanceTask::*next)())
{
	auto list = LAUNCHER->metadataIndex()->get(uid);
	if (!list->isLoaded()) {
		m_load = list->getLoadTask();
		if (m_load && !m_load->isFinished()) {
			connect(m_load.get(), &Task::finished, this, next);
			if (!m_load->isRunning()) {
				m_load->start();
			}
			return;
		}
	}
	(this->*next)();
}

void PerformanceInstanceTask::onMinecraftListReady()
{
	if (m_aborted) {
		return;
	}
	auto list = LAUNCHER->metadataIndex()->get(kMinecraftUid);
	if (!list->findVersion(m_mcVersion)) {
		emitFailed(tr("Minecraft %1 is not available").arg(m_mcVersion));
		return;
	}
	loadThen(kFabricUid, &PerformanceInstanceTask::onFabricListReady);
}

void PerformanceInstanceTask::onFabricListReady()
{
	if (m_aborted) {
		return;
	}
	/* The one a new Fabric instance would get: the recommended build, else
	 * the newest. */
	auto list = LAUNCHER->metadataIndex()->get(kFabricUid);
	BaseVersionPtr version = list->getRecommended();
	if (!version && list->count() > 0) {
		version = list->at(0);
	}
	if (!version) {
		emitFailed(tr("Could not find a Fabric version"));
		return;
	}
	m_loaderVersion = version->descriptor();
	createInstance();
}

void PerformanceInstanceTask::createInstance()
{
	auto list = LAUNCHER->metadataIndex()->get(kMinecraftUid);
	auto* creation = new InstanceCreationTask(list->findVersion(m_mcVersion),
											  kFabricUid, m_loaderVersion);
	creation->setName(m_name);
	creation->setGroup(m_group);
	creation->setTargetDir(LAUNCHER->instances()->primaryDir());
	m_creation = Task::Ptr(LAUNCHER->instances()->wrapInstanceTask(creation));

	/* The final id is settled when InstanceList commits the staged
	 * instance, announced just before the task reports success - the same
	 * capture NewInstanceController::create() does. */
	m_idCapture = connect(
		LAUNCHER->instances().get(), &InstanceList::instanceSelectRequest, this,
		[this](const QString& id) {
			m_instanceId = id;
			emit instanceCreated(id);
		},
		Qt::SingleShotConnection);
	connect(m_creation.get(), &Task::finished, this,
			[this] { QObject::disconnect(m_idCapture); });
	connect(m_creation.get(), &Task::status, this, &Task::setStatus);
	connect(m_creation.get(), &Task::progress, this, &Task::setProgress);
	connect(m_creation.get(), &Task::succeeded, this,
			&PerformanceInstanceTask::onCreated);
	connect(m_creation.get(), &Task::failed, this,
			[this](QString reason) { emitFailed(reason); });
	m_creation->start();
}

void PerformanceInstanceTask::onCreated()
{
	auto instance = std::dynamic_pointer_cast<MinecraftInstance>(
		LAUNCHER->instances()->getInstanceById(m_instanceId));
	if (!instance) {
		emitFailed(tr("The new instance could not be found"));
		return;
	}

	BundleCatalog catalog;
	catalog.loadCached();
	catalog.applyPresetDefaults(m_instanceId, kPerformancePreset);

	m_setup.reset(new PerformanceSetupTask(instance));
	connect(m_setup.get(), &Task::finished, this,
			&PerformanceInstanceTask::onSetupFinished);
	connect(m_setup.get(), &Task::status, this, &Task::setStatus);
	connect(m_setup.get(), &Task::progress, this, &Task::setProgress);
	m_setup->start();
}

void PerformanceInstanceTask::onSetupFinished()
{
	if (m_aborted) {
		return;
	}
	/* The setup never fails; how many mods it added is all that differs. */
	const int added = m_setup->added();
	setStatus(added > 0
				  ? tr("Created %1 with %n performance mod(s)", "", added)
						.arg(m_name)
				  : tr("Created %1").arg(m_name));
	emitSucceeded();
}

bool PerformanceInstanceTask::abort()
{
	if (isFinished()) {
		return false;
	}
	m_aborted = true;
	if (m_load) {
		m_load->abort();
	}
	if (m_creation && m_creation->isRunning()) {
		m_creation->abort();
	}
	if (m_setup && m_setup->isRunning()) {
		m_setup->abort();
	}
	emitAborted();
	return true;
}
