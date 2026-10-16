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

#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>
#include <memory>

class Mod;
class ModFolderModel;
class ModMetadataIndex;

/*
 * What ContentTab.qml lists for one content folder: the folder's
 * ModFolderModel sorted by name and narrowed by a search, plus the details
 * the widget page showed in its info panel (MCModInfoFrame) as roles of
 * their own, so one delegate can show them without a second lookup.
 *
 * The QML-side replacement for ModFolderPage's ModSortProxy: the search
 * matches the same fields that proxy did (name, description, authors),
 * plus the file name, which is how people tell two copies of a mod apart.
 *
 * Rows are addressed by `key` (the file name without a ".disabled"
 * suffix, ModMetadataIndex::canonicalFileName()) rather than by row: rows
 * move as the list is sorted, searched and toggled, but a key stays the
 * same when a file is switched off and on again, so a selection made in
 * QML survives all three.
 *
 * Holds a shared_ptr to its folder model, so a model the instance has
 * already replaced (the global data pack folder is rebuilt whenever its
 * settings change) stays valid until this proxy is pointed at the new one.
 *
 * Core, like the rest of models/: QtCore only, no QtWidgets, no ui/.
 */
class ContentFilterModel : public QSortFilterProxyModel
{
	Q_OBJECT

	/// Case-insensitive; empty shows everything.
	Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY
				   filterTextChanged)
	/// Rows left after the search.
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	/// Rows in the folder, searched or not.
	Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged)

  public:
	/* Past ModFolderModel's own named roles (Qt::UserRole..+4), which this
	 * proxy passes through untouched. */
	enum ExtraRoles {
		KeyRole = Qt::UserRole + 100,
		FileNameRole,
		DescriptionRole,
		/// QStringList.
		AuthorsRole,
		/// The project page on the platform it was installed from, or the
		/// address the file itself declares; empty when neither exists.
		HomepageRole,
		/// "Modrinth" / "CurseForge" when the file was installed from one,
		/// empty otherwise.
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

	/// Source indexes (into folderModel()) of the rows with these keys,
	/// searched or not; keys that name nothing are skipped.
	QModelIndexList sourceIndexesForKeys(const QStringList& keys) const;
	/// The keys of the rows the search currently shows, top to bottom.
	Q_INVOKABLE QStringList visibleKeys() const;
	/// Re-announces the rows with @p keys, for a change the folder model
	/// cannot see (the sidecar behind `source`/`homepage`).
	void refreshRows(const QStringList& keys);

	/// ModMetadataIndex::canonicalFileName() of the mod's file.
	static QString keyFor(const Mod& mod);
	/// Same rule as ModFolderPage::homepageForRow(): the page of the
	/// platform the file was installed from first, the address the file
	/// declares itself second. @p index may be null.
	static QString homepageFor(const Mod& mod, const ModMetadataIndex* index);
	/// "Modrinth" / "CurseForge" / "". @p index may be null.
	static QString sourceFor(const Mod& mod, const ModMetadataIndex* index);
	/// Whether @p text (already trimmed) appears in the mod's name,
	/// description, authors or file name, ignoring case.
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
