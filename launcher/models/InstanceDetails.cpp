/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include "InstanceDetails.h"

#include <QSortFilterProxyModel>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <functional>

#include "core/LauncherContext.h"
#include "launch/LaunchTask.h"
#include "launch/LogModel.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/legacy/LegacyInstance.h"
#include "minecraft/legacy/LegacyUpgradeTask.h"
#include "minecraft/PackProfile.h"
#include "minecraft/WorldList.h"
#include "minecraft/gameoptions/GameOptions.h"
#include "minecraft/mod/ModFolderModel.h"
#include "models/BackupController.h"
#include "models/ComponentTools.h"
#include "models/ContentBrowser.h"
#include "models/ContentDrop.h"
#include "models/InstanceContentController.h"
#include "models/KeyValueFilterModel.h"
#include "models/LegacyUpgrade.h"
#include "models/LogFind.h"
#include "models/LogUploader.h"
#include "models/LoaderInstaller.h"
#include "models/ManagedPackController.h"
#include "models/ModFolderWatch.h"
#include "models/NewInstanceController.h"
#include "models/OtherLogsModel.h"
#include "models/ScreenshotUploader.h"
#include "models/ServersListModel.h"
#include "models/SettingsAdapter.h"
#include "models/WorldDataPacksController.h"
#include "models/WorldCopyTask.h"
#include "models/WorldTools.h"
#include "screenshots/ScreenshotListModel.h"
#include "settings/SettingsObject.h"
#include "tasks/Task.h"
#include "tasks/TaskWatcher.h"
#include "FileSystem.h"
#include "InstanceList.h"

namespace
{
	std::unique_ptr<QSortFilterProxyModel> sortedByName(ModFolderModel* source)
	{
		auto proxy = std::make_unique<QSortFilterProxyModel>();
		proxy->setSourceModel(source);
		proxy->setSortRole(source->roleNames().key("name", Qt::DisplayRole));
		proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
		proxy->setDynamicSortFilter(true);
		proxy->sort(0);
		return proxy;
	}

} // namespace

InstanceDetails::InstanceDetails(InstancePtr instance, QObject* parent)
	: QObject(parent), m_instance(std::move(instance))
{
	if (!m_instance) {
		return;
	}

	// Coarse: propertiesChanged can mean more than a rename; re-reading name is cheap.
	connect(m_instance.get(), &BaseInstance::propertiesChanged, this,
			[this](BaseInstance*) { emit nameChanged(); });
	connect(m_instance.get(), &BaseInstance::runningStatusChanged, this,
			&InstanceDetails::onRunningStatusChanged);

	m_settings = std::make_unique<SettingsAdapter>(m_instance->settings());

	m_mc = dynamic_cast<MinecraftInstance*>(m_instance.get());
	if (m_mc) {
		m_mods = m_mc->loaderModList();
		m_sortedMods = sortedByName(m_mods.get());
		startWatchingFresh(m_mods.get());

		m_resourcePacks = m_mc->resourcePackList();
		startWatchingFresh(m_resourcePacks.get());

		m_shaderPacks = m_mc->shaderPackList();
		startWatchingFresh(m_shaderPacks.get());

		if (m_mc->traits().contains("texturepacks")) {
			m_texturePacks = m_mc->texturePackList();
			startWatchingFresh(m_texturePacks.get());
		}

		m_worlds = m_mc->worldList();
		m_worlds->startWatching();

		m_gameOptions = m_mc->gameOptionsModel();
		m_gameOptionsFilter = new KeyValueFilterModel(this);
		m_gameOptionsFilter->setSourceModel(m_gameOptions.get());

		m_servers = std::make_unique<ServersListModel>(m_instance->gameRoot());
		m_servers->setLocked(m_instance->isRunning());
		m_servers->startWatching();
	}

	// A legacy instance is not Minecraft-backed but still lists its worlds.
	if (!m_mc) {
		if (auto legacy =
				std::dynamic_pointer_cast<LegacyInstance>(m_instance)) {
			m_worlds = legacy->worldList();
			m_worlds->startWatching();
		}
	}

	m_backups = new BackupController(m_instance, this);

	// The folder may not exist yet; the model lists nothing until the page is reopened.
	m_screenshots = std::make_unique<ScreenshotListModel>();
	m_screenshots->setDirectory(screenshotsDir());

	m_log = new InstanceLogBridge(m_instance, this);

	// Touches only the filesystem, so it is created eagerly. Not every instance has a filter.
	if (auto matcher = m_instance->getLogFileMatcher()) {
		m_otherLogs =
			new OtherLogsModel(m_instance->getLogFileRoot(), matcher, this);
	}

	m_logUploader = new LogUploader(this);
	m_screenshotUploader = new ScreenshotUploader(this);
}

