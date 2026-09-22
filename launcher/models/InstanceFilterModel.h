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

/*
 * QML replacement for the widget grid's proxy
 * (ui/instanceview/InstanceProxyModel.h). QML has no QItemSelectionModel and
 * cannot reach into the widget layer, so this is a second proxy rather than a
 * shared one -- but it reproduces that proxy's ordering exactly (grouped,
 * locale-sorted groups, natural-sort names within a group, with the same
 * "InstSortMode" last-launch override) so that switching between the widget
 * grid and the QML one never reorders anyone's instances.
 *
 * QtCore/QtGui only, same rule as the rest of the core: no QtWidgets, nothing
 * from ui/. In particular this cannot resolve icons the way the old proxy did
 * (APPLICATION->icons()->getIcon() in its data() override) -- APPLICATION is
 * a QApplication and off limits here. iconKey is forwarded as the plain
 * string InstanceList already exposes it as; whatever turns that into a
 * picture is the QML delegate's problem, not this proxy's.
 *
 * The name, group and last-launch roles are looked up by NAME from the
 * source model's roleNames() rather than assumed to be
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

  public:
	explicit InstanceFilterModel(QObject* parent = nullptr);

	QString filterText() const;
	void setFilterText(const QString& text);

	QString group() const;
	void setGroup(const QString& group);

	/// Row count after filtering -- what QML needs to decide an empty state.
	int count() const;

	void setSourceModel(QAbstractItemModel* sourceModel) override;

  signals:
	void filterTextChanged();
	void groupChanged();
	void countChanged();

  protected:
	bool filterAcceptsRow(int sourceRow,
						   const QModelIndex& sourceParent) const override;
	bool lessThan(const QModelIndex& left,
				  const QModelIndex& right) const override;

  private:
	bool subSortLessThan(const QModelIndex& left,
						  const QModelIndex& right) const;

	QCollator m_naturalSort;
	QString m_filterText;
	QString m_group;

	/* Resolved in setSourceModel(). The fallbacks are the roles Qt itself
	 * gives the same meaning by convention, so a source model that never
	 * names its roles still filters and sorts by something sane instead of
	 * silently matching nothing. -1 for lastLaunch means "this source has no
	 * such role", which makes the LastLaunch sort mode fall back to name
	 * sorting instead of comparing garbage. */
	int m_nameRole = Qt::DisplayRole;
	int m_groupRole = Qt::UserRole;
	int m_lastLaunchRole = -1;
};
