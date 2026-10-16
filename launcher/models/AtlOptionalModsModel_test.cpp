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

#include "models/AtlOptionalModsModel.h"

/* The optional-mod rules AtlOptionalModListModel had, on hand-built mods:
 * defaults, groups, dependencies, hidden libraries. */
class AtlOptionalModsModelTest : public QObject
{
	Q_OBJECT

  private:
	static ATLauncher::VersionMod mod(const QString& name, bool selected,
									  const QString& group = QString(),
									  const QVector<QString>& depends = {},
									  bool hidden = false,
									  bool recommended = false)
	{
		ATLauncher::VersionMod m;
		m.name = name;
		m.optional = true;
		m.selected = selected;
		m.recommended = recommended;
		m.hidden = hidden;
		m.library = false;
		m.effectively_hidden = hidden;
		m.group = group;
		m.depends = depends;
		m.client = true;
		return m;
	}

  private slots:
	void startsFromThePackSelection()
	{
		AtlOptionalModsModel model({mod("A", true), mod("B", false)});
		QCOMPARE(model.selectedNames(), QVector<QString>{"A"});
		QCOMPARE(model.selectedCount(), 1);
		QCOMPARE(model.count(), 2);
	}

	void oneOfAGroup()
	{
		AtlOptionalModsModel model(
			{mod("Low", true, "shaders"), mod("High", false, "shaders")});
		QSignalSpy changed(&model, &AtlOptionalModsModel::selectionChanged);
		model.toggle(1);
		QCOMPARE(model.selectedNames(), QVector<QString>{"High"});
		QCOMPARE(changed.count(), 1);
	}

	void dependenciesComeAlongAndHiddenOnesGo()
	{
		AtlOptionalModsModel model({mod("Map", false, QString(), {"Lib"}),
									mod("Lib", false, QString(), {}, true)});
		model.toggle(0);
		QCOMPARE(model.selectedNames(), (QVector<QString>{"Map", "Lib"}));
		QVERIFY(model.index(1).data(AtlOptionalModsModel::HiddenRole).toBool());

		/* Nothing needs the hidden library any more. */
		model.toggle(0);
		QVERIFY(model.selectedNames().isEmpty());
	}

	void unTickingADependencyUnticksItsDependants()
	{
		AtlOptionalModsModel model({mod("Addon", false, QString(), {"Core"}),
									mod("Core", false)});
		model.setSelected(0, true);
		QCOMPARE(model.selectedCount(), 2);
		model.setSelected(1, false);
		QVERIFY(model.selectedNames().isEmpty());
	}

	void recommendedAndNone()
	{
		AtlOptionalModsModel model({mod("A", false, QString(), {}, false, true),
									mod("B", true)});
		model.selectRecommended();
		QCOMPARE(model.selectedNames(), QVector<QString>{"A"});
		model.clearAll();
		QCOMPARE(model.selectedCount(), 0);
	}

	void unknownDependencyAndBadRowsAreIgnored()
	{
		AtlOptionalModsModel model({mod("A", false, QString(), {"Missing"})});
		model.toggle(0);
		QCOMPARE(model.selectedNames(), QVector<QString>{"A"});
		model.toggle(5);
		model.setSelected(-1, true);
		QCOMPARE(model.selectedCount(), 1);
	}
};

QTEST_GUILESS_MAIN(AtlOptionalModsModelTest)

#include "AtlOptionalModsModel_test.moc"
