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

#pragma once

#include <QAbstractListModel>
#include <QFutureWatcher>
#include <QList>
#include <QString>
#include <QTimer>

class InstanceList;

/*
 * The Home page's "Recent worlds" row: the most recently played worlds
 * across every Minecraft instance, newest first (see the approved Home page
 * spec - "Jump back in" shows recent instances, this shows recent worlds
 * underneath). Roles: worldName, folderName, iconUrl (file:// URL of the
 * world's icon.png, or empty), lastPlayed (ms since epoch, 0 if unknown),
 * instanceId, instanceName, instanceIconKey.
 *
 * SCANNING. Reading every instance's saves folder - a directory listing
 * plus a level.dat parse per world - is not free, and the spec requires
 * zero impact on startup. scan() below does that work; it is a static, pure
 * function (only plain value types in and out, no QObject, no `this`) so it
 * can run on a QThreadPool worker thread via QtConcurrent::run() without
 * touching anything the GUI thread might be using concurrently, and so it
 * can be unit-tested directly against a temporary directory without a real
 * InstanceList or MinecraftInstance. startScan() gathers the per-instance
 * inputs (id/name/iconKey/worldsDir - all plain QStrings, copied by value)
 * on the GUI thread, which is cheap, then hands them to the worker; the
 * result comes back through a QFutureWatcher and is applied with a model
 * reset on the GUI thread. Destroying the model while a scan is in flight
 * is safe without any extra guard: m_watcher is a member, so it is
 * destroyed synchronously (on the GUI thread) before the model itself is
 * gone, and once it is destroyed its finished() signal can no longer fire -
 * the worker keeps running to completion (it holds no reference back to
 * this object) but its result is simply never delivered, the same
 * destruction pattern InstanceCopyTask/ExtractZipTask already rely on for
 * their own QFutureWatcher members.
 *
 * RESCAN TRIGGERS. Construction, an instance being added or removed
 * (InstanceList's rowsInserted/rowsRemoved - loadList() emits genuine Qt
 * model signals for these, unlike instancesChanged() which fires before the
 * reload happens), and an instance's IsRunningRole flipping to false (it
 * may have played a world during that session). All three go through
 * scheduleRescan(), which (re)starts a short single-shot timer, so a burst
 * of them - e.g. several instances loading at once - collapses into one
 * scan, the same coalescing ScreenshotListModel's m_refreshTimer does for
 * filesystem-watcher events.
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

	/* One instance's worlds folder plus the bit of instance metadata a
	 * "Recent worlds" row needs about where it came from. Plain QStrings
	 * only - see the class comment's SCANNING section for why. */
	struct InstanceWorldSource {
		QString instanceId;
		QString instanceName;
		QString instanceIconKey;
		QString worldsDir;
	};

	/// One row of the model - see the class comment for what each field is.
	struct Entry {
		QString worldName;
		QString folderName;
		QString iconUrl;
		qint64 lastPlayed = 0;
		QString instanceId;
		QString instanceName;
		QString instanceIconKey;
	};

	/* @p instances may be null (an empty model, never rescanned) for a
	 * caller with nothing to show yet; production code passes
	 * LAUNCHER->instances().get(), the way QmlShell's other models are
	 * built against it. Not owned - InstanceList outlives this the same
	 * way it outlives InstanceFilterModel. */
	explicit RecentWorldsModel(InstanceList* instances,
							   QObject* parent = nullptr);
	~RecentWorldsModel() override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	/* The @p maxCount most recently played worlds across every source,
	 * newest first. Pure and static - see the class comment's SCANNING
	 * section. Skips anything that is not a directory or has no level.dat,
	 * the same shortcut World::isValid() gives WorldList::update(). */
	static QList<Entry> scan(const QList<InstanceWorldSource>& sources,
							 int maxCount);

	/// Re-scans right away, bypassing the debounce timer - for a caller
	/// that already knows this is a good moment to look (a page opening).
	Q_INVOKABLE void refresh();

  private:
	void scheduleRescan();
	void startScan();
	void onScanFinished();

	InstanceList* m_instances;
	QList<Entry> m_entries;
	QTimer m_rescanTimer;
	QFutureWatcher<QList<Entry>> m_watcher;
	/// A rescan was asked for while one was already running; picked up by
	/// onScanFinished() instead of being dropped on the floor.
	bool m_scanPending = false;

	static constexpr int kMaxWorlds = 8;
	static constexpr int kRescanDebounceMs = 250;
};
