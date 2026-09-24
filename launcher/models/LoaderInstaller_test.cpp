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

#include <QTest>

#include "models/LoaderInstaller.h"

/* installSequence() is public and static, and touches neither a
 * PackProfile nor a MinecraftInstance - see its own comment and
 * install()'s. Constructing a real PackProfile (which needs a
 * MinecraftInstance) to exercise install() itself is impractical here,
 * the same reason ContentBrowser_test.cpp only exercises
 * ContentBrowser's static helpers - installSequence() exists so the one
 * thing actually worth testing (the order install() performs its side
 * effects in) can be, without that fixture. */
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

	/* The bug this review caught: EnableSelected has to happen before
	 * ChangeVersion (which triggers PackProfile::resolve()), whatever
	 * else is going on - see the class comment. */
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
