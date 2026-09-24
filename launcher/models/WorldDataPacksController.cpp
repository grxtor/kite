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

#include "WorldDataPacksController.h"

#include <QDir>
#include <QUrl>

#include "BaseInstance.h"
#include "FileSystem.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/WorldList.h"
#include "minecraft/mod/DataPackFolderModel.h"

WorldDataPacksController::WorldDataPacksController(MinecraftInstance* instance,
													WorldList* worlds,
													QObject* parent)
	: QObject(parent), m_instance(instance), m_worlds(worlds)
{
	if (m_instance) {
		connect(m_instance, &BaseInstance::runningStatusChanged, this,
				&WorldDataPacksController::onRunningStatusChanged);
	}
}

WorldDataPacksController::~WorldDataPacksController()
{
	if (m_model) {
		m_model->stopWatching();
	}
}

QObject* WorldDataPacksController::model() const
{
	return m_sorted.get();
}

QString WorldDataPacksController::directory() const
{
	return m_model ? m_model->dir().absolutePath() : QString();
}

bool WorldDataPacksController::unlocked() const
{
	return m_instance && !m_instance->isRunning();
}

void WorldDataPacksController::onRunningStatusChanged()
{
	emit unlockedChanged();
}

void WorldDataPacksController::openForWorld(int row)
{
	if (!m_worlds || row < 0 ||
		static_cast<size_t>(row) >= m_worlds->size()) {
		return;
	}

	auto& world = (*m_worlds)[static_cast<size_t>(row)];
	const QString worldDir = m_worlds->dir().absoluteFilePath(world.folderName());
	const QString folder = FS::PathCombine(worldDir, "datapacks");
	const QString name =
		world.name().isEmpty() ? world.folderName() : world.name();

	if (m_model && QDir(m_model->dir().absolutePath()) == QDir(folder)) {
		// Already showing this world - the name may still have changed.
		m_worldName = name;
		emit modelChanged();
		return;
	}

	if (m_model) {
		m_model->stopWatching();
	}

	m_worldName = name;
	m_model = std::make_unique<DataPackFolderModel>(folder);
	m_sorted = std::make_unique<QSortFilterProxyModel>();
	m_sorted->setSourceModel(m_model.get());
	m_sorted->setSortRole(m_model->roleNames().key("name", Qt::DisplayRole));
	m_sorted->setSortCaseSensitivity(Qt::CaseInsensitive);
	m_sorted->setDynamicSortFilter(true);
	m_sorted->sort(0);

	// Mirrors InstanceDetails.cpp's startWatchingFresh(): a folder that
	// already exists (this world had data packs before) needs an explicit
	// update() too, since startWatching() only runs one on its very first
	// call ever for a given QFileSystemWatcher path.
	const bool wasValid = m_model->isValid() && m_model->dir().exists();
	m_model->startWatching();
	if (wasValid) {
		m_model->update();
	}

	emit modelChanged();
}

void WorldDataPacksController::setEnabled(int row, bool enabled)
{
	if (!m_model || !m_sorted || row < 0 || row >= m_sorted->rowCount()) {
		return;
	}
	const QModelIndex source =
		m_sorted->mapToSource(m_sorted->index(row, 0));
	if (!source.isValid()) {
		return;
	}
	m_model->setModStatus({ source }, enabled ? ModFolderModel::Enable
											  : ModFolderModel::Disable);
}

void WorldDataPacksController::remove(int row)
{
	if (!m_model || !m_sorted || row < 0 || row >= m_sorted->rowCount()) {
		return;
	}
	const QModelIndex source =
		m_sorted->mapToSource(m_sorted->index(row, 0));
	if (!source.isValid()) {
		return;
	}
	m_model->deleteMods({ source });
}

bool WorldDataPacksController::install(const QString& fileUrlOrPath)
{
	if (!m_model) {
		return false;
	}
	const QUrl url(fileUrlOrPath);
	const QString path = url.isLocalFile() ? url.toLocalFile() : fileUrlOrPath;
	return m_model->installMod(path);
}
