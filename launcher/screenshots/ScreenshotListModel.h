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
#include <QFileSystemWatcher>
#include <QList>
#include <QString>
#include <QTimer>

/*
 * A QAbstractListModel over one directory's image files (png/jpg/jpeg,
 * not recursive), newest-modified first -- the widget-free
 * replacement for the QFileSystemModel + FilterModel pair
 * ui/pages/instance/ScreenshotsPage.cpp builds today. That page's
 * thumbnailing (ThumbnailRunnable, the RWStorage<QString, QIcon> cache) is
 * deliberately not ported here: it produces QIcon/QPixmap, which are
 * GUI-only types this widget-free model must not touch. A QML-facing
 * thumbnail provider (image:// scheme) is expected to sit on top of the
 * `path`/`modified` roles this model exposes instead.
 *
 * DIRECTORY. Sourced from one flat directory, set with `directory`. A
 * directory that does not exist (yet) is treated as empty rather than an
 * error -- e.g. an instance that has never had a screenshot taken has no
 * screenshots/ folder on disk at all. This does not create the folder
 * itself; whoever wires this to a real instance is expected to do that the
 * same way ScreenshotsPage::openedImpl() calls FS::ensureFolderPathExists()
 * today, if screenshots should be creatable from an empty state.
 *
 * WATCHING. A QFileSystemWatcher on `directory` drives refreshes, but the
 * watcher's directoryChanged() only restarts a short single-shot debounce
 * timer rather than refreshing immediately: several filesystem events in a
 * quick burst (e.g. importing a batch of screenshots) collapse into one
 * refresh instead of one reset per event. A refresh that finds nothing
 * actually changed (same set of files, same sizes, same modification
 * times) leaves the model untouched -- no reset, no signal -- so an
 * unrelated touch of the directory (e.g. another file being renamed away
 * from *.png) does not disturb a bound view.
 *
 * Only the directory itself is watched, not each file individually (unlike
 * ScreenshotsPage's FilterModel, whose per-file QFileSystemWatcher entries
 * are never pruned when a file goes away -- see the FIXME in
 * ScreenshotsPage.cpp). A file being watched for content changes has no
 * analogue here: this model only cares about which files exist and their
 * mtime/size, both of which directoryChanged() already correlates with.
 *
 * If `directory` does not exist yet when set (or when it disappears, e.g.
 * the instance folder being deleted out from under the launcher), it is
 * not watched at all, since QFileSystemWatcher::addPath() silently no-ops
 * on a path that is not there. Each refresh re-arms the watch once the
 * directory exists again, so the very next externally-triggered refresh
 * (a later setDirectory() call, for instance) picks it back up -- but nothing
 * polls in between purely to notice the directory's own re-creation.
 */
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

	/* Removes the file backing row `row` from disk: moved to the
	 * platform's trash when one is available (FS::trash(), so the user can
	 * put it back), or deleted outright otherwise -- either way logged, so
	 * which of the two happened is at least discoverable from the log.
	 * Returns false for an out-of-range row or if the filesystem operation
	 * itself failed; the model row is only removed on success, matching
	 * WorldList::deleteWorld()/ModFolderModel::deleteMods() -- a failed
	 * delete must not leave the model claiming a file is gone when it is
	 * still sitting on disk. */
	Q_INVOKABLE bool remove(int row);
	/* Absolute path of the file at `row`, or an empty string if `row` is
	 * out of range. */
	Q_INVOKABLE QString pathAt(int row) const;

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
