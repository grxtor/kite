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

#include <QAbstractListModel>
#include <QSignalSpy>
#include <QTest>

#include "models/InstanceFilterModel.h"

namespace
{

/*
 * Stand-in for InstanceList: exposes the same role NAMES (name, group,
 * instanceId, lastLaunch) but deliberately different role NUMBERS. A test
 * that passes against this only passes because InstanceFilterModel resolves
 * roles by name, not because it happens to reuse InstanceList's own numbers.
 */
class FakeInstanceModel : public QAbstractListModel
{
  public:
	struct Row {
		QString id;
		QString name;
		QString group;
		qint64 lastLaunch = 0;
	};

	enum Roles { IdRole = Qt::UserRole + 1, NameRole, GroupRole, LastLaunchRole };

	explicit FakeInstanceModel(QList<Row> rows, QObject* parent = nullptr)
		: QAbstractListModel(parent), m_rows(std::move(rows))
	{
	}

	int rowCount(const QModelIndex& parent = QModelIndex()) const override
	{
		return parent.isValid() ? 0 : m_rows.count();
	}

	QVariant data(const QModelIndex& index, int role) const override
	{
		if (!index.isValid() || index.row() >= m_rows.count()) {
			return QVariant();
		}
		const Row& row = m_rows.at(index.row());
		switch (role) {
			case IdRole:
				return row.id;
			case NameRole:
				return row.name;
			case GroupRole:
				return row.group;
			case LastLaunchRole:
				return row.lastLaunch;
			default:
				return QVariant();
		}
	}

	QHash<int, QByteArray> roleNames() const override
	{
		return {
			{ IdRole, "instanceId" },
			{ NameRole, "name" },
			{ GroupRole, "group" },
			{ LastLaunchRole, "lastLaunch" },
		};
	}

  private:
	QList<Row> m_rows;
};

} // namespace

class InstanceFilterModelTest : public QObject
{
	Q_OBJECT

  private slots:
	/// "Pack 2" has to sort before "Pack 10" -- a plain string compare would
	/// put "Pack 10" first, which is the exact bug QCollator numeric mode
	/// exists to avoid.
	void test_naturalSort_ordersNumericSuffixesNumerically()
	{
		FakeInstanceModel source({
			{ "b", "Pack 10", "", 0 },
			{ "a", "Pack 2", "", 0 },
		});
		InstanceFilterModel filter;
		filter.setSourceModel(&source);
		filter.sort(0);

		QCOMPARE(filter.index(0, 0).data(FakeInstanceModel::NameRole).toString(),
				 QString("Pack 2"));
		QCOMPARE(filter.index(1, 0).data(FakeInstanceModel::NameRole).toString(),
				 QString("Pack 10"));
	}

	/// Same-group rows fall through to natural sort; different-group rows
	/// sort by group first, mirroring the widget proxy's lessThan().
	void test_lessThan_groupsSortBeforeName()
	{
		FakeInstanceModel source({
			{ "a", "Alpha", "Zeta Group", 0 },
			{ "b", "Zulu", "Alpha Group", 0 },
		});
		InstanceFilterModel filter;
		filter.setSourceModel(&source);
		filter.sort(0);

		// "Alpha Group" sorts before "Zeta Group", so "Zulu" (in Alpha
		// Group) comes first even though "Alpha" < "Zulu" by name.
		QCOMPARE(filter.index(0, 0).data(FakeInstanceModel::NameRole).toString(),
				 QString("Zulu"));
		QCOMPARE(filter.index(1, 0).data(FakeInstanceModel::NameRole).toString(),
				 QString("Alpha"));
	}

	void test_filterText_matchesNameCaseInsensitively_andUpdatesCount()
	{
		FakeInstanceModel source({
			{ "a", "Pack Alpha", "", 0 },
			{ "b", "Pack Beta", "", 0 },
			{ "c", "Something Else", "", 0 },
		});
		InstanceFilterModel filter;
		filter.setSourceModel(&source);
		filter.sort(0);
		QCOMPARE(filter.count(), 3);

		QSignalSpy countSpy(&filter, &InstanceFilterModel::countChanged);
		filter.setFilterText("PACK");
		QVERIFY(!countSpy.isEmpty());
		QCOMPARE(filter.count(), 2);

		filter.setFilterText("pack alpha");
		QCOMPARE(filter.count(), 1);
		QCOMPARE(
			filter.index(0, 0).data(FakeInstanceModel::NameRole).toString(),
			QString("Pack Alpha"));

		filter.setFilterText(QString());
		QCOMPARE(filter.count(), 3);
	}

	/// Empty group means "no filter"; a non-empty one keeps only that group.
	void test_group_emptyMeansAllGroups_otherwiseOnlyThatGroup()
	{
		FakeInstanceModel source({
			{ "a", "One", "Modpacks", 0 },
			{ "b", "Two", "Vanilla", 0 },
			{ "c", "Three", "Modpacks", 0 },
		});
		InstanceFilterModel filter;
		filter.setSourceModel(&source);
		filter.sort(0);
		QCOMPARE(filter.count(), 3);

		filter.setGroup("Modpacks");
		QCOMPARE(filter.count(), 2);
		for (int i = 0; i < filter.rowCount(); ++i) {
			QCOMPARE(
				filter.index(i, 0).data(FakeInstanceModel::GroupRole).toString(),
				QString("Modpacks"));
		}

		filter.setGroup(QString());
		QCOMPARE(filter.count(), 3);
	}

	/// QML delegates reach instanceId/name/group/lastLaunch through the
	/// proxy by name, so the proxy's roleNames() has to be the source
	/// model's, not QSortFilterProxyModel's own default.
	void test_roleNames_areForwardedFromSourceModel()
	{
		FakeInstanceModel source({});
		InstanceFilterModel filter;
		filter.setSourceModel(&source);

		QCOMPARE(filter.roleNames(), source.roleNames());
	}
};

QTEST_GUILESS_MAIN(InstanceFilterModelTest)

#include "InstanceFilterModel_test.moc"
