/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include "FriendsGameWatcher.h"
#include "InstanceList.h"
#include "NullInstance.h"
#include "launch/LaunchTask.h"
#include "settings/INISettingsObject.h"

using FriendsApi::World;

namespace
{
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

	void makeInstance(const QString& root, const QString& id,
					  const QString& name)
	{
		QVERIFY(QDir().mkpath(QDir(root).filePath(id)));
		QFile file(QDir(root).filePath(id + "/instance.cfg"));
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write("InstanceType=NullTest\nname=" + name.toUtf8() + "\n");
	}

	struct Fixture {
		QTemporaryDir globalDir;
		QTemporaryDir instDir;
		SettingsObjectPtr global = makeGlobalSettings(globalDir);
		std::shared_ptr<InstanceList> list;

		Fixture()
		{
			makeInstance(instDir.path(), "one", "Survival");
			makeInstance(instDir.path(), "two", "Creative");
			list = std::make_shared<InstanceList>(
				global, QStringList{instDir.path()});
			list->loadList();
		}

		/// Starts the game of @p id and returns its launch task.
		shared_qobject_ptr<LaunchTask> start(const QString& id)
		{
			auto inst = list->getInstanceById(id);
			auto task = LaunchTask::create(inst);
			emit inst->launchTaskChanged(task);
			return task;
		}
	};
} // namespace

class FriendsGameWatcherTest : public QObject
{
	Q_OBJECT

  private slots:
	void nothingRunningIsNotPlaying()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		QVERIFY(!watcher.presence().playing);
	}

	void aRunningGameIsPlayingWithItsName()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		QSignalSpy spy(&watcher, &FriendsGameWatcher::changed);
		auto task = f.start("one");
		QVERIFY(watcher.presence().playing);
		QCOMPARE(watcher.presence().instance, QString("Survival"));
		QCOMPARE(watcher.presence().world.type, World::Type::None);
		QVERIFY(spy.count() >= 1);
	}

	void theLogMovesThePlayerAround()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one");
		task->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						"mc.hypixel.net, 25565");
		QCOMPARE(watcher.presence().world.type, World::Type::Server);
		QCOMPARE(watcher.presence().world.host, QString("mc.hypixel.net"));

		task->onLogLine("[12:00:30] [Server thread/INFO]: Starting integrated "
						"minecraft server version 1.21.4");
		QCOMPARE(watcher.presence().world.type, World::Type::Singleplayer);

		task->onLogLine("[12:01:00] [Server thread/INFO]: Stopping server");
		QCOMPARE(watcher.presence().world.type, World::Type::None);
		QVERIFY(watcher.presence().playing);
	}

	void aChangeIsAnnouncedOnce()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one");
		QSignalSpy spy(&watcher, &FriendsGameWatcher::changed);
		task->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						"mc.hypixel.net, 25565");
		task->onLogLine("[12:00:01] [Render thread/INFO]: Connecting to "
						"mc.hypixel.net, 25565");
		task->onLogLine("[12:00:02] [Render thread/INFO]: Setting user: Steve");
		QCOMPARE(spy.count(), 1);
	}

	void theGameEndingClearsEverything()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one");
		task->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						"mc.hypixel.net, 25565");
		f.list->getInstanceById("one")->setRunning(false);
		QVERIFY(!watcher.presence().playing);
		QCOMPARE(watcher.presence().world.type, World::Type::None);

		/* The next launch starts in the menus. */
		auto again = f.start("one");
		QVERIFY(watcher.presence().playing);
		QCOMPARE(watcher.presence().world.type, World::Type::None);
	}

	void theMostRecentlyStartedGameCounts()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto first = f.start("one");
		auto second = f.start("two");
		QCOMPARE(watcher.presence().instance, QString("Creative"));
		f.list->getInstanceById("two")->setRunning(false);
		QCOMPARE(watcher.presence().instance, QString("Survival"));
	}

	void aGameAlreadyRunningIsPickedUp()
	{
		Fixture f;
		auto task = f.start("one");
		FriendsGameWatcher watcher(f.list);
		QVERIFY(watcher.presence().playing);
		QCOMPARE(watcher.presence().instance, QString("Survival"));
	}
};

QTEST_GUILESS_MAIN(FriendsGameWatcherTest)

#include "FriendsGameWatcher_test.moc"
