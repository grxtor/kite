/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QDir>
#include <QAbstractListModel>

#include "Mod.h"

#include "ModFolderLoadTask.h"
#include "LocalModParseTask.h"
#include "ModMetadataIndex.h"

#include <memory>

class LegacyInstance;
class BaseInstance;
class QFileSystemWatcher;

/**
 * A legacy mod list.
 * Backed by a folder.
 */
class ModFolderModel : public QAbstractListModel
{
	Q_OBJECT
  public:
	enum Columns {
		ActiveColumn = 0,
		NameColumn,
		VersionColumn,
		DateColumn,
		NUM_COLUMNS
	};
	/* Named roles, so a QML ListView (which only ever sees column 0) can
	 * still get at what the widget views show as columns 1..3. */
	enum ModelRoles {
		NameRole = Qt::UserRole,
		VersionRole,
		DateRole,
		EnabledRole
	};
	enum ModStatusAction { Disable, Enable, Toggle };
	ModFolderModel(const QString& dir);

	virtual QVariant data(const QModelIndex& index,
						  int role = Qt::DisplayRole) const override;
	virtual bool setData(const QModelIndex& index, const QVariant& value,
						 int role = Qt::EditRole) override;
	QHash<int, QByteArray> roleNames() const override;
	Qt::DropActions supportedDropActions() const override;

	/// flags, mostly to support drag&drop
	virtual Qt::ItemFlags flags(const QModelIndex& index) const override;
	QStringList mimeTypes() const override;
	bool dropMimeData(const QMimeData* data, Qt::DropAction action, int row,
					  int column, const QModelIndex& parent) override;

	virtual int rowCount(const QModelIndex&) const override
	{
		return size();
	}

	virtual QVariant headerData(int section, Qt::Orientation orientation,
								int role = Qt::DisplayRole) const override;
	virtual int columnCount(const QModelIndex& parent) const override;

	size_t size() const
	{
		return mods.size();
	};
	bool empty() const
	{
		return size() == 0;
	}
	Mod& operator[](size_t index)
	{
		return mods[index];
	}
	const Mod& at(size_t index) const
	{
		return mods.at(index);
	}

	/// Reloads the mod list and returns true if the list changed.
	bool update();

	/**
	 * Adds the given mod to the list at the given index - if the list supports
	 * custom ordering
	 */
	bool installMod(const QString& filename);

	/// Deletes all the selected mods
	bool deleteMods(const QModelIndexList& indexes);

	/// Enable or disable listed mods
	bool setModStatus(const QModelIndexList& indexes, ModStatusAction action);

	void startWatching();
	void stopWatching();

	bool isValid();

	QDir dir()
	{
		return m_dir;
	}

	const QList<Mod>& allMods()
	{
		return mods;
	}

	/* Access to the persistent install metadata for this folder.
	 * Lifetime is bound to the model. May be null only before construction
	 * completes; otherwise always non-null. Use this from install and
	 * dependency-resolution code to skip already-installed projects and
	 * to drive update checks. */
	std::shared_ptr<ModMetadataIndex> metadataIndex() const
	{
		return m_metadata;
	}

  public slots:
	void disableInteraction(bool disabled);

  private slots:
	void directoryChanged(QString path);
	void finishUpdate();
	void finishModParse(int token);

  signals:
	void updateFinished();

  private:
	bool setModStatus(int index, ModStatusAction action);

	/* Shared by the Qt::DisplayRole VersionColumn case and the named
	 * VersionRole, so QML gets the same "Folder"/"File" placeholder the
	 * Version column shows for those mod types. */
	QVariant versionData(int row) const;

  protected:
	void resolveMod(Mod& m);

	/* Gate for "Add" / drag-and-drop installs.
	 *
	 * The base model accepts anything Mod recognises, which is right for
	 * mods and resource packs - both are identified by the folder they
	 * live in rather than by their contents. Subclasses backing a folder
	 * whose format has a mandatory on-disk layout (shader packs, data
	 * packs) override this so a file that would silently do nothing
	 * in-game is rejected up front instead of quietly sitting there.
	 *
	 * Called before the file is copied into the folder, so `file` still
	 * points at the user's original path. */
	virtual bool acceptsFile(const QFileInfo& file, Mod::ModType type) const
	{
		Q_UNUSED(file)
		Q_UNUSED(type)
		return true;
	}

  protected:
	QFileSystemWatcher* m_watcher;
	bool is_watching = false;
	ModFolderLoadTask::ResultPtr m_update;
	bool scheduled_update = false;
	bool interaction_disabled = false;
	QDir m_dir;
	QMap<QString, int> modsIndex;
	QMap<int, LocalModParseTask::ResultPtr> activeTickets;
	int nextResolutionTicket = 0;
	QList<Mod> mods;
	std::shared_ptr<ModMetadataIndex> m_metadata;
};
