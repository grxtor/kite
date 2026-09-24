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

#include "OtherLogsModel.h"

#include <QDir>
#include <QFile>

#include "FileSystem.h"
#include "GZip.h"
#include "RecursiveFileSystemWatcher.h"

namespace
{
	// Mirrors OtherLogsPage::on_btnReload_clicked()'s own ceilings.
	constexpr qint64 kTooBigToOpen = 1024ll * 1024ll * 12ll;
	constexpr qint64 kTooBigToShow = 50000000ll;
} // namespace

OtherLogsModel::OtherLogsModel(const QString& path, IPathMatcher::Ptr fileFilter,
							   QObject* parent)
	: QAbstractListModel(parent), m_path(path),
	  m_watcher(new RecursiveFileSystemWatcher(this))
{
	m_watcher->setMatcher(std::move(fileFilter));
	m_watcher->setRootDir(QDir::current().absoluteFilePath(m_path));
	connect(m_watcher, &RecursiveFileSystemWatcher::filesChanged, this,
			&OtherLogsModel::onFilesChanged);
	m_watcher->enable();
}

OtherLogsModel::~OtherLogsModel()
{
	m_watcher->disable();
}

int OtherLogsModel::rowCount(const QModelIndex& parent) const
{
	if (parent.isValid()) {
		return 0;
	}
	return m_watcher->files().size();
}

QVariant OtherLogsModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 ||
		index.row() >= m_watcher->files().size()) {
		return QVariant();
	}
	switch (role) {
		case Qt::DisplayRole:
		case NameRole:
			return m_watcher->files().at(index.row());
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> OtherLogsModel::roleNames() const
{
	QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
	roles.insert(NameRole, "name");
	return roles;
}

void OtherLogsModel::onFilesChanged()
{
	beginResetModel();
	endResetModel();

	// The selected file may have just been deleted (or renamed) out from
	// under us - same rule OtherLogsPage::populateSelectLogBox() applies.
	if (!m_currentFile.isEmpty() &&
		!QFile::exists(FS::PathCombine(m_path, m_currentFile))) {
		selectFile(QString());
	}
}

void OtherLogsModel::selectFile(const QString& name)
{
	if (m_currentFile == name) {
		return;
	}
	m_currentFile = name;
	emit currentFileChanged();
	reload();
}

void OtherLogsModel::setContent(const QString& text)
{
	if (m_content == text) {
		return;
	}
	m_content = text;
	emit contentChanged();
}

void OtherLogsModel::reload()
{
	if (m_currentFile.isEmpty()) {
		setContent(QString());
		return;
	}
	QFile file(FS::PathCombine(m_path, m_currentFile));
	if (!file.open(QFile::ReadOnly)) {
		setContent(tr("Unable to open %1 for reading: %2")
					   .arg(m_currentFile, file.errorString()));
		return;
	}
	if (file.size() > kTooBigToOpen) {
		setContent(tr("The file (%1) is too big. You may want to open it in "
					  "a viewer optimized for large files.")
					   .arg(file.fileName()));
		return;
	}
	QString content;
	if (file.fileName().endsWith(QStringLiteral(".gz"))) {
		QByteArray uncompressed;
		if (!GZip::unzip(file.readAll(), uncompressed)) {
			setContent(tr("The file (%1) is not readable.").arg(file.fileName()));
			return;
		}
		content = QString::fromUtf8(uncompressed);
	} else {
		content = QString::fromUtf8(file.readAll());
	}
	if (content.size() >= kTooBigToShow) {
		setContent(tr("The file (%1) is too big. You may want to open it in "
					  "a viewer optimized for large files.")
					   .arg(file.fileName()));
		return;
	}
	setContent(content);
}

bool OtherLogsModel::deleteCurrent()
{
	if (m_currentFile.isEmpty()) {
		return false;
	}
	QFile file(FS::PathCombine(m_path, m_currentFile));
	if (!file.remove()) {
		return false;
	}
	selectFile(QString());
	return true;
}

QStringList OtherLogsModel::deleteAll()
{
	QStringList failed;
	for (const QString& name : m_watcher->files()) {
		QFile file(FS::PathCombine(m_path, name));
		if (!file.remove()) {
			failed.append(name);
		}
	}
	// Same immediate check onFilesChanged() does once the filesystem
	// watcher notices - not worth waiting for that here too.
	if (!m_currentFile.isEmpty() &&
		!QFile::exists(FS::PathCombine(m_path, m_currentFile))) {
		selectFile(QString());
	}
	return failed;
}
