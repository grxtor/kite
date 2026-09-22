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
#include <QDir>
#include <QList>
#include <QString>

#include "minecraft/auth/MinecraftAccount.h"
#include "minecraft/skins/SkinEntry.h"

class QFileSystemWatcher;

/* The local skin library: every skin the user has saved, as a list model.
 *
 * Backed by a directory of PNG files plus an "index.json" holding the
 * metadata that a PNG cannot carry (arm width, cape, profile URL). The
 * directory is the source of truth for *which* skins exist -- files dropped
 * in by hand show up on the next rescan, and files deleted by hand disappear
 * -- while the index only decorates the files it recognises. That way the
 * library can never end up insisting on skins that are not there.
 *
 * The account is needed for more than display: whatever skin the account is
 * currently wearing is adopted into the library on rescan, so a fresh install
 * opens with the user's own skin already in the list and already selected.
 *
 * Entries are keyed by name (the file name without ".png"), which is also
 * what the model reports for Qt::UserRole. Callers hold names, not pointers
 * or row numbers, because a rescan can reorder or replace the whole list.
 */
class SkinLibrary : public QAbstractListModel
{
	Q_OBJECT

  public:
	/* Named roles, so a QML ListView can get at the name and image without
	 * relying on the generic "display"/"decoration" role names. */
	enum ModelRoles {
		NameRole = Qt::UserRole + 1,
		ImageRole
	};

	/* `path` is the library directory; it is created if missing. */
	SkinLibrary(QObject* parent, const QString& path,
				MinecraftAccountPtr account);
	~SkinLibrary() override;

	/* Row of the entry with this name, or -1. */
	int indexOfName(const QString& name) const;

	/* Row of the entry whose texture URL matches what the account is
	 * currently wearing, or -1 if the account's skin is not in the library.
	 * This is what the dialog preselects on open. */
	int indexOfAccountSkin() const;

	const SkinEntry* entry(const QString& name) const;
	SkinEntry* entry(const QString& name);

	QString directory() const
	{
		return m_dir.absolutePath();
	}

	/* Write index.json. Called on every mutation and on destruction. */
	void save();

	/* Fold an entry built elsewhere (an import, say) into the library:
	 * updates the metadata of the matching file if there is one, otherwise
	 * appends it as a new row. */
	void mergeEntry(const SkinEntry& incoming);

	/* Copy a PNG into the library.
	 *
	 * Returns an empty string on success, or a human-readable reason why
	 * not -- the caller shows it verbatim. `preferredName` overrides the
	 * source file name; either way a numeric suffix is appended if the name
	 * is taken, so importing never overwrites. */
	QString importFile(const QString& sourcePath,
					   const QString& preferredName = QString());

	/* Bulk form of importFile(), used by drag-and-drop. Failures are logged
	 * rather than reported: a drop of twelve files should not produce twelve
	 * modal dialogs. */
	void importFiles(const QStringList& sourcePaths);

	/* Delete a skin's file, and with it the entry. `toTrash` asks for the
	 * system trash; the caller is expected to retry with false if that is
	 * not available on this platform. */
	bool remove(const QString& name, bool toTrash);

	void startWatching();
	void stopWatching();

	/* QAbstractListModel */
	QVariant data(const QModelIndex& index,
				  int role = Qt::DisplayRole) const override;
	bool setData(const QModelIndex& index, const QVariant& value,
				 int role) override;
	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QHash<int, QByteArray> roleNames() const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	QStringList mimeTypes() const override;
	Qt::DropActions supportedDropActions() const override;
	bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
					  int column, const QModelIndex& parent) override;

  protected slots:
	void onDirectoryChanged(const QString& path);
	void onFileChanged(const QString& path);

	/* Rebuild the whole list from the directory and the index. */
	void rescan();

  private:
	SkinLibrary(const SkinLibrary&) = delete;
	SkinLibrary& operator=(const SkinLibrary&) = delete;

	/* Pull the account's currently worn skin into `entries`, saving it to
	 * disk if it is not there yet. Returns true if the index needs writing. */
	bool adoptAccountSkin(QList<SkinEntry>& entries) const;

	QFileSystemWatcher* m_watcher = nullptr;
	bool m_isWatching = false;
	QList<SkinEntry> m_entries;
	QDir m_dir;
	MinecraftAccountPtr m_account;
};
