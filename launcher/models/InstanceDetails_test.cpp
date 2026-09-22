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
#include <QTemporaryDir>
#include <QTest>

#include "models/InstanceDetails.h"

#include "NullInstance.h"
#include "launch/LaunchTask.h"
#include "launch/LogModel.h"
#include "settings/INISettingsObject.h"

/* Only InstanceLogBridge is exercised here - constructing a full
 * MinecraftInstance (needed for InstanceDetails' mods/worlds/components)
 * is impractical in a unit test. InstanceLogBridge only needs a
 * BaseInstance and a LaunchTask, both of which are usable directly:
 * NullInstance is a ready-made minimal concrete BaseInstance, and
 * LaunchTask itself (not an OS-specific subclass) is what
 * LaunchTask::create() hands back. */
namespace
{
	/* BaseInstance's constructor registers overrides/passthroughs against
	 * these global setting ids; every one of them has to already be
	 * registered on whatever SettingsObject is passed in as
	 * globalSettings, or the registration calls hand it a null Setting. */
	SettingsObjectPtr makeGlobalSettings(QTemporaryDir& dir)
	{
		auto settings = std::make_shared<INISettingsObject>(
			dir.filePath("global.ini"));
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

	/* Every Setting a SettingsObject registers keeps a raw SettingsObject*
	 * back-pointer to it (Setting::m_storage), including ones an instance
	 * only holds a passthrough/override *over* (PassthroughSetting::m_other
	 * etc.) - so the global SettingsObject has to stay alive for as long as
	 * the instance does, not just for the call that constructs it. Bundling
	 * them in one fixture, in this declaration order, makes that automatic. */
	struct InstanceFixture {
		QTemporaryDir globalDir;
		QTemporaryDir instDir;
		SettingsObjectPtr global = makeGlobalSettings(globalDir);
		InstancePtr instance = std::make_shared<NullInstance>(
			global,
			std::make_shared<INISettingsObject>(
				instDir.filePath("instance.cfg")),
			instDir.path());
	};
} // namespace

class InstanceLogBridgeTest : public QObject
{
	Q_OBJECT

  private slots:
	void test_NoLaunchTask_ModelIsNullAndHasLogIsFalse()
	{
		InstanceFixture fixture;
		InstanceLogBridge bridge(fixture.instance);

		QVERIFY(bridge.model() == nullptr);
		QVERIFY(!bridge.hasLog());
		QCOMPARE(bridge.text(), QString());
	}

	void test_LaunchTaskChanged_SwitchesToItsLogModel()
	{
		InstanceFixture fixture;
		InstanceLogBridge bridge(fixture.instance);
		QSignalSpy modelSpy(&bridge, &InstanceLogBridge::modelChanged);

		auto task = LaunchTask::create(fixture.instance);
		emit fixture.instance->launchTaskChanged(task);

		QCOMPARE(modelSpy.count(), 1);
		QVERIFY(bridge.model() != nullptr);
		QVERIFY(bridge.hasLog());
		QCOMPARE(bridge.model(), task->getLogModel().get());
	}

	void test_ClearAndTextPassThroughToTheLogModel()
	{
		InstanceFixture fixture;
		InstanceLogBridge bridge(fixture.instance);
		auto task = LaunchTask::create(fixture.instance);
		emit fixture.instance->launchTaskChanged(task);

		task->getLogModel()->append(MessageLevel::Info, "hello");
		QCOMPARE(bridge.text(), QString("hello\n"));

		bridge.clear();
		QCOMPARE(bridge.text(), QString());
	}

	void test_SetSuspended_StopsTheModelAcceptingLines()
	{
		InstanceFixture fixture;
		InstanceLogBridge bridge(fixture.instance);
		auto task = LaunchTask::create(fixture.instance);
		emit fixture.instance->launchTaskChanged(task);

		bridge.setSuspended(true);
		task->getLogModel()->append(MessageLevel::Info, "should be dropped");

		QCOMPARE(bridge.text(), QString());
	}

	void test_LaunchTaskReplaced_SwitchesAgain()
	{
		InstanceFixture fixture;
		InstanceLogBridge bridge(fixture.instance);

		auto task1 = LaunchTask::create(fixture.instance);
		emit fixture.instance->launchTaskChanged(task1);
		auto* model1 = bridge.model();

		auto task2 = LaunchTask::create(fixture.instance);
		emit fixture.instance->launchTaskChanged(task2);

		QVERIFY(bridge.model() != model1);
		QCOMPARE(bridge.model(), task2->getLogModel().get());
	}

	void test_LaunchTaskClearedToNull_ModelGoesBackToNull()
	{
		InstanceFixture fixture;
		InstanceLogBridge bridge(fixture.instance);
		auto task = LaunchTask::create(fixture.instance);
		emit fixture.instance->launchTaskChanged(task);
		QVERIFY(bridge.hasLog());

		emit fixture.instance->launchTaskChanged(shared_qobject_ptr<LaunchTask>());

		QVERIFY(bridge.model() == nullptr);
		QVERIFY(!bridge.hasLog());
	}
};

QTEST_GUILESS_MAIN(InstanceLogBridgeTest)

#include "InstanceDetails_test.moc"
