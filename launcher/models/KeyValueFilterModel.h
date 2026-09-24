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

/*
 * Filters a `key`/`value`-role source model (GameOptions today) to rows
 * where either role contains `filterText`, case-insensitively - a QML
 * search box's WRITE target, since QSortFilterProxyModel's own
 * setFilterFixedString()/setFilterKeyColumn() are plain C++ methods QML
 * cannot call. Empty `filterText` keeps every row.
 *
 * Not specific to GameOptions - filterAcceptsRow() falls back to matching
 * every role a row's roleNames() has string data for as soon as it sees
 * something other than key/value, so it stays correct if it is ever pointed
 * at a different two-column model. QtCore only, same rule as the rest of
 * models/: no QtWidgets, no ui/.
 */
class KeyValueFilterModel : public QSortFilterProxyModel
{
	Q_OBJECT

	Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY
				   filterTextChanged)

  public:
	explicit KeyValueFilterModel(QObject* parent = nullptr);

	QString filterText() const
	{
		return m_filterText;
	}
	void setFilterText(const QString& text);

  signals:
	void filterTextChanged();

  protected:
	bool filterAcceptsRow(int sourceRow,
						  const QModelIndex& sourceParent) const override;

  private:
	QString m_filterText;
};
