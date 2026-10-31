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

#include <QObject>
#include <QString>
#include <memory>

#include "BaseInstance.h"
#include "backup/BackupManager.h"

class QAbstractListModel;

/*
 * QML-facing instance backups. Create, restore, export, import and delete
 * (BackupManager blocks the calling thread for all five) run on a worker thread
 * behind a TaskWatcher, so the GUI never freezes zipping a multi-gigabyte
 * instance. Created once per InstanceDetails, for every instance type.
 */
class BackupController : public QObject
{
	Q_OBJECT

	/// The instance's backups, newest first: roles "name" (label, else file name),
	/// "fileName", "timestampText", "sizeText".
	Q_PROPERTY(QObject* model READ model CONSTANT)
	/// The condition restoreBackup() refuses on, exposed so the tab can warn before the click.
	Q_PROPERTY(bool running READ running NOTIFY runningChanged)

  public:
	explicit BackupController(InstancePtr instance, QObject* parent = nullptr);
	~BackupController() override;

	QObject* model() const;
	bool running() const;

	/// Whether a create, restore, delete, export or import of this instance is
	/// still running, even one started from a page that has since been left.
	Q_INVOKABLE bool jobRunning() const;
	static bool hasRunningJob(const QString& instanceId);
	/// Restoring wipes the instance: nothing may start it meanwhile.
	static bool isRestoring(const QString& instanceId);

	Q_INVOKABLE void refresh();
	Q_INVOKABLE QObject* createBackup(const QString& label);
	/// Null if @p row is out of range, the instance is running (restoring would
	/// fight a live process for the files) or another job is running.
	Q_INVOKABLE QObject* restoreBackup(int row);
	Q_INVOKABLE QObject* deleteBackup(int row);
	/// @p destUrlOrPath: a file:// URL or a local path.
	Q_INVOKABLE QObject* exportBackup(int row, const QString& destUrlOrPath);
	/// @p fileUrlOrPath: likewise.
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
