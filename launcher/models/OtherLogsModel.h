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
#include <QString>
#include <QStringList>

#include "pathmatcher/IPathMatcher.h"

class RecursiveFileSystemWatcher;

/*
 * QML-facing bridge to an instance's "other" log files - logs/*.log* and
 * crash-reports/*.txt under its game root - the widget-free replacement for
 * OtherLogsPage. A plain list model of file names (role `name`); picking one
 * loads its text into `content`, gzip-decompressed the same way
 * OtherLogsPage::on_btnReload_clicked() does.
 *
 * Watches @p path for as long as this bridge lives (RecursiveFileSystemWatcher
 * itself, not the open/close toggling OtherLogsPage does per its BasePage
 * lifecycle) - InstanceDetails, which owns this, already only lives for as
 * long as the instance page is open, so there is no separate "page is
 * visible" state to toggle it against here.
 */
class OtherLogsModel : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(QString path READ path CONSTANT)
	/// The file selectFile() was last called with, or empty.
	Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
	/// `currentFile`'s text, or a placeholder for "too big to show" / "not
	/// readable" - see reload(), which mirrors OtherLogsPage's own
	/// handling of both.
	Q_PROPERTY(QString content READ content NOTIFY contentChanged)

  public:
	enum Roles { NameRole = Qt::UserRole };

	/// @p path / @p fileFilter: same as BaseInstance::getLogFileRoot() /
	/// getLogFileMatcher().
	explicit OtherLogsModel(const QString& path, IPathMatcher::Ptr fileFilter,
							QObject* parent = nullptr);
	~OtherLogsModel() override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index,
				 int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	QString path() const
	{
		return m_path;
	}
	QString currentFile() const
	{
		return m_currentFile;
	}
	QString content() const
	{
		return m_content;
	}

	/// Selects @p name (as listed by this model) and loads its content;
	/// clears both if @p name is empty or no longer exists.
	Q_INVOKABLE void selectFile(const QString& name);
	/// Re-reads `currentFile` from disk - for a "Reload" action, or after
	/// deleteCurrent() elsewhere changes what is on disk.
	Q_INVOKABLE void reload();
	/// Deletes `currentFile`. Returns false (and leaves it selected) if
	/// the delete failed; clears the selection on success.
	Q_INVOKABLE bool deleteCurrent();
	/// Deletes every file this model lists. Returns the names that could
	/// not be removed (empty means every file was deleted).
	Q_INVOKABLE QStringList deleteAll();

  signals:
	void currentFileChanged();
	void contentChanged();

  private slots:
	void onFilesChanged();

  private:
	void setContent(const QString& text);

	QString m_path;
	QString m_currentFile;
	QString m_content;
	RecursiveFileSystemWatcher* m_watcher;
};
