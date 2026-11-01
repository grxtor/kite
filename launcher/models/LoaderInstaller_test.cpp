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

#include <QTest>

#include "models/LoaderInstaller.h"

/* installSequence() is public and static and touches neither a PackProfile nor
 * a MinecraftInstance; it exists so the order install() performs its side
 * effects in can be tested without that fixture. */
class LoaderInstallerTest : public QObject
{
	Q_OBJECT

  private slots:

	void test_NoConflictsEnablesBeforeChangingVersion()
	{
		using Step = LoaderInstaller::InstallStep;
		const QList<Step> steps = LoaderInstaller::installSequence(0);

		QCOMPARE(steps.size(), 2);
		QVERIFY(steps.at(0) == Step::EnableSelected);
		QVERIFY(steps.at(1) == Step::ChangeVersion);
	}

	void test_ConflictsAreDisabledBeforeEnablingOrChangingVersion()
	{
		using Step = LoaderInstaller::InstallStep;
		const QList<Step> steps = LoaderInstaller::installSequence(2);

		QCOMPARE(steps.size(), 4);
		QVERIFY(steps.at(0) == Step::DisableConflict);
		QVERIFY(steps.at(1) == Step::DisableConflict);
		QVERIFY(steps.at(2) == Step::EnableSelected);
		QVERIFY(steps.at(3) == Step::ChangeVersion);
	}

	/* EnableSelected must happen before ChangeVersion (which triggers PackProfile::resolve()). */
	void test_EnableAlwaysPrecedesChangeVersion()
	{
		using Step = LoaderInstaller::InstallStep;
		for (int conflictCount = 0; conflictCount < 4; ++conflictCount) {
			const QList<Step> steps =
				LoaderInstaller::installSequence(conflictCount);
			QVERIFY(steps.indexOf(Step::EnableSelected) <
					steps.indexOf(Step::ChangeVersion));
		}
	}
};

QTEST_GUILESS_MAIN(LoaderInstallerTest)

#include "LoaderInstaller_test.moc"
