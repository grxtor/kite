/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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
#include <QSet>

#include "launcherimport/LauncherImportTypes.h"

/*
 * The instances other launchers have, one row each, grouped by launcher
 * (the "launcherName" role, for a ListView section), most recently played
 * first. Holds what the user picked: whether a row is ticked and whether it
 * comes over as a copy or a link.
 */
class LauncherImportModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

  public:
	enum Role {
		KeyRole = Qt::UserRole + 1,
		LauncherRole,
		LauncherNameRole,
		NameRole,
		MinecraftVersionRole,
		LoaderRole,
		LoaderVersionRole,
		SizeTextRole,
		LastPlayedRole,
		CheckedRole,
		LinkedRole,
		LinkableRole,
		LinkBlockerRole,
		ProblemRole,
		ImportedRole,
		LeftBehindRole
	};

	struct Row {
		LauncherImport::FoundInstance instance;
		bool checked = false;
		LauncherImport::Mode mode = LauncherImport::Mode::Copy;
		/* Already brought over (an instance in Kite names it). */
		bool imported = false;
	};

	explicit LauncherImportModel(QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	/* Replaces the rows; @p importedKeys are the keys instances in Kite
	 * already carry. */
	void reset(const QList<LauncherImport::FoundLauncher>& launchers,
			   const QSet<QString>& importedKeys);

	const QList<Row>& rows() const
	{
		return m_rows;
	}
	/* -1 when no row has @p key. */
	int indexOf(const QString& key) const;

	/* Only importable rows take a tick, and only linkable ones a link. */
	bool setChecked(int row, bool checked);
	bool setMode(int row, LauncherImport::Mode mode);
	void setImported(int row);

  signals:
	void countChanged();

  private:
	void changed(int row, const QList<int>& roles);

	QList<Row> m_rows;
};
