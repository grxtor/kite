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

#include "models/RecentWorldsModel.h"

#include <QDir>
#include <QFileInfo>
#include <QThreadPool>
#include <QtConcurrentRun>
#include <QUrl>
#include <algorithm>

#include "BaseInstance.h"
#include "InstanceList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/World.h"

RecentWorldsModel::RecentWorldsModel(InstanceList* instances, QObject* parent)
	: QAbstractListModel(parent), m_instances(instances)
{
	m_rescanTimer.setSingleShot(true);
	connect(&m_rescanTimer, &QTimer::timeout, this,
			&RecentWorldsModel::startScan);
	connect(&m_watcher, &QFutureWatcher<QList<Entry>>::finished, this,
			&RecentWorldsModel::onScanFinished);

	if (m_instances) {
		connect(m_instances, &QAbstractItemModel::rowsInserted, this,
				&RecentWorldsModel::scheduleRescan);
		connect(m_instances, &QAbstractItemModel::rowsRemoved, this,
				&RecentWorldsModel::scheduleRescan);
		connect(
			m_instances, &QAbstractItemModel::dataChanged, this,
			[this](const QModelIndex& topLeft, const QModelIndex& bottomRight,
				   const QList<int>& roles) {
				// Only InstanceList::emitIsRunningChanged() emits IsRunningRole, always named
				// explicitly; every other edit emits dataChanged with an empty role list and
				// must not trigger a rescan.
				if (!roles.contains(InstanceList::IsRunningRole)) {
					return;
				}
				for (int row = topLeft.row(); row <= bottomRight.row();
					 ++row) {
					const bool running =
						m_instances
							->data(m_instances->index(row),
								   InstanceList::IsRunningRole)
							.toBool();
					// Only a stop matters: that is when a session could have left a world behind.
					if (!running) {
						scheduleRescan();
						return;
					}
				}
			});
	}

	scheduleRescan();
}

RecentWorldsModel::~RecentWorldsModel() = default;

int RecentWorldsModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid()) {
		return 0;
	}
	return m_entries.size();
}

QVariant RecentWorldsModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 ||
		index.row() >= m_entries.size()) {
		return QVariant();
	}
	const Entry& entry = m_entries.at(index.row());
	switch (role) {
		case WorldNameRole:
			return entry.worldName;
		case FolderNameRole:
			return entry.folderName;
		case IconUrlRole:
			return entry.iconUrl;
		case LastPlayedRole:
			return entry.lastPlayed;
		case InstanceIdRole:
			return entry.instanceId;
		case InstanceNameRole:
			return entry.instanceName;
		case InstanceIconKeyRole:
			return entry.instanceIconKey;
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> RecentWorldsModel::roleNames() const
{
	return {
		{WorldNameRole, "worldName"},
		{FolderNameRole, "folderName"},
		{IconUrlRole, "iconUrl"},
		{LastPlayedRole, "lastPlayed"},
		{InstanceIdRole, "instanceId"},
		{InstanceNameRole, "instanceName"},
		{InstanceIconKeyRole, "instanceIconKey"},
	};
}

QList<RecentWorldsModel::Entry>
RecentWorldsModel::scan(const QList<InstanceWorldSource>& sources,
						 int maxCount)
{
	QList<Entry> all;
	for (const InstanceWorldSource& source : sources) {
		QDir dir(source.worldsDir);
		if (source.worldsDir.isEmpty() || !dir.exists()) {
			continue;
		}
		const QFileInfoList entries =
			dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
		for (const QFileInfo& entry : entries) {
			// Cheap rejection before the NBT parse World's constructor would do for every candidate.
			if (!QFileInfo(entry.absoluteFilePath() + "/level.dat")
					 .exists()) {
				continue;
			}
			World world(entry);
			if (!world.isValid()) {
				continue;
			}
			Entry row;
			row.worldName = world.name();
			row.folderName = world.folderName();
			row.iconUrl = world.iconFile().isEmpty()
							  ? QString()
							  : QUrl::fromLocalFile(world.iconFile())
									.toString();
			row.lastPlayed = world.lastPlayed().isValid()
								  ? world.lastPlayed().toMSecsSinceEpoch()
								  : 0;
			row.instanceId = source.instanceId;
			row.instanceName = source.instanceName;
			row.instanceIconKey = source.instanceIconKey;
			all.append(row);
		}
	}

	// Stable: worlds tied on lastPlayed (often all 0) must not reorder between rescans.
	std::stable_sort(all.begin(), all.end(), [](const Entry& a, const Entry& b) {
		return a.lastPlayed > b.lastPlayed;
	});
	if (all.size() > maxCount) {
		all.resize(maxCount);
	}
	return all;
}

void RecentWorldsModel::refresh()
{
	m_rescanTimer.stop();
	startScan();
}

void RecentWorldsModel::scheduleRescan()
{
	m_rescanTimer.start(kRescanDebounceMs);
}

void RecentWorldsModel::startScan()
{
	if (m_watcher.isRunning()) {
		// Coalesced like the debounce timer: one more scan once this one is done.
		m_scanPending = true;
		return;
	}
	m_scanPending = false;

	QList<InstanceWorldSource> sources;
	if (m_instances) {
		const int count = m_instances->count();
		sources.reserve(count);
		for (int i = 0; i < count; ++i) {
			InstancePtr inst = m_instances->at(i);
			auto* mcInst = dynamic_cast<MinecraftInstance*>(inst.get());
			if (!mcInst) {
				continue;
			}
			InstanceWorldSource source;
			source.instanceId = mcInst->id();
			source.instanceName = mcInst->name();
			source.instanceIconKey = mcInst->iconKey();
			source.worldsDir = mcInst->worldDir();
			sources.append(source);
		}
	}

	m_watcher.setFuture(QtConcurrent::run(QThreadPool::globalInstance(),
										  &RecentWorldsModel::scan, sources,
										  kMaxWorlds));
}

void RecentWorldsModel::onScanFinished()
{
	const QList<Entry> entries = m_watcher.result();
	const auto same = [](const Entry& a, const Entry& b) {
		return a.worldName == b.worldName && a.folderName == b.folderName &&
			   a.iconUrl == b.iconUrl && a.lastPlayed == b.lastPlayed &&
			   a.instanceId == b.instanceId &&
			   a.instanceName == b.instanceName &&
			   a.instanceIconKey == b.instanceIconKey;
	};
	if (entries.size() != m_entries.size() ||
		!std::equal(entries.begin(), entries.end(), m_entries.begin(), same)) {
		beginResetModel();
		m_entries = entries;
		endResetModel();
	}

	emit scanFinished();

	if (m_scanPending) {
		startScan();
	}
}
