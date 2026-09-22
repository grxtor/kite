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

#include "InstanceDetails.h"

#include <QSortFilterProxyModel>
#include <QUrl>

#include "launch/LaunchTask.h"
#include "launch/LogModel.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/WorldList.h"
#include "minecraft/mod/ModFolderModel.h"
#include "models/SettingsAdapter.h"
#include "screenshots/ScreenshotListModel.h"
#include "FileSystem.h"

InstanceDetails::InstanceDetails(InstancePtr instance, QObject* parent)
	: QObject(parent), m_instance(std::move(instance))
{
	if (!m_instance) {
		return;
	}

	// Coarse, like InstanceList::propertiesChanged: name is not the only
	// thing propertiesChanged can mean, but re-reading it on every one of
	// these is cheap and never misses a rename.
	connect(m_instance.get(), &BaseInstance::propertiesChanged, this,
			[this](BaseInstance*) { emit nameChanged(); });
	connect(m_instance.get(), &BaseInstance::runningStatusChanged, this,
			&InstanceDetails::onRunningStatusChanged);

	m_settings = std::make_unique<SettingsAdapter>(m_instance->settings());

	m_mc = dynamic_cast<MinecraftInstance*>(m_instance.get());
	if (m_mc) {
		m_mods = m_mc->loaderModList();
		/* The folder model lists files in whatever order the directory
		 * gives them; people look for a mod by name. */
		m_sortedMods = std::make_unique<QSortFilterProxyModel>();
		m_sortedMods->setSourceModel(m_mods.get());
		m_sortedMods->setSortRole(m_mods->roleNames().key("name", Qt::DisplayRole));
		m_sortedMods->setSortCaseSensitivity(Qt::CaseInsensitive);
		m_sortedMods->setDynamicSortFilter(true);
		m_sortedMods->sort(0);
		m_worlds = m_mc->worldList();

		// Mirrors ModFolderPage::openedImpl(): startWatching() runs an
		// update() itself whenever it actually starts watching; force one
		// here too in case the model was already watching (shared with
		// another open view of it), so this bridge never shows stale data.
		const bool wasValid = m_mods->isValid() && m_mods->dir().exists();
		m_mods->startWatching();
		if (wasValid) {
			m_mods->update();
		}

		// Mirrors WorldListPage::openedImpl().
		m_worlds->startWatching();
	}

	/* An instance that never took a screenshot has no folder yet; the
	 * model then simply lists nothing until the page is opened again. */
	m_screenshots = std::make_unique<ScreenshotListModel>();
	m_screenshots->setDirectory(screenshotsDir());

	m_log = new InstanceLogBridge(m_instance, this);
}

InstanceDetails::~InstanceDetails()
{
	// Mirrors ModFolderPage::~ModFolderPage() / WorldListPage::~WorldListPage():
	// the models are owned by the instance, not by this bridge, but nothing
	// else stops watching them once this detail page closes.
	if (m_mods) {
		m_mods->stopWatching();
	}
	if (m_worlds) {
		m_worlds->stopWatching();
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
	// BaseInstance::setNotes() does not itself emit anything (unlike most
	// of its other setters) - notify explicitly, and only on a real change.
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

QModelIndex InstanceDetails::sourceModIndex(int row) const
{
	if (!m_sortedMods || row < 0 || row >= m_sortedMods->rowCount()) {
		return {};
	}
	return m_sortedMods->mapToSource(m_sortedMods->index(row, 0));
}

void InstanceDetails::setModEnabled(int row, bool enabled)
{
	const QModelIndex index = sourceModIndex(row);
	if (!index.isValid()) {
		return;
	}
	m_mods->setModStatus({ index },
						 enabled ? ModFolderModel::Enable
								 : ModFolderModel::Disable);
}

void InstanceDetails::deleteMod(int row)
{
	const QModelIndex index = sourceModIndex(row);
	if (!index.isValid()) {
		return;
	}
	m_mods->deleteMods({ index });
}

bool InstanceDetails::installMod(const QString& fileUrlOrPath)
{
	if (!m_mods) {
		return false;
	}
	// Same conversion ModFolderModel's own drop handling uses
	// (dropMimeData(): url.toLocalFile()) - a QML FileDialog hands out
	// file:// URLs, but accept a plain path too.
	const QUrl url(fileUrlOrPath);
	const QString path = url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;
	return m_mods->installMod(path);
}

bool InstanceDetails::contentChangesAllowed() const
{
	// Mirrors ModFolderPage::contentChangesAllowed() specialized to mods:
	// allowsChangesWhileRunning() is always false for ModPlatform::ContentType::Mod,
	// so that page's rule reduces to exactly this.
	return m_instance && !m_instance->isRunning();
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

QObject* InstanceDetails::log() const
{
	return m_log;
}

QObject* InstanceDetails::components() const
{
	return m_mc ? m_mc->getPackProfile().get() : nullptr;
}

bool InstanceDetails::isMinecraft() const
{
	return m_mc != nullptr;
}

void InstanceDetails::onRunningStatusChanged(bool)
{
	emit contentChangesAllowedChanged();
}

// ---------------------------------------------------------------------------

InstanceLogBridge::InstanceLogBridge(InstancePtr instance, QObject* parent)
	: QObject(parent), m_instance(std::move(instance))
{
	if (!m_instance) {
		return;
	}

	// Mirrors LogPage::LogPage(): pick up whatever launch is already in
	// flight, then follow every launch after that.
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
