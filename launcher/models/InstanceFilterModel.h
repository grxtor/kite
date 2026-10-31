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

#include <QCollator>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

/*
 * QML replacement for the widget grid's proxy (ui/instanceview/InstanceProxyModel.h)
 * with the same ordering: grouped, locale-sorted groups, natural-sort names
 * within a group, the same "InstSortMode" last-launch override. The Library's
 * "Time played" sort is a QML-only third value ("TotalTimePlayed") on that
 * setting. QtCore/QtGui only: iconKey is forwarded as the plain string
 * InstanceList exposes. The name, group, last-launch and total-time-played
 * roles are looked up by name from the source's roleNames(), so any model
 * exposing the same names works (see InstanceFilterModel_test.cpp).
 */
class InstanceFilterModel : public QSortFilterProxyModel
{
	Q_OBJECT

	Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY
				   filterTextChanged)
	Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	/* When set, `group` is matched exactly: the empty string then means "ungrouped
	 * only" instead of "every group". */
	Q_PROPERTY(bool exactGroup READ exactGroup WRITE setExactGroup NOTIFY
				   exactGroupChanged)
	/// When non-empty, only the instance with this id passes the filter.
	Q_PROPERTY(QString instanceId READ instanceId WRITE setInstanceId NOTIFY
				   instanceIdChanged)
	/* Most recently launched first, ignoring groups and the InstSortMode
	 * setting, and instances that were never launched are left out. */
	Q_PROPERTY(bool recentFirst READ recentFirst WRITE setRecentFirst NOTIFY
				   recentFirstChanged)
	/* Whether rows are ordered group by group (default). Off, the library is one
	 * list ordered only by InstSortMode. */
	Q_PROPERTY(bool grouped READ grouped WRITE setGrouped NOTIFY
				   groupedChanged)
	/// Distinct groups of the rows that pass the filter, in row order.
	Q_PROPERTY(QStringList groups READ groups NOTIFY groupsChanged)
	/// Id of the first row after filtering and sorting; empty when none.
	Q_PROPERTY(QString firstId READ firstId NOTIFY firstIdChanged)

  public:
	explicit InstanceFilterModel(QObject* parent = nullptr);

	QString filterText() const;
	void setFilterText(const QString& text);

	QString group() const;
	void setGroup(const QString& group);

	int count() const;

	bool exactGroup() const;
	void setExactGroup(bool exact);

	QString instanceId() const;
	void setInstanceId(const QString& id);

	bool recentFirst() const;
	void setRecentFirst(bool recentFirst);

	bool grouped() const;
	void setGrouped(bool grouped);

	QStringList groups() const;
	QString firstId() const;

	void setSourceModel(QAbstractItemModel* sourceModel) override;

  signals:
	void filterTextChanged();
	void groupChanged();
	void countChanged();
	void exactGroupChanged();
	void instanceIdChanged();
	void recentFirstChanged();
	void groupedChanged();
	void groupsChanged();
	void firstIdChanged();

  protected:
	bool filterAcceptsRow(int sourceRow,
						   const QModelIndex& sourceParent) const override;
	bool lessThan(const QModelIndex& left,
				  const QModelIndex& right) const override;

	/* The "InstSortMode" setting ("Name", "LastLaunch" or "TotalTimePlayed"), read
	 * from the live LauncherContext; a seam so tests can override it. */
	virtual QString sortModeSetting() const;

  private:
	bool subSortLessThan(const QModelIndex& left,
						  const QModelIndex& right) const;
	/// Recomputes groups and firstId, emitting only what actually moved.
	void refreshDerived();

	QCollator m_naturalSort;
	QString m_filterText;
	QString m_group;
	bool m_exactGroup = false;
	QString m_instanceId;
	bool m_recentFirst = false;
	bool m_grouped = true;
	QStringList m_groups;
	QString m_firstId;

	/* Resolved in setSourceModel(). Fallbacks are Qt's conventional roles. -1 for
	 * lastLaunch means the source has no such role, so LastLaunch sorting falls
	 * back to name. */
	int m_nameRole = Qt::DisplayRole;
	int m_groupRole = Qt::UserRole;
	int m_lastLaunchRole = -1;
	int m_idRole = -1;
	/// -1 when the source has no such role; "Time played" then falls back to name sorting.
	int m_totalTimePlayedRole = -1;
};
