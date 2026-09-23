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

#include "qml/QmlShell.h"

/*
 * Covers sanitizedInstanceName() and the setup-step-needed rules
 * (languageSetupStepNeeded()/javaSetupStepNeeded()), the free-standing
 * helpers QmlShell::renameInstance()/duplicateInstance() and
 * recomputeSetupSteps() build on. They are free-standing rather than QmlShell
 * methods precisely so this can run without a LauncherContext, the way
 * NewInstanceController_test.cpp exercises composeSuggestedInstanceName()
 * without a NewInstanceController.
 */
class QmlShellTest : public QObject
{
	Q_OBJECT

  private slots:
	void leavesAnAlreadyCleanNameUnchanged()
	{
		QCOMPARE(sanitizedInstanceName(QStringLiteral("Vanilla 1.20.1")),
				 QStringLiteral("Vanilla 1.20.1"));
	}

	void trimsSurroundingWhitespace()
	{
		QCOMPARE(sanitizedInstanceName(QStringLiteral("  Modded Survival  ")),
				 QStringLiteral("Modded Survival"));
	}

	void collapsesEmbeddedNewlinesToSpaces()
	{
		// Same as NoReturnTextEdit's commit path (InstanceDelegate.cpp):
		// a pasted multi-line name becomes one line, not several.
		QCOMPARE(sanitizedInstanceName(QStringLiteral("Line one\nLine two")),
				 QStringLiteral("Line one Line two"));
	}

	void whollyBlankNameSanitizesToEmpty()
	{
		QVERIFY(sanitizedInstanceName(QStringLiteral("   \n  ")).isEmpty());
	}

	void emptyNameStaysEmpty()
	{
		QVERIFY(sanitizedInstanceName(QString()).isEmpty());
	}

	void languageStepNeededOnlyWhenLanguageIsEmpty()
	{
		QVERIFY(languageSetupStepNeeded(QString()));
		QVERIFY(!languageSetupStepNeeded(QStringLiteral("en_US")));
	}

	void javaStepNeededWhenHostnameChangedRegardlessOfJavaPath()
	{
		QVERIFY(javaSetupStepNeeded(/* hostnameChanged */ true,
									/* javaPathResolves */ true));
		QVERIFY(javaSetupStepNeeded(true, false));
	}

	void javaStepNeededWhenJavaPathDoesNotResolve()
	{
		QVERIFY(javaSetupStepNeeded(/* hostnameChanged */ false,
									/* javaPathResolves */ false));
	}

	void javaStepNotNeededWhenHostnameSameAndJavaPathResolves()
	{
		QVERIFY(!javaSetupStepNeeded(/* hostnameChanged */ false,
									 /* javaPathResolves */ true));
	}
};

QTEST_GUILESS_MAIN(QmlShellTest)

#include "QmlShell_test.moc"
