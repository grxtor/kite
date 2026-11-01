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

#include <QString>
#include <QList>
#include <QDateTime>
#include <functional>

/* Instance backup snapshots (create, restore, export, import, delete) as
 * zip archives in `<instanceRoot>/.backups/`. The timestamp and optional
 * label are in the file name, so a copied or hand-restored directory still
 * lists correctly:
 *   2026-01-15_14-30-00.zip
 *   2026-01-15_14-30-00_pre-launch.zip
 *
 * Everything blocks the calling thread; BackupTask runs createBackup() on a
 * worker thread. */

struct BackupEntry {
	QString name;		  /* human-readable label, may be empty */
	QString fileName;	  /* zip file name */
	QString fullPath;	  /* absolute path to the backup zip */
	QDateTime timestamp;  /* when the backup was created */
	qint64 sizeBytes = 0; /* file size */

	bool isValid() const
	{
		return !fullPath.isEmpty();
	}
};

class BackupManager
{
  public:
	/* Progress of a long backup: status and details are text lines (empty
	 * leaves it alone); current/total are file counts (total 0 = unknown).
	 * Called on the thread createBackup() runs on, so a UI implementation must
	 * marshal (BackupTask does). Throttled to about one call per percent. */
	using ProgressFn =
		std::function<void(const QString& status, const QString& details,
						   qint64 current, qint64 total)>;

	BackupManager(const QString& instanceId, const QString& instanceRoot);

	QString backupDir() const;

	QList<BackupEntry> listBackups() const;

	/* An invalid entry (empty fullPath) on failure. */
	BackupEntry createBackup(const QString& label = {},
							 const ProgressFn& progress = nullptr);

	/* Wipes the instance root (except .backups/) and unpacks `entry` over it.
	 * Destructive; callers must confirm. */
	bool restoreBackup(const BackupEntry& entry);

	bool exportBackup(const BackupEntry& entry, const QString& destPath);
	BackupEntry importBackup(const QString& srcZipPath,
							 const QString& label = {});
	bool deleteBackup(const BackupEntry& entry);

  private:
	bool ensureBackupDir();
	QString generateFileName(const QString& label) const;
	BackupEntry entryFromFile(const QString& filePath) const;

	QString m_instanceId;
	QString m_instanceRoot;
	QString m_backupDir;
};
