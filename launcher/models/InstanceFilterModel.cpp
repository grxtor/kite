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

#include "InstanceFilterModel.h"

#include <QLocale>

#include "core/LauncherContext.h"
#include "settings/SettingsObject.h"

namespace
{
	/* First role in @p model's roleNames() named @p name, or @p fallback if
	 * @p model is null or names no role that way. */
	int roleByName(const QAbstractItemModel* model, const QByteArray& name,
					int fallback)
	{
		if (!model) {
			return fallback;
		}
		const auto roles = model->roleNames();
		for (auto it = roles.constBegin(); it != roles.constEnd(); ++it) {
			if (it.value() == name) {
				return it.key();
			}
		}
		return fallback;
	}

	/* Natural order that does not depend on QCollator::setNumericMode(),
	 * which some backends silently ignore (Qt without ICU on Linux sorted
	 * "Pack 10" before "Pack 2"). Runs of digits compare by value -- first
	 * by length once leading zeros are dropped, then digit by digit, so any
	 * length works -- and everything between them goes through @p collator.
	 */
	int naturalCompare(const QCollator& collator, const QString& a,
					   const QString& b)
	{
		int i = 0;
		int j = 0;
		while (i < a.size() && j < b.size()) {
			const bool digitA = a.at(i).isDigit();
			const bool digitB = b.at(j).isDigit();
			int endA = i;
			while (endA < a.size() && a.at(endA).isDigit() == digitA)
				++endA;
			int endB = j;
			while (endB < b.size() && b.at(endB).isDigit() == digitB)
				++endB;
			const QStringView runA = QStringView(a).mid(i, endA - i);
			const QStringView runB = QStringView(b).mid(j, endB - j);

			int result = 0;
			if (digitA && digitB) {
				QStringView numA = runA;
				QStringView numB = runB;
				while (numA.size() > 1 && numA.front() == u'0')
					numA = numA.mid(1);
				while (numB.size() > 1 && numB.front() == u'0')
					numB = numB.mid(1);
				result = numA.size() == numB.size()
							 ? numA.compare(numB)
							 : (numA.size() < numB.size() ? -1 : 1);
			} else {
				result = collator.compare(runA, runB);
			}
			if (result != 0)
				return result < 0 ? -1 : 1;
			i = endA;
			j = endB;
		}
		if (i < a.size())
			return 1;
		if (j < b.size())
			return -1;
		return collator.compare(a, b);
	}
} // namespace

InstanceFilterModel::InstanceFilterModel(QObject* parent)
	: QSortFilterProxyModel(parent)
{
	// Numbers are handled by naturalCompare(); numeric mode stays on for
	// backends that honour it, so a tie between runs orders the same way.
	m_naturalSort.setNumericMode(true);
	m_naturalSort.setCaseSensitivity(Qt::CaseInsensitive);
	// FIXME: same as the widget proxy -- use loaded translation as source of
	// locale instead, hook this up to translation changes.
	m_naturalSort.setLocale(QLocale::system());

	// rowCount() changes on all four of these; simplest to treat them all as
	// "the filtered count might have moved" rather than track it by hand.
	connect(this, &QAbstractItemModel::rowsInserted, this,
			&InstanceFilterModel::countChanged);
	connect(this, &QAbstractItemModel::rowsRemoved, this,
			&InstanceFilterModel::countChanged);
	connect(this, &QAbstractItemModel::modelReset, this,
			&InstanceFilterModel::countChanged);
	connect(this, &QAbstractItemModel::layoutChanged, this,
			&InstanceFilterModel::countChanged);

	// groups and firstId follow the same row changes, plus plain data
	// changes: renaming or regrouping an instance moves neither row count.
	connect(this, &QAbstractItemModel::rowsInserted, this,
			&InstanceFilterModel::refreshDerived);
	connect(this, &QAbstractItemModel::rowsRemoved, this,
			&InstanceFilterModel::refreshDerived);
	connect(this, &QAbstractItemModel::modelReset, this,
			&InstanceFilterModel::refreshDerived);
	connect(this, &QAbstractItemModel::layoutChanged, this,
			&InstanceFilterModel::refreshDerived);
	connect(this, &QAbstractItemModel::dataChanged, this,
			&InstanceFilterModel::refreshDerived);
}

QString InstanceFilterModel::filterText() const
{
	return m_filterText;
}

void InstanceFilterModel::setFilterText(const QString& text)
{
	if (m_filterText == text) {
		return;
	}
	m_filterText = text;
	Q_EMIT filterTextChanged();
	invalidateFilter();
}

QString InstanceFilterModel::group() const
{
	return m_group;
}

void InstanceFilterModel::setGroup(const QString& group)
{
	if (m_group == group) {
		return;
	}
	m_group = group;
	Q_EMIT groupChanged();
	invalidateFilter();
}

int InstanceFilterModel::count() const
{
	return rowCount();
}

bool InstanceFilterModel::exactGroup() const
{
	return m_exactGroup;
}

void InstanceFilterModel::setExactGroup(bool exact)
{
	if (m_exactGroup == exact) {
		return;
	}
	m_exactGroup = exact;
	Q_EMIT exactGroupChanged();
	invalidateFilter();
}

QString InstanceFilterModel::instanceId() const
{
	return m_instanceId;
}

void InstanceFilterModel::setInstanceId(const QString& id)
{
	if (m_instanceId == id) {
		return;
	}
	m_instanceId = id;
	Q_EMIT instanceIdChanged();
	invalidateFilter();
}

