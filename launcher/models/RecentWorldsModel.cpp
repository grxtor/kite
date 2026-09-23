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
				// InstanceList::emitIsRunningChanged() is the only place
				// that emits IsRunningRole, and it always names it
				// explicitly (never with an empty role list) for a
				// genuine running-state transition. Every other edit -
				// rename, icon change, setLastLaunch, a managed-pack
				// update, the hasCrashed flag, ... - goes through
				// propertiesChanged() instead, which emits dataChanged
				// with an empty role list; that is not a stop, so it must
				// not trigger a rescan.
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
					// Only a stop is interesting here: that is when a play
					// session could have left a world behind, and rescanning
					// on every start as well would just double the work for
					// no gain.
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
			// Cheap rejection before the (relatively expensive) NBT parse
			// World's constructor would otherwise do for every candidate on
			// every rescan of every instance.
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

	// Stable: several worlds tied on lastPlayed (most commonly several all
	// at 0, i.e. no LastPlayed tag) would otherwise reorder nondeterminis-
	// tically between rescans and flicker in the UI.
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
		// Coalesced the same way the debounce timer coalesces a burst of
		// triggers: one more scan once the current one is done, not a
		// second one queued alongside it.
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
	beginResetModel();
	m_entries = m_watcher.result();
	endResetModel();

	if (m_scanPending) {
		startScan();
	}
}
