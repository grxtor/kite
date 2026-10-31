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

#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QList>
#include <QString>
#include <QTimer>

class InstanceList;

/*
 * The Home page's "Recent worlds" row: the most recently played worlds across
 * every Minecraft instance, newest first. Roles: worldName, folderName, iconUrl
 * (file:// URL of icon.png, or empty), lastPlayed (ms since epoch, 0 if
 * unknown), instanceId, instanceName, instanceIconKey.
 *
 * Scanning reads every instance's saves folder (a listing plus a level.dat
 * parse per world), so scan() is a static pure function over plain values and
 * runs on a QThreadPool worker via QtConcurrent::run(); startScan() copies the
 * per-instance inputs on the GUI thread first. The result returns through a
 * QFutureWatcher and is applied as a model reset on the GUI thread. Destroying
 * the model mid-scan is safe: m_watcher is a member, so its finished() can no
 * longer fire and the worker's result is simply never delivered.
 *
 * Rescans are triggered by construction, an instance being added or removed
 * (rowsInserted/rowsRemoved) and an instance's IsRunningRole flipping to false.
 * All go through scheduleRescan(), a short single-shot timer, so a burst
 * collapses into one scan.
 */
class RecentWorldsModel : public QAbstractListModel
{
	Q_OBJECT

  public:
	enum Roles {
		WorldNameRole = Qt::UserRole + 1,
		FolderNameRole,
		IconUrlRole,
		LastPlayedRole,
		InstanceIdRole,
		InstanceNameRole,
		InstanceIconKeyRole,
	};

	/* One instance's worlds folder plus the metadata a row needs; plain QStrings
	 * so scan() can run off-thread. */
	struct InstanceWorldSource {
		QString instanceId;
		QString instanceName;
		QString instanceIconKey;
		QString worldsDir;
	};

	struct Entry {
		QString worldName;
		QString folderName;
		QString iconUrl;
		qint64 lastPlayed = 0;
		QString instanceId;
		QString instanceName;
		QString instanceIconKey;
	};

	/* @p instances may be null (an empty model, never rescanned). Not owned;
	 * InstanceList outlives this. */
	explicit RecentWorldsModel(InstanceList* instances,
							   QObject* parent = nullptr);
	~RecentWorldsModel() override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	/* The @p maxCount most recently played worlds across every source, newest
	 * first. Skips anything that is not a directory or has no level.dat. */
	static QList<Entry> scan(const QList<InstanceWorldSource>& sources,
							 int maxCount);

	/// Re-scans right away, bypassing the debounce timer.
	Q_INVOKABLE void refresh();

  private:
	void scheduleRescan();
	void startScan();
	void onScanFinished();

	InstanceList* m_instances;
	QList<Entry> m_entries;
	QTimer m_rescanTimer;
	QFutureWatcher<QList<Entry>> m_watcher;
	/// A rescan was asked for while one was running; onScanFinished() picks it up.
	bool m_scanPending = false;

	static constexpr int kMaxWorlds = 8;
	static constexpr int kRescanDebounceMs = 250;
};