InstanceDetails::~InstanceDetails()
{
	// The models belong to the instance; stop watching them when the page closes.
	if (m_mods) {
		m_mods->stopWatching();
	}
	if (m_resourcePacks) {
		m_resourcePacks->stopWatching();
	}
	if (m_shaderPacks) {
		m_shaderPacks->stopWatching();
	}
	if (m_texturePacks) {
		m_texturePacks->stopWatching();
	}
	if (m_worlds) {
		m_worlds->stopWatching();
	}
	if (m_servers) {
		m_servers->stopWatching();
	}
}

QString InstanceDetails::instanceId() const
{
	return m_instance ? m_instance->id() : QString();
}

QString InstanceDetails::name() const
{
	return m_instance ? m_instance->name() : QString();
}

QString InstanceDetails::gameRoot() const
{
	return m_instance ? m_instance->gameRoot() : QString();
}

QString InstanceDetails::instanceRoot() const
{
	return m_instance ? m_instance->instanceRoot() : QString();
}

QObject* InstanceDetails::settings() const
{
	return m_settings.get();
}

QString InstanceDetails::notes() const
{
	return m_instance ? m_instance->notes() : QString();
}

void InstanceDetails::setNotes(const QString& notes)
{
	// BaseInstance::setNotes() emits nothing; notify here, only on a real change.
	if (!m_instance || m_instance->notes() == notes) {
		return;
	}
	m_instance->setNotes(notes);
	emit notesChanged();
}

QObject* InstanceDetails::mods() const
{
	return m_sortedMods.get();
}

QString InstanceDetails::modsDir() const
{
	return m_mods ? m_mods->dir().absolutePath() : QString();
}

bool InstanceDetails::contentChangesAllowed() const
{
	return m_instance && !m_instance->isRunning();
}

QObject* InstanceDetails::contentBrowser() const
{
	// Lazy: opening the page must not start network activity.
	if (!m_contentBrowser && m_mc) {
		m_contentBrowser =
			std::make_unique<ContentBrowser>(m_mc, const_cast<InstanceDetails*>(this));
	}
	return m_contentBrowser.get();
}

QObject* InstanceDetails::worlds() const
{
	return m_worlds.get();
}

QString InstanceDetails::worldsDir() const
{
	return m_worlds ? m_worlds->dir().absolutePath() : QString();
}

QObject* InstanceDetails::screenshots() const
{
	return m_screenshots.get();
}

QString InstanceDetails::screenshotsDir() const
{
	return FS::PathCombine(m_instance->gameRoot(), "screenshots");
}

void InstanceDetails::deleteWorld(int row)
{
	if (!m_worlds || row < 0 || static_cast<size_t>(row) >= m_worlds->size()) {
		return;
	}
	m_worlds->deleteWorld(row);
}

bool InstanceDetails::renameWorld(int row, const QString& name)
{
	const QString trimmed = name.trimmed();
	if (!m_worlds || trimmed.isEmpty() || row < 0 ||
		static_cast<size_t>(row) >= m_worlds->size()) {
		return false;
	}
	return (*m_worlds)[static_cast<size_t>(row)].rename(trimmed);
}

QObject* InstanceDetails::copyWorld(int row, const QString& name)
{
	const QString trimmed = name.trimmed();
	if (!m_worlds || trimmed.isEmpty() || row < 0 ||
		static_cast<size_t>(row) >= m_worlds->size()) {
		return nullptr;
	}

	// World is a plain value type, safe to copy into a worker thread.
	World world = (*m_worlds)[static_cast<size_t>(row)];
	const QString to = m_worlds->dir().absolutePath();

	auto* task = new WorldCopyTask(
		tr("Copying world…"),
		[world, to, trimmed]() mutable { return world.install(to, trimmed); });

	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Copy world"));
	task->start();
	return watcher;
}

