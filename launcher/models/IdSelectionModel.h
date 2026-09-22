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

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

/*
 * Selection for the QML instance grid, keyed by instance id string rather
 * than by QModelIndex/QPersistentModelIndex.
 *
 * QML has no QItemSelectionModel to begin with, but even a hand-rolled
 * index-based equivalent would be the wrong tool here: the grid's model is
 * InstanceFilterModel, a QSortFilterProxyModel, and its row numbers move
 * every time the filter text, the group filter or the sort mode changes. A
 * plain QModelIndex would silently point at a different instance after a
 * reorder; a QPersistentModelIndex survives a reorder but is invalidated
 * outright whenever the row it names drops out of the filter -- which is
 * exactly what happens while the user is typing into the search box that
 * owns filterText. An instance id has neither problem: it names the
 * instance itself, not a position in some particular view of it, so it
 * keeps meaning the same thing across every reorder and refilter.
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
	/// The most recently selected id, or empty when nothing is selected.
	QString currentId() const;

	Q_INVOKABLE void select(const QString& id);
	Q_INVOKABLE void deselect(const QString& id);
	Q_INVOKABLE void toggle(const QString& id);
	Q_INVOKABLE void clear();
	/// Replaces the whole selection with just @p id. An empty id clears the
	/// selection instead, matching clear() rather than selecting "nothing".
	Q_INVOKABLE void selectOnly(const QString& id);
	Q_INVOKABLE bool isSelected(const QString& id) const;

  signals:
	/// Fires alongside whichever of the signals below also fire, so a
	/// binding that only cares "did anything change" does not need to listen
	/// to all three.
	void changed();
	void countChanged();
	void selectedIdsChanged();
	void currentIdChanged();

  private:
	/// Returns whether @p id actually replaced the previous current id, and
	/// emits currentIdChanged() when it did.
	bool setCurrentIdInternal(const QString& id);

	QSet<QString> m_selected;
	QString m_currentId;
};
