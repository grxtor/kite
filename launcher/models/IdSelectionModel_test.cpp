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

#include <QSignalSpy>
#include <QTest>

#include "models/IdSelectionModel.h"

class IdSelectionModelTest : public QObject
{
	Q_OBJECT

  private slots:
	void test_select_addsIdAndBecomesCurrent()
	{
		IdSelectionModel selection;
		QSignalSpy changed(&selection, &IdSelectionModel::changed);
		QSignalSpy countChanged(&selection, &IdSelectionModel::countChanged);
		QSignalSpy idsChanged(&selection, &IdSelectionModel::selectedIdsChanged);
		QSignalSpy currentChanged(&selection, &IdSelectionModel::currentIdChanged);

		selection.select("a");

		QVERIFY(selection.isSelected("a"));
		QCOMPARE(selection.count(), 1);
		QCOMPARE(selection.selectedIds(), QStringList{ "a" });
		QCOMPARE(selection.currentId(), QString("a"));
		QCOMPARE(changed.count(), 1);
		QCOMPARE(countChanged.count(), 1);
		QCOMPARE(idsChanged.count(), 1);
		QCOMPARE(currentChanged.count(), 1);
	}

	void test_select_alreadySelected_emitsNothing()
	{
		IdSelectionModel selection;
		selection.select("a");

		QSignalSpy changed(&selection, &IdSelectionModel::changed);
		selection.select("a");

		QCOMPARE(changed.count(), 0);
		QCOMPARE(selection.count(), 1);
	}

	void test_select_emptyId_isIgnored()
	{
		IdSelectionModel selection;
		QSignalSpy changed(&selection, &IdSelectionModel::changed);

		selection.select(QString());

		QCOMPARE(changed.count(), 0);
		QCOMPARE(selection.count(), 0);
	}

	void test_deselect_removesId()
	{
		IdSelectionModel selection;
		selection.select("a");
		selection.select("b");

		QSignalSpy changed(&selection, &IdSelectionModel::changed);
		selection.deselect("a");

		QVERIFY(!selection.isSelected("a"));
		QVERIFY(selection.isSelected("b"));
		QCOMPARE(selection.count(), 1);
		QCOMPARE(changed.count(), 1);
	}

	void test_deselect_notSelected_emitsNothing()
	{
		IdSelectionModel selection;
		selection.select("a");

		QSignalSpy changed(&selection, &IdSelectionModel::changed);
		selection.deselect("z");

		QCOMPARE(changed.count(), 0);
	}

	/// Deselecting the current id clears currentId rather than leaving it
	/// pointing at an id that is no longer selected.
	void test_deselect_currentId_clearsCurrentId()
	{
		IdSelectionModel selection;
		selection.select("a");
		selection.select("b");
		QCOMPARE(selection.currentId(), QString("b"));

		selection.deselect("b");

		QCOMPARE(selection.currentId(), QString());
	}

	/// Deselecting an id that is not the current one leaves currentId alone.
	void test_deselect_otherId_leavesCurrentIdAlone()
	{
		IdSelectionModel selection;
		selection.select("a");
		selection.select("b");

		selection.deselect("a");

		QCOMPARE(selection.currentId(), QString("b"));
	}

	void test_toggle_flipsSelection()
	{
		IdSelectionModel selection;
		selection.toggle("a");
		QVERIFY(selection.isSelected("a"));

		selection.toggle("a");
		QVERIFY(!selection.isSelected("a"));
		QCOMPARE(selection.count(), 0);
	}

	void test_clear_removesEverything()
	{
		IdSelectionModel selection;
		selection.select("a");
		selection.select("b");

		QSignalSpy changed(&selection, &IdSelectionModel::changed);
		selection.clear();

		QCOMPARE(selection.count(), 0);
		QCOMPARE(selection.currentId(), QString());
		QCOMPARE(changed.count(), 1);
	}

	void test_clear_alreadyEmpty_emitsNothing()
	{
		IdSelectionModel selection;

		QSignalSpy changed(&selection, &IdSelectionModel::changed);
		selection.clear();

		QCOMPARE(changed.count(), 0);
	}

	void test_selectOnly_replacesWholeSelection()
	{
		IdSelectionModel selection;
		selection.select("a");
		selection.select("b");

		selection.selectOnly("c");

		QCOMPARE(selection.count(), 1);
		QVERIFY(selection.isSelected("c"));
		QVERIFY(!selection.isSelected("a"));
		QVERIFY(!selection.isSelected("b"));
		QCOMPARE(selection.currentId(), QString("c"));
	}

	void test_selectOnly_sameSingleSelection_emitsNothing()
	{
		IdSelectionModel selection;
		selection.selectOnly("a");

		QSignalSpy changed(&selection, &IdSelectionModel::changed);
		selection.selectOnly("a");

		QCOMPARE(changed.count(), 0);
	}

	/// An empty id clears the selection instead of trying to select nothing.
	void test_selectOnly_emptyId_clearsSelection()
	{
		IdSelectionModel selection;
		selection.select("a");

		selection.selectOnly(QString());

		QCOMPARE(selection.count(), 0);
		QCOMPARE(selection.currentId(), QString());
	}

	void test_isSelected_falseForUnknownId()
	{
		IdSelectionModel selection;
		QVERIFY(!selection.isSelected("nope"));
	}
};

QTEST_GUILESS_MAIN(IdSelectionModelTest)

#include "IdSelectionModel_test.moc"