bool InstanceDetails::resetWorldIcon(int row)
{
	if (!m_worlds) {
		return false;
	}
	return m_worlds->resetIcon(row);
}

QObject* InstanceDetails::worldDataPacks() const
{
	if (!m_worldDataPacks && m_mc) {
		m_worldDataPacks = std::make_unique<WorldDataPacksController>(
			m_mc, m_worlds.get(), const_cast<InstanceDetails*>(this));
	}
	return m_worldDataPacks.get();
}

QObject* InstanceDetails::servers() const
{
	return m_servers.get();
}

QString InstanceDetails::serversDir() const
{
	return m_instance ? m_instance->gameRoot() : QString();
}

QObject* InstanceDetails::backups() const
{
	return m_backups;
}

QObject* InstanceDetails::managedPack() const
{
	if (!m_managedPackChecked) {
		m_managedPackChecked = true;
		if (m_instance && ManagedPackController::isSupported(m_instance.get())) {
			m_managedPack = std::make_unique<ManagedPackController>(
				m_instance.get(), const_cast<InstanceDetails*>(this));
		}
	}
	return m_managedPack.get();
}

QObject* InstanceDetails::log() const
{
	return m_log;
}

QObject* InstanceDetails::otherLogs() const
{
	return m_otherLogs;
}

QObject* InstanceDetails::components() const
{
	return m_mc ? m_mc->getPackProfile().get() : nullptr;
}

QObject* InstanceDetails::minecraftVersions() const
{
	// Lazy: nothing downloads until QML reads the property.
	if (!m_minecraftVersions && m_mc) {
		m_minecraftVersions = std::make_unique<MinecraftVersionListProxy>(
			const_cast<InstanceDetails*>(this));
		auto list =
			LAUNCHER->metadataIndex()->get(QStringLiteral("net.minecraft"));
		m_minecraftVersions->setSourceModel(list.get());
	}
	return m_minecraftVersions.get();
}

QObject* InstanceDetails::loaderInstaller() const
{
	if (!m_loaderInstaller && m_mc) {
		m_loaderInstaller = std::make_unique<LoaderInstaller>(
			m_mc->getPackProfile().get(), const_cast<InstanceDetails*>(this));
	}
	return m_loaderInstaller.get();
}

QObject* InstanceDetails::componentTools() const
{
	if (!m_componentTools && m_mc) {
		m_componentTools = std::make_unique<ComponentTools>(
			std::dynamic_pointer_cast<MinecraftInstance>(m_instance),
			const_cast<InstanceDetails*>(this));
	}
	return m_componentTools.get();
}

QObject* InstanceDetails::gameOptions() const
{
	return m_gameOptionsFilter;
}

bool InstanceDetails::isMinecraft() const
{
	return m_mc != nullptr;
}

QObject* InstanceDetails::content() const
{
	if (!m_content && m_mc) {
		m_content = new InstanceContentController(
			m_mc, const_cast<InstanceDetails*>(this));
	}
	return m_content;
}

QObject* InstanceDetails::drop() const
{
	if (!m_drop && m_mc) {
		m_drop = new ContentDrop(m_instance, const_cast<InstanceDetails*>(this));
	}
	return m_drop;
}

QObject* InstanceDetails::logUploader() const
{
	return m_logUploader;
}

QObject* InstanceDetails::screenshotUploader() const
{
	return m_screenshotUploader;
}

QObject* InstanceDetails::installWorld(const QString& fileUrlOrPath)
{
	if (!m_worlds) {
		return nullptr;
	}
	const QString source = FS::pathFromUrlOrPath(fileUrlOrPath);
	if (!WorldTools::isWorld(source)) {
		return nullptr;
	}
	const QString to = m_worlds->dir().absolutePath();

	auto* task = new WorldCopyTask(tr("Adding world…"), [source, to]() {
		return WorldTools::installWorld(source, to);
	});
	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Add world"));
	task->start();
	return watcher;
}

