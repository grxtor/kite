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

#include "models/ManagedPackController.h"

/* Exercises only providerFromString() - the one static, instance-free
 * helper this class has. isSupported()/fetchVersions()/updateToVersion()
 * all need a real BaseInstance (and, for the CurseForge branch, a
 * LauncherContext to read the API key setting from), which is impractical
 * to construct here - the same reason models/ContentBrowser_test.cpp only
 * exercises ContentBrowser's own static helpers. */
class ManagedPackControllerTest : public QObject
{
	Q_OBJECT

  private slots:
	void providerFromString();
};

void ManagedPackControllerTest::providerFromString()
{
	using Provider = ManagedPackController::Provider;

	QCOMPARE(ManagedPackController::providerFromString("modrinth"),
			Provider::Modrinth);
	QCOMPARE(ManagedPackController::providerFromString("MODRINTH"),
			Provider::Modrinth);
	QCOMPARE(ManagedPackController::providerFromString("  modrinth  "),
			Provider::Modrinth);
	QCOMPARE(ManagedPackController::providerFromString("curseforge"),
			Provider::CurseForge);
	// "flame" is what the upstream launchers call CurseForge.
	QCOMPARE(ManagedPackController::providerFromString("flame"),
			Provider::CurseForge);
	QCOMPARE(ManagedPackController::providerFromString("Flame"),
			Provider::CurseForge);
	QCOMPARE(ManagedPackController::providerFromString(""),
			Provider::Unknown);
	QCOMPARE(ManagedPackController::providerFromString("technic"),
			Provider::Unknown);
}

QTEST_GUILESS_MAIN(ManagedPackControllerTest)
#include "ManagedPackController_test.moc"
