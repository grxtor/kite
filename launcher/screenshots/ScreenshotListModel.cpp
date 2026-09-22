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

#include "ScreenshotListModel.h"

#include <QDir>
#include <QFileInfo>
#include <QUrl>
#include <algorithm>

#include <FileSystem.h>

namespace
{
bool caseInsensitiveContains(const QStringList& list, const QString& value)
{
	for (const QString& entry : list) {
		if (entry.compare(value, Qt::CaseInsensitive) == 0) {
			return true;
		}
	}
	return false;
}
} // namespace

ScreenshotListModel::ScreenshotListModel(QObject* parent)
	: QAbstractListModel(parent), m_watcher(new QFileSystemWatcher(this))
{
	m_refreshTimer.setSingleShot(true);
	connect(&m_refreshTimer, &QTimer::timeout, this,
			&ScreenshotListModel::refreshNow);
	connect(m_watcher, &QFileSystemWatcher::directoryChanged, this,
			&ScreenshotListModel::onWatcherDirectoryChanged);
}

ScreenshotListModel::~ScreenshotListModel() {}

int ScreenshotListModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid()) {
		return 0;
	}
	return m_entries.size();
}

QVariant ScreenshotListModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 ||
		index.row() >= m_entries.size()) {
		return QVariant();
	}

	const Entry& entry = m_entries.at(index.row());
	switch (role) {
		case Qt::DisplayRole:
		case NameRole:
			return entry.name;
		case PathRole:
			return entry.path;
		case UrlRole:
			return QUrl::fromLocalFile(entry.path).toString();
		case ModifiedRole:
			return entry.modifiedMs;
		case SizeRole:
			return entry.size;
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> ScreenshotListModel::roleNames() const
{
	return {
		{NameRole, "name"},
		{PathRole, "path"},
		{UrlRole, "url"},
		{ModifiedRole, "modified"},
		{SizeRole, "size"},
	};
}

void ScreenshotListModel::setDirectory(const QString& directory)
{
	const QString cleaned =
		directory.isEmpty() ? QString() : QDir(directory).absolutePath();
	if (cleaned == m_directory) {
		return;
	}

	if (!m_watcher->directories().isEmpty()) {
		m_watcher->removePaths(m_watcher->directories());
	}
	m_directory = cleaned;
	if (!m_directory.isEmpty() && QDir(m_directory).exists()) {
		m_watcher->addPath(m_directory);
	}
	emit directoryChanged();

	// A directory switch is a direct, user-visible request (opening a
	// different instance's screenshots) -- refresh right away rather than
	// through the debounce timer, which exists only to coalesce bursts of
	// filesystem events.
	m_refreshTimer.stop();
	refreshNow();
}

bool ScreenshotListModel::remove(int row)
{
	if (row < 0 || row >= m_entries.size()) {
		return false;
	}

	const QString path = m_entries.at(row).path;

	QString pathInTrash;
	if (FS::trash(path, &pathInTrash)) {
		qDebug() << "Screenshot" << path << "moved to trash at"
				 << pathInTrash;
	} else if (FS::deletePath(path)) {
		qDebug() << "Screenshot" << path
				 << "deleted outright (no trash available)";
	} else {
		qWarning() << "Failed to remove screenshot" << path;
		return false;
	}

	beginRemoveRows(QModelIndex(), row, row);
	m_entries.removeAt(row);
	endRemoveRows();
	emit countChanged();
	return true;
}

QString ScreenshotListModel::pathAt(int row) const
{
	if (row < 0 || row >= m_entries.size()) {
		return QString();
	}
	return m_entries.at(row).path;
}

void ScreenshotListModel::onWatcherDirectoryChanged(const QString& path)
{
	Q_UNUSED(path);
	// Restarting an already-running single-shot timer pushes its
	// deadline out, so a burst of signals in under kRefreshDebounceMs
	// collapses into the one refresh that follows the last of them.
	m_refreshTimer.start(kRefreshDebounceMs);
}

void ScreenshotListModel::refreshNow()
{
	QList<Entry> fresh = listEntries(m_directory);

	// The watcher silently drops a path that stops existing and never
	// re-adds it on its own -- re-arm it here so a directory that
	// reappears (or appears for the first time) between refreshes is
	// picked up by the next one.
	if (!m_directory.isEmpty() && QDir(m_directory).exists() &&
		!m_watcher->directories().contains(m_directory)) {
		m_watcher->addPath(m_directory);
	}

	if (fresh == m_entries) {
		return;
	}

	beginResetModel();
	m_entries = std::move(fresh);
	endResetModel();
	emit countChanged();
}

bool ScreenshotListModel::isImageFile(const QString& fileName)
{
	static const QStringList kExtensions = {
		QStringLiteral("png"),
		QStringLiteral("jpg"),
		QStringLiteral("jpeg"),
	};
	return caseInsensitiveContains(kExtensions, QFileInfo(fileName).suffix());
}

QList<ScreenshotListModel::Entry>
ScreenshotListModel::listEntries(const QString& directory)
{
	QList<Entry> entries;
	if (directory.isEmpty()) {
		return entries;
	}

	QDir dir(directory);
	if (!dir.exists()) {
		return entries;
	}

	const QFileInfoList files =
		dir.entryInfoList(QDir::Files | QDir::Readable, QDir::NoSort);
	entries.reserve(files.size());
	for (const QFileInfo& info : files) {
		if (!isImageFile(info.fileName())) {
			continue;
		}
		Entry entry;
		entry.name = info.fileName();
		entry.path = info.absoluteFilePath();
		entry.modifiedMs = info.lastModified().toMSecsSinceEpoch();
		entry.size = info.size();
		entries.append(entry);
	}

	// Newest-modified first; the name is only a tiebreak, to keep the
	// order deterministic between two refreshes of files sharing an mtime.
	std::sort(entries.begin(), entries.end(),
			  [](const Entry& a, const Entry& b) {
				  if (a.modifiedMs != b.modifiedMs) {
					  return a.modifiedMs > b.modifiedMs;
				  }
				  return a.name > b.name;
			  });

	return entries;
}
