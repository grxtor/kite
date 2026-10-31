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
#include <QFileSystemWatcher>
#include <QList>
#include <QString>
#include <QTimer>

/* A QAbstractListModel over one directory's image files (png/jpg/jpeg, not
 * recursive), newest-modified first. Thumbnails are left to a QML image
 * provider using the `path`/`modified` roles.
 *
 * A missing `directory` is empty and never created, nor watched
 * (QFileSystemWatcher::addPath() no-ops); each refresh re-arms the watch
 * once it exists. directoryChanged() restarts a single-shot timer, so a
 * burst of events is one refresh, and a refresh that finds the same files,
 * sizes and mtimes leaves the model untouched. */
class ScreenshotListModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(
		QString directory READ directory WRITE setDirectory NOTIFY directoryChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)

  public:
	enum Roles {
		NameRole = Qt::UserRole + 1, ///< File name, with extension.
		PathRole,                    ///< Absolute path on disk.
		UrlRole,                     ///< "file://" URL, e.g. for Image.source.
		ModifiedRole,                ///< Last-modified time, ms since epoch.
		SizeRole,                    ///< File size in bytes.
	};
	Q_ENUM(Roles)

	explicit ScreenshotListModel(QObject* parent = nullptr);
	~ScreenshotListModel() override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index,
				  int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	QString directory() const
	{
		return m_directory;
	}
	void setDirectory(const QString& directory);

	int count() const
	{
		return m_entries.size();
	}

	/* Removes the file at `row`: to the platform trash when available, else
	 * deleted outright; logged either way. False for a bad row or a
	 * filesystem failure; the row is removed from the model only on success. */
	Q_INVOKABLE bool remove(int row);
	/* Absolute path of the file at `row`, or empty if out of range. */
	Q_INVOKABLE QString pathAt(int row) const;
	/* Renames the file at `row` to `newName` (see renamedFileName()). False,
	 * with nothing changed, for a bad row, an unusable or taken name, or a
	 * filesystem failure. The row keeps its place: the sort key is the
	 * modification time. */
	Q_INVOKABLE bool rename(int row, const QString& newName);
	/* Puts the picture at `row` on the clipboard as an image. False for a
	 * bad row, an unreadable image, or no clipboard (no GUI application). */
	Q_INVOKABLE bool copyImage(int row) const;
	/* The file name `input` asks for given `currentName`, or empty if unusable
	 * (empty after trimming, or containing a path separator or a character no
	 * common filesystem allows). A typed png/jpg/jpeg extension is kept,
	 * otherwise the current extension is added back. */
	static QString renamedFileName(const QString& currentName,
								   const QString& input);

  signals:
	void directoryChanged();
	void countChanged();

  private slots:
	void onWatcherDirectoryChanged(const QString& path);
	void refreshNow();

  private:
	struct Entry {
		QString name;
		QString path;
		qint64 modifiedMs = 0;
		qint64 size = 0;

		bool operator==(const Entry& other) const
		{
			return path == other.path && modifiedMs == other.modifiedMs &&
				   size == other.size;
		}
	};

	static QList<Entry> listEntries(const QString& directory);
	static bool isImageFile(const QString& fileName);

	// Coalesces a burst of directoryChanged() signals into one refresh.
	static constexpr int kRefreshDebounceMs = 150;

	QString m_directory;
	QList<Entry> m_entries;
	QFileSystemWatcher* m_watcher;
	QTimer m_refreshTimer;
};
