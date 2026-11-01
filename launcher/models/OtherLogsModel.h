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
#include <QString>
#include <QStringList>

#include "pathmatcher/IPathMatcher.h"

class RecursiveFileSystemWatcher;

/*
 * QML-facing list of an instance's "other" log files (logs and crash reports
 * under its game root), role `name`. Picking one loads its text into `content`,
 * gzip-decompressed. Watches @p path for the bridge's lifetime.
 */
class OtherLogsModel : public QAbstractListModel
{
	Q_OBJECT

	Q_PROPERTY(QString path READ path CONSTANT)
	Q_PROPERTY(QString currentFile READ currentFile NOTIFY currentFileChanged)
	/// `currentFile`'s text, or a placeholder for "too big to show" / "not readable".
	Q_PROPERTY(QString content READ content NOTIFY contentChanged)

  public:
	enum Roles { NameRole = Qt::UserRole };

	/// @p path / @p fileFilter: as BaseInstance::getLogFileRoot()/getLogFileMatcher().
	/// @p recursive false lists only @p path itself (the launcher's own logs).
	explicit OtherLogsModel(const QString& path, IPathMatcher::Ptr fileFilter,
							QObject* parent = nullptr, bool recursive = true);
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

	/// Clears both if @p name is empty or no longer exists.
	Q_INVOKABLE void selectFile(const QString& name);
	Q_INVOKABLE void reload();
	/// False (and stays selected) if the delete failed.
	Q_INVOKABLE bool deleteCurrent();
	/// Returns the names that could not be removed.
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