bool InstanceDetails::isLegacy() const
{
	return LegacyUpgrade::isLegacy(m_instance);
}

QString InstanceDetails::kiteModSupport() const
{
	if (!m_mc) {
		return QStringLiteral("unknown");
	}
	switch (m_mc->kiteModSupport()) {
		case KiteMod::Support::Available:
			return QStringLiteral("available");
		case KiteMod::Support::NoBuildForMinecraft:
			return QStringLiteral("noBuild");
		case KiteMod::Support::LoaderTooOld:
			return QStringLiteral("loaderTooOld");
		case KiteMod::Support::Unknown:
			break;
	}
	return QStringLiteral("unknown");
}

QObject* InstanceDetails::upgradeLegacy()
{
	auto* launcher = LauncherContext::instance();
	auto instances = launcher ? launcher->instances() : nullptr;
	if (!instances) {
		return nullptr;
	}
	auto* task = LegacyUpgrade::makeTask(
		m_instance, instances->getInstanceGroup(m_instance->id()));
	if (!task) {
		return nullptr;
	}
	Task* wrapped = instances->wrapInstanceTask(task);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), this);
	watcher->setTitle(LegacyUpgrade::upgradedName(m_instance->name()));
	wrapped->start();
	return watcher;
}

void InstanceDetails::refreshWorlds()
{
	if (m_worlds) {
		m_worlds->update();
	}
}

bool InstanceDetails::mceditConfigured() const
{
	auto* launcher = LauncherContext::instance();
	if (!launcher || !launcher->settings()) {
		return false;
	}
	const QString path =
		launcher->settings()->get(QStringLiteral("MCEditPath")).toString();
	return !WorldTools::mceditProgram(path).isEmpty();
}

QString InstanceDetails::openWorldInMCEdit(int row)
{
	if (!m_worlds || row < 0 || static_cast<size_t>(row) >= m_worlds->size()) {
		return tr("That world is no longer in the list.");
	}
	auto* launcher = LauncherContext::instance();
	const QString path =
		launcher && launcher->settings()
			? launcher->settings()->get(QStringLiteral("MCEditPath")).toString()
			: QString();
	const QString folder =
		m_worlds->dir().absoluteFilePath((*m_worlds)[static_cast<size_t>(row)].folderName());
	return WorldTools::openInMCEdit(path, folder);
}

void InstanceDetails::onRunningStatusChanged(bool running)
{
	emit contentChangesAllowedChanged();
	if (m_servers) {
		m_servers->setLocked(running);
	}
	// The game rewrites options.txt as it quits; show what it saved.
	if (!running && m_gameOptions) {
		m_gameOptions->reload();
	}
}

InstanceLogBridge::InstanceLogBridge(InstancePtr instance, QObject* parent)
	: QObject(parent), m_instance(std::move(instance))
{
	if (!m_instance) {
		return;
	}

	m_task = m_instance->getLaunchTask();
	if (m_task) {
		m_model = m_task->getLogModel();
	}
	connect(m_instance.get(), &BaseInstance::launchTaskChanged, this,
			&InstanceLogBridge::onLaunchTaskChanged);
}

InstanceLogBridge::~InstanceLogBridge() = default;

QObject* InstanceLogBridge::model() const
{
	return m_model.get();
}

bool InstanceLogBridge::hasLog() const
{
	return m_model != nullptr;
}

void InstanceLogBridge::clear()
{
	if (m_model) {
		m_model->clear();
	}
}

QString InstanceLogBridge::text()
{
	return m_model ? m_model->toPlainText() : QString();
}

QVariantList InstanceLogBridge::findLines(const QString& needle) const
{
	QVariantList out;
	for (int row : LogFind::matchingRows(m_model.get(), LogModel::LineRole, needle)) {
		out.append(row);
	}
	return out;
}

void InstanceLogBridge::setSuspended(bool suspended)
{
	if (m_model) {
		m_model->suspend(suspended);
	}
}

void InstanceLogBridge::onLaunchTaskChanged(shared_qobject_ptr<LaunchTask> task)
{
	m_task = task;
	m_model = m_task ? m_task->getLogModel() : shared_qobject_ptr<LogModel>();
	emit modelChanged();
}
