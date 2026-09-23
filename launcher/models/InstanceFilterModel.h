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

#include <QCollator>
#include <QSortFilterProxyModel>
#include <QString>
#include <QStringList>

/*
 * QML replacement for the widget grid's proxy
 * (ui/instanceview/InstanceProxyModel.h). QML has no QItemSelectionModel and
 * cannot reach into the widget layer, so this is a second proxy rather than a
 * shared one -- but it reproduces that proxy's ordering exactly (grouped,
 * locale-sorted groups, natural-sort names within a group, with the same
 * "InstSortMode" last-launch override) so that switching between the widget
 * grid and the QML one never reorders anyone's instances. The Library
 * toolbar's own "Time played" sort is a QML-only third value
 * ("TotalTimePlayed") on that same setting -- the classic widget page never
 * writes it, but it re-sorts both UIs' proxies the same way "LastLaunch"
 * already does.
 *
 * QtCore/QtGui only, same rule as the rest of the core: no QtWidgets, nothing
 * from ui/. In particular this cannot resolve icons the way the old proxy did
 * (APPLICATION->icons()->getIcon() in its data() override) -- APPLICATION is
 * a QApplication and off limits here. iconKey is forwarded as the plain
 * string InstanceList already exposes it as; whatever turns that into a
 * picture is the QML delegate's problem, not this proxy's.
 *
 * The name, group, last-launch and total-time-played roles are looked up by
 * NAME from the source model's roleNames() rather than assumed to be
 * Qt::DisplayRole/GroupRole/LastLaunchRole, so this works against
 * InstanceList's real role numbers (see InstanceList.h -- GroupRole is
 * pinned to Qt::UserRole, the QML-only roles start at Qt::UserRole + 10) and
 * against any other model that merely exposes the same names (see
 * InstanceFilterModel_test.cpp, which deliberately numbers them
 * differently).
 */
class InstanceFilterModel : public QSortFilterProxyModel
{
	Q_OBJECT

	Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY
				   filterTextChanged)
	Q_PROPERTY(QString group READ group WRITE setGroup NOTIFY groupChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	/* When set, `group` is matched exactly -- including the empty string,
	 * which then means "ungrouped only" instead of "every group". This is
	 * what a per-group section of the library needs. */
	Q_PROPERTY(bool exactGroup READ exactGroup WRITE setExactGroup NOTIFY
				   exactGroupChanged)
	/// When non-empty, only the instance with this id passes the filter.
	Q_PROPERTY(QString instanceId READ instanceId WRITE setInstanceId NOTIFY
				   instanceIdChanged)
	/* Most recently launched first, ignoring groups and the InstSortMode
	 * setting, and instances that were never launched are left out. */
	Q_PROPERTY(bool recentFirst READ recentFirst WRITE setRecentFirst NOTIFY
				   recentFirstChanged)
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

	/// Row count after filtering -- what QML needs to decide an empty state.
	int count() const;

	bool exactGroup() const;
	void setExactGroup(bool exact);

	QString instanceId() const;
	void setInstanceId(const QString& id);

	bool recentFirst() const;
	void setRecentFirst(bool recentFirst);

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
	void groupsChanged();
	void firstIdChanged();

  protected:
	bool filterAcceptsRow(int sourceRow,
						   const QModelIndex& sourceParent) const override;
	bool lessThan(const QModelIndex& left,
				  const QModelIndex& right) const override;

	/* The "InstSortMode" setting's current value ("Name", "LastLaunch" or
	 * the QML library's own "TotalTimePlayed"), read from the live
	 * LauncherContext. A seam purely for tests: subSortLessThan() cannot
	 * otherwise be exercised without constructing a whole LauncherContext,
	 * so a test subclass overrides this instead. */
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
	QStringList m_groups;
	QString m_firstId;

	/* Resolved in setSourceModel(). The fallbacks are the roles Qt itself
	 * gives the same meaning by convention, so a source model that never
	 * names its roles still filters and sorts by something sane instead of
	 * silently matching nothing. -1 for lastLaunch means "this source has no
	 * such role", which makes the LastLaunch sort mode fall back to name
	 * sorting instead of comparing garbage. */
	int m_nameRole = Qt::DisplayRole;
	int m_groupRole = Qt::UserRole;
	int m_lastLaunchRole = -1;
	int m_idRole = -1;
	/// -1 when the source has no such role, same convention as
	/// m_lastLaunchRole -- the "Time played" InstSortMode value then falls
	/// back to name sorting instead of comparing garbage.
	int m_totalTimePlayedRole = -1;
};
