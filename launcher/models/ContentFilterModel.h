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

#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <memory>

class Mod;
class ModFolderModel;
class ModMetadataIndex;

/*
 * What ContentTab.qml lists for one content folder: the ModFolderModel sorted
 * by name and narrowed by a search (name, description, authors and file name),
 * plus per-file details as roles. Rows are addressed by `key`
 * (the file name without ".disabled", ModMetadataIndex::canonicalFileName()),
 * which survives sorting, searching and switching a file off and on. Holds a
 * shared_ptr to its folder model, so a model the instance has replaced (the
 * global data pack folder is rebuilt when its settings change) stays valid
 * until this proxy is repointed.
 */
class ContentFilterModel : public QSortFilterProxyModel
{
	Q_OBJECT

	/// Case-insensitive; empty shows everything.
	Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY
				   filterTextChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)

  public:
	/* Past ModFolderModel's own named roles (Qt::UserRole..+4), passed through untouched. */
	enum ExtraRoles {
		KeyRole = Qt::UserRole + 100,
		FileNameRole,
		DescriptionRole,
		AuthorsRole,
		/// Platform project page, or the address the file declares; empty if neither.
		HomepageRole,
		/// "Modrinth" / "CurseForge", or empty.
		SourceRole
	};

	explicit ContentFilterModel(QObject* parent = nullptr);
	~ContentFilterModel() override;

	void setFolderModel(std::shared_ptr<ModFolderModel> model);
	ModFolderModel* folderModel() const
	{
		return m_folder.get();
	}

	QHash<int, QByteArray> roleNames() const override;
	QVariant data(const QModelIndex& index,
				  int role = Qt::DisplayRole) const override;

	QString filterText() const
	{
		return m_filterText;
	}
	void setFilterText(const QString& text);
	int count() const;
	int totalCount() const;

	/// Source indexes of the rows with these keys, searched or not; unknown keys are skipped.
	QModelIndexList sourceIndexesForKeys(const QStringList& keys) const;
	Q_INVOKABLE QStringList visibleKeys() const;
	/// Re-announces the rows with @p keys, for a change the folder model
	/// cannot see (the sidecar behind `source`/`homepage`).
	void refreshRows(const QStringList& keys);

	/// ModMetadataIndex::canonicalFileName() of the mod's file.
	static QString keyFor(const Mod& mod);
	/// Platform page first, the address the file declares second. @p index may be null.
	static QString homepageFor(const Mod& mod, const ModMetadataIndex* index);
	/// "Modrinth" / "CurseForge" / "". @p index may be null.
	static QString sourceFor(const Mod& mod, const ModMetadataIndex* index);
	/// Whether @p text (trimmed) appears in the name, description, authors or file name, ignoring case.
	static bool matches(const Mod& mod, const QString& text);

  signals:
	void filterTextChanged();
	void countChanged();

  protected:
	bool filterAcceptsRow(int sourceRow,
						  const QModelIndex& sourceParent) const override;
	bool lessThan(const QModelIndex& left,
				  const QModelIndex& right) const override;

  private:
	std::shared_ptr<ModFolderModel> m_folder;
	QString m_filterText;
};