bool InstanceFilterModel::recentFirst() const
{
	return m_recentFirst;
}

void InstanceFilterModel::setRecentFirst(bool recentFirst)
{
	if (m_recentFirst == recentFirst) {
		return;
	}
	m_recentFirst = recentFirst;
	Q_EMIT recentFirstChanged();
	// Both the ordering and the "never launched" filter depend on it.
	invalidate();
}

QStringList InstanceFilterModel::groups() const
{
	return m_groups;
}

QString InstanceFilterModel::firstId() const
{
	return m_firstId;
}

void InstanceFilterModel::refreshDerived()
{
	QStringList groups;
	const int rows = rowCount();
	for (int row = 0; row < rows; ++row) {
		const QString group = index(row, 0).data(m_groupRole).toString();
		if (!groups.contains(group)) {
			groups.append(group);
		}
	}
	if (groups != m_groups) {
		m_groups = groups;
		Q_EMIT groupsChanged();
	}

	const QString firstId = (rows > 0 && m_idRole >= 0)
								? index(0, 0).data(m_idRole).toString()
								: QString();
	if (firstId != m_firstId) {
		m_firstId = firstId;
		Q_EMIT firstIdChanged();
	}
}

void InstanceFilterModel::setSourceModel(QAbstractItemModel* sourceModel)
{
	/* Roles first: the base class filters every source row as soon as it
	 * has the model, and a filter set before this call (recentFirst,
	 * instanceId) would otherwise judge those rows with unresolved roles
	 * and drop all of them. */
	m_nameRole = roleByName(sourceModel, "name", Qt::DisplayRole);
	m_groupRole = roleByName(sourceModel, "group", Qt::UserRole);
	m_lastLaunchRole = roleByName(sourceModel, "lastLaunch", -1);
	m_idRole = roleByName(sourceModel, "instanceId", -1);
	m_totalTimePlayedRole = roleByName(sourceModel, "totalTimePlayed", -1);
	QSortFilterProxyModel::setSourceModel(sourceModel);

	/* A QSortFilterProxyModel does not sort until something asks it to, and a
	 * caller that forgets sort(0) shows instances in discovery order. Asked
	 * for here, after the roles above are resolved, so the very first
	 * ordering already uses them rather than the defaults. */
	sort(0);
	refreshDerived();
}

bool InstanceFilterModel::filterAcceptsRow(
	int sourceRow, const QModelIndex& sourceParent) const
{
	if (!sourceModel()) {
		return false;
	}
	const QModelIndex index =
		sourceModel()->index(sourceRow, 0, sourceParent);
	if (!m_instanceId.isEmpty() &&
		(m_idRole < 0 || index.data(m_idRole).toString() != m_instanceId)) {
		return false;
	}
	if ((m_exactGroup || !m_group.isEmpty()) &&
		index.data(m_groupRole).toString() != m_group) {
		return false;
	}
	if (m_recentFirst &&
		(m_lastLaunchRole < 0 ||
		 index.data(m_lastLaunchRole).toLongLong() <= 0)) {
		return false;
	}
	if (!m_filterText.isEmpty() &&
		!index.data(m_nameRole).toString().contains(m_filterText,
													 Qt::CaseInsensitive)) {
		return false;
	}
	return true;
}

bool InstanceFilterModel::lessThan(const QModelIndex& left,
									const QModelIndex& right) const
{
	if (m_recentFirst && m_lastLaunchRole >= 0) {
		return left.data(m_lastLaunchRole).toLongLong() >
			   right.data(m_lastLaunchRole).toLongLong();
	}
	const QString leftGroup = left.data(m_groupRole).toString();
	const QString rightGroup = right.data(m_groupRole).toString();
	if (leftGroup == rightGroup) {
		return subSortLessThan(left, right);
	} else {
		// FIXME: ported as-is from the widget proxy -- real group ordering
		// happens in InstanceView::updateGeometries() on that side, not
		// here. Carrying the same gap over to both UIs beats fixing it in
		// only one of them.
		auto result = leftGroup.localeAwareCompare(rightGroup);
		if (result == 0) {
			return subSortLessThan(left, right);
		}
		return result < 0;
	}
}

QString InstanceFilterModel::sortModeSetting() const
{
	auto* context = LauncherContext::instance();
	return context ? context->settings()->get("InstSortMode").toString()
				   : QString();
}

bool InstanceFilterModel::subSortLessThan(const QModelIndex& left,
										   const QModelIndex& right) const
{
	const QString sortMode = sortModeSetting();
	if (m_lastLaunchRole >= 0 && sortMode == "LastLaunch") {
		return left.data(m_lastLaunchRole).toLongLong() >
			   right.data(m_lastLaunchRole).toLongLong();
	}
	// "Time played" is a QML-library-only addition to InstSortMode (the
	// classic widget page only ever writes "Name"/"LastLaunch"), so it
	// falls back to name sorting the same way an unresolved role does.
	if (m_totalTimePlayedRole >= 0 && sortMode == "TotalTimePlayed") {
		return left.data(m_totalTimePlayedRole).toLongLong() >
			   right.data(m_totalTimePlayedRole).toLongLong();
	}
	return naturalCompare(m_naturalSort, left.data(m_nameRole).toString(),
						  right.data(m_nameRole).toString()) < 0;
}
