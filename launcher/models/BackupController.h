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

#include <QObject>
#include <QString>
#include <memory>

#include "BaseInstance.h"
#include "backup/BackupManager.h"

class QAbstractListModel;

/*
 * QML-facing instance backups - the widget-free replacement for BackupPage.
 *
 * Every operation that can take a while (create, restore, export, import,
 * delete - BackupManager itself blocks the calling thread for all five, see
 * its class comment) runs on a worker thread behind a TaskWatcher, the same
 * pattern QmlShell::duplicateInstance()/NewInstanceController::importFrom()
 * use for InstanceCopyTask/InstanceImportTask: this page must not freeze the
 * GUI thread zipping or unzipping a multi-gigabyte instance.
 *
 * Created once per InstanceDetails (InstanceDetails::backups()), for every
 * instance type - a backup is just a zip of the instance folder, nothing
 * about it needs the instance to be Minecraft-backed.
 */
class BackupController : public QObject
{
	Q_OBJECT

	/// QAbstractListModel of the instance's backups, newest first - roles
	/// "name" (label, falling back to the file name), "fileName",
	/// "timestampText" (yyyy-MM-dd HH:mm:ss) and "sizeText" (human size).
	Q_PROPERTY(QObject* model READ model CONSTANT)
	/// Whether the instance is currently running - the same condition
	/// restoreBackup() itself refuses on, exposed so the tab can warn
	/// before the click rather than only after it, mirroring
	/// WorldDataPacksController::unlocked().
	Q_PROPERTY(bool running READ running NOTIFY runningChanged)

  public:
	explicit BackupController(InstancePtr instance, QObject* parent = nullptr);
	~BackupController() override;

	QObject* model() const;
	bool running() const;

	Q_INVOKABLE void refresh();
	/// Starts a new backup; returns a TaskWatcher.
	Q_INVOKABLE QObject* createBackup(const QString& label);
	/// Null if @p row is out of range or the instance is currently
	/// running - restoring would fight a live process for the same files
	/// (same guard BackupPage's own restore button applies). See
	/// `running` above for surfacing that to the user before the click.
	Q_INVOKABLE QObject* restoreBackup(int row);
	Q_INVOKABLE QObject* deleteBackup(int row);
	/// @p destUrlOrPath: a file:// URL (as a QML FileDialog save-mode
	/// picker hands out) or a plain local path.
	Q_INVOKABLE QObject* exportBackup(int row, const QString& destUrlOrPath);
	/// @p fileUrlOrPath: likewise, from an open-mode FileDialog.
	Q_INVOKABLE QObject* importBackup(const QString& fileUrlOrPath,
									 const QString& label);

  signals:
	void runningChanged();

  private:
	InstancePtr m_instance;
	BackupManager m_manager;
	std::unique_ptr<QAbstractListModel> m_model;
	QList<BackupEntry> m_entries;
};
