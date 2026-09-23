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
		qint64 totalTimePlayed = 0;
	};

	enum Roles { IdRole = Qt::UserRole + 1, NameRole, GroupRole, LastLaunchRole, TotalTimePlayedRole };

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
			case TotalTimePlayedRole:
				return row.totalTimePlayed;
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
			{ TotalTimePlayedRole, "totalTimePlayed" },
		};
	}

  private:
	QList<Row> m_rows;
};

/*
 * subSortLessThan() reads its sort mode off the live LauncherContext, which
 * nothing in this test binary constructs -- see sortModeSetting()'s own
 * comment. This override stands in for it so the comparator itself (name
 * sort, "LastLaunch" and "TotalTimePlayed") can be exercised directly.
 */
class SortModeInstanceFilterModel : public InstanceFilterModel
{
  public:
	QString mode;

  protected:
	QString sortModeSetting() const override { return mode; }
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

	/// Numbers compare by value whatever the collator backend supports:
	/// leading zeros, numbers longer than any integer type, and case
	/// differences in the text between them.
	void test_naturalSort_doesNotRelyOnCollatorNumericMode()
	{
		FakeInstanceModel source({
			{ "a", "pack 100000000000000000000", "", 0 },
			{ "b", "Pack 10", "", 0 },
			{ "c", "Pack 9", "", 0 },
			{ "d", "Pack 010b", "", 0 },
			{ "e", "Pack 10a", "", 0 },
			{ "f", "Pack", "", 0 },
		});
		InstanceFilterModel filter;
		filter.setSourceModel(&source);
		filter.sort(0);

		const QStringList expected = { "Pack", "Pack 9", "Pack 10",
									   "Pack 10a", "Pack 010b",
									   "pack 100000000000000000000" };
		QStringList actual;
		for (int row = 0; row < filter.rowCount(); ++row)
			actual << filter.index(row, 0)
						  .data(FakeInstanceModel::NameRole)
						  .toString();
		QCOMPARE(actual, expected);
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

	/// exactGroup turns the empty group into "ungrouped only", which is
	/// what one library section needs; groups lists what is left, in order.
	void test_exactGroup_emptyMeansUngrouped_andGroupsFollowRows()
	{
		FakeInstanceModel source({
			{ "a", "One", "Modpacks", 0 },
			{ "b", "Two", "", 0 },
			{ "c", "Three", "Vanilla", 0 },
		});
		InstanceFilterModel filter;
		filter.setSourceModel(&source);
		QCOMPARE(filter.groups(), QStringList({ "", "Modpacks", "Vanilla" }));

		QSignalSpy groupsSpy(&filter, &InstanceFilterModel::groupsChanged);
		filter.setExactGroup(true);
		QCOMPARE(filter.count(), 1);
		QCOMPARE(filter.firstId(), QString("b"));
		QCOMPARE(filter.groups(), QStringList({ "" }));
		QCOMPARE(groupsSpy.count(), 1);
	}

	/// recentFirst: newest launch first, never-launched instances left out,
	/// groups ignored for ordering.
	void test_recentFirst_ordersByLastLaunch_andDropsNeverLaunched()
	{
		FakeInstanceModel source({
			{ "a", "One", "", 100 },
			{ "b", "Two", "Vanilla", 0 },
			{ "c", "Three", "Vanilla", 300 },
			{ "d", "Four", "Modpacks", 200 },
		});
		InstanceFilterModel filter;
		filter.setSourceModel(&source);
		// Grouped order puts the ungrouped "a" first...
		QCOMPARE(filter.firstId(), QString("a"));
		QSignalSpy firstSpy(&filter, &InstanceFilterModel::firstIdChanged);
		filter.setRecentFirst(true);

		QCOMPARE(filter.count(), 3);
		QCOMPARE(filter.index(0, 0).data(FakeInstanceModel::IdRole).toString(),
				 QString("c"));
		QCOMPARE(filter.index(1, 0).data(FakeInstanceModel::IdRole).toString(),
				 QString("d"));
		QCOMPARE(filter.index(2, 0).data(FakeInstanceModel::IdRole).toString(),
				 QString("a"));
		// ...and the switch has to be announced, or a binding on firstId
		// keeps showing the old instance.
		QCOMPARE(filter.firstId(), QString("c"));
		QCOMPARE(firstSpy.count(), 1);
	}

	/// Filters set before the source arrives must already see its roles --
	/// the shell configures its recent and hero proxies exactly this way.
	void test_filtersSetBeforeSource_applyToFirstRows()
	{
		FakeInstanceModel source({
			{ "a", "One", "", 100 },
			{ "b", "Two", "", 0 },
			{ "c", "Three", "", 300 },
		});
		InstanceFilterModel recent;
		recent.setRecentFirst(true);
		recent.setSourceModel(&source);
		QCOMPARE(recent.count(), 2);
		QCOMPARE(recent.firstId(), QString("c"));

		InstanceFilterModel single;
		single.setInstanceId("b");
		single.setSourceModel(&source);
		QCOMPARE(single.count(), 1);
		QCOMPARE(single.firstId(), QString("b"));
	}

	/// instanceId narrows the proxy to one row -- a single-instance view.
	void test_instanceId_keepsOnlyThatInstance()
	{
		FakeInstanceModel source({
			{ "a", "One", "Modpacks", 0 },
			{ "b", "Two", "Vanilla", 0 },
		});
		InstanceFilterModel filter;
		filter.setSourceModel(&source);
		filter.setInstanceId("b");
		QCOMPARE(filter.count(), 1);
		QCOMPARE(filter.firstId(), QString("b"));

		filter.setInstanceId("missing");
		QCOMPARE(filter.count(), 0);
		QCOMPARE(filter.firstId(), QString());
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

	/// Within a group, "InstSortMode" == "TotalTimePlayed" (the Library
	/// toolbar's "Time played" option) orders most-played first.
	void test_subSort_totalTimePlayed_ordersMostPlayedFirst()
	{
		FakeInstanceModel source({
			{ "a", "Alpha", "", 100 },
			{ "b", "Beta", "", 0, 500 },
			{ "c", "Gamma", "", 0, 200 },
		});
		SortModeInstanceFilterModel filter;
		filter.mode = "TotalTimePlayed";
		filter.setSourceModel(&source);
		filter.sort(0);

		QCOMPARE(filter.index(0, 0).data(FakeInstanceModel::IdRole).toString(), QString("b"));
		QCOMPARE(filter.index(1, 0).data(FakeInstanceModel::IdRole).toString(), QString("c"));
		QCOMPARE(filter.index(2, 0).data(FakeInstanceModel::IdRole).toString(), QString("a"));
	}

	/// Same "InstSortMode" seam, exercising the pre-existing "LastLaunch"
	/// value: most recently played first, same as recentFirst's ordering
	/// but as the in-group tie-break rather than a top-level filter.
	void test_subSort_lastLaunch_ordersMostRecentFirst()
	{
		FakeInstanceModel source({
			{ "a", "Alpha", "", 100 },
			{ "b", "Beta", "", 300 },
			{ "c", "Gamma", "", 200 },
		});
		SortModeInstanceFilterModel filter;
		filter.mode = "LastLaunch";
		filter.setSourceModel(&source);
		filter.sort(0);

		QCOMPARE(filter.index(0, 0).data(FakeInstanceModel::IdRole).toString(), QString("b"));
		QCOMPARE(filter.index(1, 0).data(FakeInstanceModel::IdRole).toString(), QString("c"));
		QCOMPARE(filter.index(2, 0).data(FakeInstanceModel::IdRole).toString(), QString("a"));
	}

	/// An unrecognised (or empty, i.e. "Name") sort mode always falls back
	/// to natural name order, even once totalTimePlayed/lastLaunch roles
	/// exist and carry data -- "Time played" must never leak in silently.
	void test_subSort_unknownSortMode_fallsBackToNaturalNameOrder()
	{
		FakeInstanceModel source({
			{ "a", "Pack 10", "", 300, 300 },
			{ "b", "Pack 2", "", 100, 900 },
		});
		SortModeInstanceFilterModel filter;
		filter.mode = "Name";
		filter.setSourceModel(&source);
		filter.sort(0);

		QCOMPARE(filter.index(0, 0).data(FakeInstanceModel::NameRole).toString(), QString("Pack 2"));
		QCOMPARE(filter.index(1, 0).data(FakeInstanceModel::NameRole).toString(), QString("Pack 10"));
	}
};

QTEST_GUILESS_MAIN(InstanceFilterModelTest)

#include "InstanceFilterModel_test.moc"
