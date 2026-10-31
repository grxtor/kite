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

#include "backup/BackupManager.h"
#include "Logging.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QDebug>
#include <memory>

#include "FileSystem.h"
#include "MMCZip.h"

namespace
{
	QString timestampFormat()
	{
		return QStringLiteral("yyyy-MM-dd_HH-mm-ss");
	}

	const QLatin1String kBackupDirName(".backups");

	/* True for paths that must never end up in a backup; relative to the
	 * instance root with '/' separators (as MMCZip::compressDir() gives). */
	bool isExcludedFromBackup(const QString& relPath)
	{
		return relPath == kBackupDirName ||
			   relPath.startsWith(QLatin1String(".backups/"));
	}
} // namespace

BackupManager::BackupManager(const QString& instanceId,
							 const QString& instanceRoot)
	: m_instanceId(instanceId), m_instanceRoot(instanceRoot)
{
	m_backupDir = QDir(m_instanceRoot).filePath(kBackupDirName);
}

QString BackupManager::backupDir() const
{
	return m_backupDir;
}

bool BackupManager::ensureBackupDir()
{
	if (!FS::ensureFolderPathExists(m_backupDir)) {
		qCWarning(backupLog) << "Could not create backup directory"
				   << m_backupDir;
		return false;
	}
	return true;
}

QList<BackupEntry> BackupManager::listBackups() const
{
	QList<BackupEntry> result;

	QDir dir(m_backupDir);
	if (!dir.exists())
		return result;

	const auto files =
		dir.entryInfoList({QStringLiteral("*.zip")}, QDir::Files, QDir::Time);
	for (const auto& fi : files) {
		result.append(entryFromFile(fi.absoluteFilePath()));
	}

	return result;
}

BackupEntry BackupManager::createBackup(const QString& label,
										const ProgressFn& progress)
{
	if (!ensureBackupDir())
		return {};

	const QString fileName = generateFileName(label);
	const QString zipPath = QDir(m_backupDir).filePath(fileName);

	qCDebug(backupLog) << "Creating backup of instance" << m_instanceId << "->"
			 << zipPath;

	MMCZip::ProgressFunction zipProgress;
	if (progress) {
		progress(QObject::tr("Compressing backup..."), QString(), 0, 0);

		/* compressDir() calls back per file; only speak up when the percentage
		 * moves. */
		auto lastReported = std::make_shared<qint64>(-1);
		zipProgress = [progress, lastReported](qint64 current, qint64 total) {
			const qint64 step = qMax(qint64(1), total / 100);
			if (current != total && current / step == *lastReported)
				return;
			*lastReported = current / step;
			progress(QString(),
					 QObject::tr("%1 / %2 files").arg(current).arg(total),
					 current, total);
		};
	}

	if (!MMCZip::compressDir(zipPath, m_instanceRoot, &isExcludedFromBackup,
							 zipProgress)) {
		qCWarning(backupLog) << "Failed to create backup:" << zipPath;
		// A half-written zip would list as a restorable snapshot.
		QFile::remove(zipPath);
		return {};
	}

	qCDebug(backupLog) << "Backup created:" << zipPath;
	return entryFromFile(zipPath);
}

bool BackupManager::restoreBackup(const BackupEntry& entry)
{
	if (!QFile::exists(entry.fullPath)) {
		qCWarning(backupLog) << "Backup file not found:" << entry.fullPath;
		return false;
	}

	qCDebug(backupLog) << "Restoring" << entry.fullPath << "into"
			 << m_instanceRoot;

	// Clear the instance root but keep the archive directory, or restoring
	// would delete every snapshot, including the one being restored.
	QDir instDir(m_instanceRoot);
	const auto entries = instDir.entryInfoList(
		QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
	for (const auto& fi : entries) {
		if (fi.fileName() == kBackupDirName)
			continue;
		if (!FS::deletePath(fi.absoluteFilePath())) {
			qCWarning(backupLog) << "Could not remove" << fi.absoluteFilePath()
					   << "- aborting restore to avoid a half-wiped instance";
			return false;
		}
	}

	if (!MMCZip::extractDir(entry.fullPath, m_instanceRoot).has_value()) {
		qCWarning(backupLog) << "Failed to extract backup:" << entry.fullPath;
		return false;
	}

	qCDebug(backupLog) << "Restore complete.";
	return true;
}

bool BackupManager::exportBackup(const BackupEntry& entry,
								 const QString& destPath)
{
	if (!QFile::exists(entry.fullPath))
		return false;

	if (!FS::ensureFilePathExists(destPath))
		return false;

	return QFile::copy(entry.fullPath, destPath);
}

BackupEntry BackupManager::importBackup(const QString& srcZipPath,
										const QString& label)
{
	if (!QFile::exists(srcZipPath))
		return {};

	if (!ensureBackupDir())
		return {};

	const QString destPath =
		QDir(m_backupDir).filePath(generateFileName(label));

	if (!QFile::copy(srcZipPath, destPath)) {
		qCWarning(backupLog) << "Could not copy" << srcZipPath << "to"
				   << destPath;
		return {};
	}

	qCDebug(backupLog) << "Imported" << srcZipPath << "as" << destPath;
	return entryFromFile(destPath);
}

bool BackupManager::deleteBackup(const BackupEntry& entry)
{
	return QFile::remove(entry.fullPath);
}

QString BackupManager::generateFileName(const QString& label) const
{
	const QString ts =
		QDateTime::currentDateTime().toString(timestampFormat());
	if (label.isEmpty())
		return ts + QStringLiteral(".zip");

	// Labels end up in a file name; the same substitution runs in
	// entryFromFile().
	static const QRegularExpression unsafeChars(
		QStringLiteral("[^a-zA-Z0-9_-]"));
	QString safe = label;
	safe.replace(unsafeChars, QStringLiteral("_"));
	safe.truncate(64);
	return ts + QLatin1Char('_') + safe + QStringLiteral(".zip");
}

BackupEntry BackupManager::entryFromFile(const QString& filePath) const
{
	const QFileInfo fi(filePath);

	BackupEntry entry;
	entry.fileName = fi.fileName();
	entry.fullPath = fi.absoluteFilePath();
	entry.sizeBytes = fi.size();

	// The generated name is authoritative for when a backup was taken: birth
	// time is not on every filesystem, and file copies (export/import)
	// rewrite birth and modification time.
	const QString base = fi.completeBaseName();
	static const QRegularExpression nameRe(QStringLiteral(
		"^(\\d{4}-\\d{2}-\\d{2}_\\d{2}-\\d{2}-\\d{2})(?:_(.+))?$"));
	const auto match = nameRe.match(base);
	if (match.hasMatch()) {
		entry.timestamp =
			QDateTime::fromString(match.captured(1), timestampFormat());
		entry.name =
			match.captured(2).replace(QLatin1Char('_'), QLatin1Char(' '));
	}

	if (!entry.timestamp.isValid()) {
		entry.timestamp =
			fi.birthTime().isValid() ? fi.birthTime() : fi.lastModified();
	}
	if (entry.name.isEmpty() && !match.hasMatch()) {
		entry.name = base;
	}

	return entry;
}
