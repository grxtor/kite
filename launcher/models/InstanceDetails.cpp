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
#include "models/ContentBrowser.h"
#include "models/SettingsAdapter.h"
#include "screenshots/ScreenshotListModel.h"
#include "FileSystem.h"

namespace
{
	/* Wraps @p source in the same by-name sorted proxy the mods list has
	 * always used: the folder model lists files in whatever order the
	 * directory gives them, but people look for content by name. */
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

	/* Mirrors ModFolderPage::openedImpl(): startWatching() runs an
	 * update() itself the first time it actually starts watching, but a
	 * subsequent call (this bridge replacing a previous one, or a widget
	 * page already showing the same folder) only re-arms the
	 * QFileSystemWatcher. Force an update so @p model never hands QML
	 * stale data. */
	void startWatchingFresh(ModFolderModel* model)
	{
		const bool wasValid = model->isValid() && model->dir().exists();
		model->startWatching();
		if (wasValid) {
			model->update();
		}
	}
} // namespace

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
		m_sortedMods = sortedByName(m_mods.get());
		startWatchingFresh(m_mods.get());

		m_resourcePacks = m_mc->resourcePackList();
		m_sortedResourcePacks = sortedByName(m_resourcePacks.get());
		startWatchingFresh(m_resourcePacks.get());

		m_shaderPacks = m_mc->shaderPackList();
		m_sortedShaderPacks = sortedByName(m_shaderPacks.get());
		startWatchingFresh(m_shaderPacks.get());

		// Legacy-only: see the texturePacks Q_PROPERTY comment.
		if (m_mc->traits().contains("texturepacks")) {
			m_texturePacks = m_mc->texturePackList();
			m_sortedTexturePacks = sortedByName(m_texturePacks.get());
			startWatchingFresh(m_texturePacks.get());
		}

		m_worlds = m_mc->worldList();
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

void InstanceDetails::setModEnabled(int row, bool enabled)
{
	setEnabled(QStringLiteral("mods"), row, enabled);
}

void InstanceDetails::deleteMod(int row)
{
	remove(QStringLiteral("mods"), row);
}

bool InstanceDetails::installMod(const QString& fileUrlOrPath)
{
	return install(QStringLiteral("mods"), fileUrlOrPath);
}

QObject* InstanceDetails::resourcePacks() const
{
	return m_sortedResourcePacks.get();
}

QString InstanceDetails::resourcePacksDir() const
{
	return m_resourcePacks ? m_resourcePacks->dir().absolutePath() : QString();
}

QObject* InstanceDetails::shaderPacks() const
{
	return m_sortedShaderPacks.get();
}

QString InstanceDetails::shaderPacksDir() const
{
	return m_shaderPacks ? m_shaderPacks->dir().absolutePath() : QString();
}

QObject* InstanceDetails::texturePacks() const
{
	return m_sortedTexturePacks.get();
}

QString InstanceDetails::texturePacksDir() const
{
	return m_texturePacks ? m_texturePacks->dir().absolutePath() : QString();
}

ModFolderModel* InstanceDetails::folderModel(const QString& kind) const
{
	if (kind == QStringLiteral("mods")) {
		return m_mods.get();
	}
	if (kind == QStringLiteral("resourcepacks")) {
		return m_resourcePacks.get();
	}
	if (kind == QStringLiteral("shaderpacks")) {
		return m_shaderPacks.get();
	}
	if (kind == QStringLiteral("texturepacks")) {
		return m_texturePacks.get();
	}
	return nullptr;
}

QSortFilterProxyModel* InstanceDetails::sortedFolderModel(const QString& kind) const
{
	if (kind == QStringLiteral("mods")) {
		return m_sortedMods.get();
	}
	if (kind == QStringLiteral("resourcepacks")) {
		return m_sortedResourcePacks.get();
	}
	if (kind == QStringLiteral("shaderpacks")) {
		return m_sortedShaderPacks.get();
	}
	if (kind == QStringLiteral("texturepacks")) {
		return m_sortedTexturePacks.get();
	}
	return nullptr;
}

QModelIndex InstanceDetails::sourceIndexFor(const QString& kind, int row) const
{
	auto* sorted = sortedFolderModel(kind);
	if (!sorted || row < 0 || row >= sorted->rowCount()) {
		return {};
	}
	return sorted->mapToSource(sorted->index(row, 0));
}

void InstanceDetails::setEnabled(const QString& kind, int row, bool enabled)
{
	auto* model = folderModel(kind);
	const QModelIndex index = sourceIndexFor(kind, row);
	if (!model || !index.isValid()) {
		return;
	}
	model->setModStatus({ index },
						enabled ? ModFolderModel::Enable
								: ModFolderModel::Disable);
}

void InstanceDetails::remove(const QString& kind, int row)
{
	auto* model = folderModel(kind);
	const QModelIndex index = sourceIndexFor(kind, row);
	if (!model || !index.isValid()) {
		return;
	}
	model->deleteMods({ index });
}

bool InstanceDetails::install(const QString& kind, const QString& fileUrlOrPath)
{
	auto* model = folderModel(kind);
	if (!model) {
		return false;
	}
	// Same conversion ModFolderModel's own drop handling uses
	// (dropMimeData(): url.toLocalFile()) - a QML FileDialog hands out
	// file:// URLs, but accept a plain path too.
	const QUrl url(fileUrlOrPath);
	const QString path = url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;
	return model->installMod(path);
}

bool InstanceDetails::contentChangesAllowed() const
{
	// Mirrors ModFolderPage::contentChangesAllowed() specialized to mods:
	// allowsChangesWhileRunning() is always false for ModPlatform::ContentType::Mod,
	// so that page's rule reduces to exactly this.
	return m_instance && !m_instance->isRunning();
}

QObject* InstanceDetails::contentBrowser() const
{
	// Lazy on purpose: instantiated the first time anything reads this
	// property, not in the constructor above - opening this page must not
	// start the network activity a search or an install would. Null for a
	// non-Minecraft instance, matching mods()/components() etc. above.
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
