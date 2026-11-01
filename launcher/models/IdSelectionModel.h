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

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

/*
 * Selection for the QML instance grid, keyed by instance id rather than by
 * QModelIndex: the grid's model is a QSortFilterProxyModel whose rows move on
 * every filter or sort change, and a QPersistentModelIndex is invalidated when
 * its row drops out of the filter (as while typing in the search box). An id
 * names the instance itself, so it survives every reorder and refilter.
 */
class IdSelectionModel : public QObject
{
	Q_OBJECT

	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(
		QStringList selectedIds READ selectedIds NOTIFY selectedIdsChanged)
	Q_PROPERTY(QString currentId READ currentId NOTIFY currentIdChanged)

  public:
	explicit IdSelectionModel(QObject* parent = nullptr);

	int count() const;
	/// Unordered -- backed by a QSet, not insertion order.
	QStringList selectedIds() const;
	QString currentId() const;

	Q_INVOKABLE void select(const QString& id);
	Q_INVOKABLE void deselect(const QString& id);
	Q_INVOKABLE void toggle(const QString& id);
	Q_INVOKABLE void clear();
	/// An empty id clears the selection (as clear()) instead of selecting "nothing".
	Q_INVOKABLE void selectOnly(const QString& id);
	Q_INVOKABLE bool isSelected(const QString& id) const;

  signals:
	/// Fires alongside any of the signals below.
	void changed();
	void countChanged();
	void selectedIdsChanged();
	void currentIdChanged();

  private:
	bool setCurrentIdInternal(const QString& id);

	QSet<QString> m_selected;
	QString m_currentId;
};
