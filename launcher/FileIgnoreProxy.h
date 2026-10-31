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

#include <QFileInfo>
#include <QSortFilterProxyModel>
#include <QStringList>

#include "SeparatorPrefixTree.h"

/* A checkable view of a directory tree for the export dialogs: a
 * QFileSystemModel with a tri-state checkbox per row. Unchecking records the
 * entry as *blocked* (not in the archive); a blocked folder covers everything
 * under it, so the list (what `.packignore` holds and filterFile() answers
 * from) stays short. *Blocked* paths are the user's choice, shown unchecked
 * and remembered; *ignored* names, suffixes and paths are ours (logs, caches,
 * `.DS_Store`) and hidden from the tree. */
class FileIgnoreProxy : public QSortFilterProxyModel
{
	Q_OBJECT

  public:
	FileIgnoreProxy(QString root, QObject* parent);

	bool lessThan(const QModelIndex& left,
				  const QModelIndex& right) const override;

	Qt::ItemFlags flags(const QModelIndex& index) const override;

	QVariant data(const QModelIndex& index,
				  int role = Qt::DisplayRole) const override;
	bool setData(const QModelIndex& index, const QVariant& value,
				 int role = Qt::EditRole) override;

	QString relPath(const QString& path) const;

	bool setFilterState(QModelIndex index, Qt::CheckState state);

	void setBlockedPaths(QStringList paths);

	const SeparatorPrefixTree<'/'>& blockedPaths() const
	{
		return m_blocked;
	}
	SeparatorPrefixTree<'/'>& blockedPaths()
	{
		return m_blocked;
	}

	QStringList& ignoreFilesWithName()
	{
		return m_ignoreFiles;
	}
	QStringList& ignoreFilesWithSuffix()
	{
		return m_ignoreFilesSuffixes;
	}
	/* Same, by relative path; a folder here takes its contents with it. */
	SeparatorPrefixTree<'/'>& ignoreFilesWithPath()
	{
		return m_ignoreFilePaths;
	}

	/* True when `file` must be left out of the archive, for either reason.
	 * This is the function the export tasks are handed. */
	bool filterFile(const QFileInfo& file) const;

	void loadBlockedPathsFromFile(const QString& fileName);
	void saveBlockedPathsToFile(const QString& fileName);

  protected:
	bool filterAcceptsColumn(int source_column,
							 const QModelIndex& source_parent) const override;
	bool filterAcceptsRow(int source_row,
						  const QModelIndex& source_parent) const override;

	bool ignoreFile(const QFileInfo& file) const;

  private:
	const QString m_root;
	SeparatorPrefixTree<'/'> m_blocked;
	QStringList m_ignoreFiles;
	QStringList m_ignoreFilesSuffixes;
	SeparatorPrefixTree<'/'> m_ignoreFilePaths;
};
