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

#include <QTemporaryDir>
#include <QTest>

#include "NullInstance.h"
#include "minecraft/legacy/LegacyInstance.h"
#include "minecraft/legacy/LegacyUpgradeTask.h"
#include "models/LegacyUpgrade.h"
#include "settings/INISettingsObject.h"

namespace
{
	/* BaseInstance's constructor registers overrides against these global
	 * setting ids; they must exist on the global SettingsObject, which must
	 * outlive the instance (same fixture shape as InstanceDetails_test). */
	SettingsObjectPtr makeGlobalSettings(QTemporaryDir& dir)
	{
		auto settings =
			std::make_shared<INISettingsObject>(dir.filePath("global.ini"));
		settings->registerSetting("PreLaunchCommand", "");
		settings->registerSetting("WrapperCommand", "");
		settings->registerSetting("PostExitCommand", "");
		settings->registerSetting("ShowConsole", true);
		settings->registerSetting("AutoCloseConsole", false);
		settings->registerSetting("ShowConsoleOnError", true);
		settings->registerSetting("LogPrePostOutput", true);
		settings->registerSetting("ConsoleMaxLines", 100000);
		settings->registerSetting("ConsoleOverflowStop", true);
		return settings;
	}

	struct Fixture {
		QTemporaryDir globalDir;
		QTemporaryDir legacyDir;
		QTemporaryDir plainDir;
		SettingsObjectPtr global = makeGlobalSettings(globalDir);
		InstancePtr legacy = std::make_shared<LegacyInstance>(
			global,
			std::make_shared<INISettingsObject>(
				legacyDir.filePath("instance.cfg")),
			legacyDir.path());
		InstancePtr plain = std::make_shared<NullInstance>(
			global,
			std::make_shared<INISettingsObject>(
				plainDir.filePath("instance.cfg")),
			plainDir.path());
	};
} // namespace

class LegacyUpgradeTest : public QObject
{
	Q_OBJECT
  private slots:

	void recognisesOnlyLegacyInstances()
	{
		Fixture f;
		QVERIFY(LegacyUpgrade::isLegacy(f.legacy));
		QVERIFY(!LegacyUpgrade::isLegacy(f.plain));
		QVERIFY(!LegacyUpgrade::isLegacy(nullptr));
	}

	void upgradedNameMarksTheCopy()
	{
		QCOMPARE(LegacyUpgrade::upgradedName("Old Pack"),
				 QString("Old Pack (Migrated)"));
	}

	void makeTaskNamesTheCopyAfterTheOriginal()
	{
		Fixture f;
		f.legacy->setName("Old Pack");
		f.legacy->setIconKey("flame");
		std::unique_ptr<LegacyUpgradeTask> task(
			LegacyUpgrade::makeTask(f.legacy, "Retro"));
		QVERIFY(task);
		QCOMPARE(task->name(), QString("Old Pack (Migrated)"));
		QCOMPARE(task->group(), QString("Retro"));
	}

	// Error path: nothing to upgrade, so no task and no crash.
	void makeTaskRefusesNonLegacyAndNull()
	{
		Fixture f;
		QVERIFY(!LegacyUpgrade::makeTask(f.plain, QString()));
		QVERIFY(!LegacyUpgrade::makeTask(nullptr, QString()));
	}
};

QTEST_GUILESS_MAIN(LegacyUpgradeTest)

#include "LegacyUpgrade_test.moc"
