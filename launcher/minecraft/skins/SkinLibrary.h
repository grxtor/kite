/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

// The local skin library: every skin the user saved, as a list model.
//
// Backed by a directory of PNGs plus an "index.json" with the metadata a PNG
// cannot carry (arm width, cape, profile URL). The directory is the source of
// truth for which skins exist (files added or deleted by hand show up on the
// next rescan); the index only decorates files it recognises.
//
// The skin the account currently wears is adopted on rescan, so a fresh
// install opens with it in the list and selected. Entries are keyed by name
// (the file name without ".png", also the Qt::UserRole); callers hold names,
// not pointers or rows, because a rescan can reorder or replace the list.
class SkinLibrary : public QAbstractListModel
{
	Q_OBJECT

  public:
	// Named roles, so a QML ListView need not rely on "display"/"decoration".
	enum ModelRoles {
		NameRole = Qt::UserRole + 1,
		ImageRole
	};

	// `path` is the library directory; it is created if missing.
	SkinLibrary(QObject* parent, const QString& path,
				MinecraftAccountPtr account);
	~SkinLibrary() override;

	// Row of the entry with this name, or -1.
	int indexOfName(const QString& name) const;

	// Row of the entry whose texture URL matches what the account wears, or -1.
	// The dialog preselects it.
	int indexOfAccountSkin() const;

	const SkinEntry* entry(const QString& name) const;
	SkinEntry* entry(const QString& name);

	QString directory() const
	{
		return m_dir.absolutePath();
	}

	// Writes index.json; called on every mutation and on destruction.
	void save();

	// Folds an entry built elsewhere (an import) into the library: updates the
	// matching file's metadata, otherwise appends a new row.
	void mergeEntry(const SkinEntry& incoming);

	// Copies a PNG into the library. Returns an empty string on success, else a
	// reason the caller shows verbatim. `preferredName` overrides the file
	// name; a numeric suffix is appended if the name is taken, so nothing is
	// overwritten.
	QString importFile(const QString& sourcePath,
					   const QString& preferredName = QString());

	// Deletes a skin's file and its entry. `toTrash` asks for the system trash;
	// retry with false where that is unavailable.
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

  protected slots:
	void onDirectoryChanged(const QString& path);
	void onFileChanged(const QString& path);

	// Rebuilds the whole list from the directory and the index.
	void rescan();

  private:
	SkinLibrary(const SkinLibrary&) = delete;
	SkinLibrary& operator=(const SkinLibrary&) = delete;

	// Pulls the account's worn skin into `entries`, saving it to disk if new.
	// Returns true if the index needs writing.
	bool adoptAccountSkin(QList<SkinEntry>& entries) const;

	QFileSystemWatcher* m_watcher = nullptr;
	bool m_isWatching = false;
	QList<SkinEntry> m_entries;
	QDir m_dir;
	MinecraftAccountPtr m_account;
};
