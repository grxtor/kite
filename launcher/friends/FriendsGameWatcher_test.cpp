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
	/* The profile Friends is signed in as, and another account's. */
	const QString kMe = QStringLiteral("11111111-2222-3333-4444-555555555555");
	const QString kOther = QStringLiteral("99999999-8888-7777-6666-555555555555");

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

		/// Starts the game of @p id, played as @p profile ("" for a launch
		/// that is not an account's own: offline, a demo), and returns its
		/// launch task.
		shared_qobject_ptr<LaunchTask> start(const QString& id,
											 const QString& profile = kMe)
		{
			auto inst = list->getInstanceById(id);
			auto task = LaunchTask::create(inst);
			task->setPlayerProfile(profile);
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
		QVERIFY(!watcher.presence(kMe).playing);
	}

	void aRunningGameIsPlayingWithItsName()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		QSignalSpy spy(&watcher, &FriendsGameWatcher::changed);
		auto task = f.start("one");
		QVERIFY(watcher.presence(kMe).playing);
		QCOMPARE(watcher.presence(kMe).instance, QString("Survival"));
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::None);
		QVERIFY(spy.count() >= 1);
	}

	void theLogMovesThePlayerAround()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one");
		task->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						"mc.hypixel.net, 25565");
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::Server);
		QCOMPARE(watcher.presence(kMe).world.host, QString("mc.hypixel.net"));

		task->onLogLine("[12:00:30] [Server thread/INFO]: Starting integrated "
						"minecraft server version 1.21.4");
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::Singleplayer);

		task->onLogLine("[12:01:00] [Server thread/INFO]: Stopping server");
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::None);
		QVERIFY(watcher.presence(kMe).playing);
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

	void aReportedWorldOverridesTheLog()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one");
		auto* game = f.list->getInstanceById("one").get();
		task->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						"mc.hypixel.net, 25565");
		QCOMPARE(watcher.presence(kMe).world.host, QString("mc.hypixel.net"));

		QSignalSpy spy(&watcher, &FriendsGameWatcher::changed);
		watcher.setReportedWorld(game, {World::Type::Server, "play.example.net", 25566});
		QCOMPARE(spy.count(), 1);
		QCOMPARE(watcher.presence(kMe).world.host, QString("play.example.net"));
		QCOMPARE(watcher.presence(kMe).world.port, 25566);

		/* The log says something else, and is not believed any more. */
		task->onLogLine("[12:00:30] [Render thread/INFO]: Connecting to "
						"other.example.net, 25565");
		task->onLogLine("[12:00:31] [Server thread/INFO]: Starting integrated "
						"minecraft server version 1.21.4");
		QCOMPARE(watcher.presence(kMe).world.host, QString("play.example.net"));
		QCOMPARE(spy.count(), 1);
	}

	void aReportedWorldIsAnnouncedOnlyWhenItChanges()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one");
		auto* game = f.list->getInstanceById("one").get();
		QSignalSpy spy(&watcher, &FriendsGameWatcher::changed);
		const World server{World::Type::Server, "play.example.net", 25565};
		watcher.setReportedWorld(game, server);
		watcher.setReportedWorld(game, server);
		QCOMPARE(spy.count(), 1);
		watcher.setReportedWorld(game, {World::Type::Singleplayer, {}, 0});
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::Singleplayer);
		QCOMPARE(spy.count(), 2);
	}

	void aReportOfNoneClearsTheServer()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one");
		auto* game = f.list->getInstanceById("one").get();
		task->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						"mc.hypixel.net, 25565");
		watcher.setReportedWorld(game, {});
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::None);
		QVERIFY(watcher.presence(kMe).world.host.isEmpty());
		QVERIFY(watcher.presence(kMe).playing);
	}

	void aReportDoesNotSurviveTheGame()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one");
		auto* game = f.list->getInstanceById("one").get();
		watcher.setReportedWorld(game, {World::Type::Server, "play.example.net", 25565});
		game->setRunning(false);
		QVERIFY(!watcher.presence(kMe).playing);

		/* The next game starts in the menus, and its log counts again. */
		auto again = f.start("one");
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::None);
		again->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						 "mc.hypixel.net, 25565");
		QCOMPARE(watcher.presence(kMe).world.host, QString("mc.hypixel.net"));
	}

	void aReportFromAGameThatIsNotRunningIsIgnored()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto* game = f.list->getInstanceById("one").get();
		QSignalSpy spy(&watcher, &FriendsGameWatcher::changed);
		watcher.setReportedWorld(game, {World::Type::Server, "play.example.net", 25565});
		QCOMPARE(spy.count(), 0);
		/* ... and it does not leave the next game's log unheard. */
		auto task = f.start("one");
		task->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						"mc.hypixel.net, 25565");
		QCOMPARE(watcher.presence(kMe).world.host, QString("mc.hypixel.net"));
	}

	void aReportIsPerGame()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto first = f.start("one");
		auto second = f.start("two");
		auto* firstGame = f.list->getInstanceById("one").get();
		watcher.setReportedWorld(firstGame, {World::Type::Server, "one.example.net", 25565});
		/* The latest game is the second, whose log still counts. */
		second->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						  "two.example.net, 25565");
		QCOMPARE(watcher.presence(kMe).instance, QString("Creative"));
		QCOMPARE(watcher.presence(kMe).world.host, QString("two.example.net"));
		f.list->getInstanceById("two")->setRunning(false);
		QCOMPARE(watcher.presence(kMe).world.host, QString("one.example.net"));
	}

	void theGameEndingClearsEverything()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one");
		task->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						"mc.hypixel.net, 25565");
		f.list->getInstanceById("one")->setRunning(false);
		QVERIFY(!watcher.presence(kMe).playing);
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::None);

		/* The next launch starts in the menus. */
		auto again = f.start("one");
		QVERIFY(watcher.presence(kMe).playing);
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::None);
	}

	void theMostRecentlyStartedGameCounts()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto first = f.start("one");
		auto second = f.start("two");
		QCOMPARE(watcher.presence(kMe).instance, QString("Creative"));
		f.list->getInstanceById("two")->setRunning(false);
		QCOMPARE(watcher.presence(kMe).instance, QString("Survival"));
	}

	void aGameOfAnotherAccountIsNotReported()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one", kOther);
		task->onLogLine("[12:00:00] [Render thread/INFO]: Connecting to "
						"play.example.net, 25565");
		// Friends is signed in as kMe: this is someone else's game.
		QVERIFY(!watcher.presence(kMe).playing);
		QVERIFY(watcher.presence(kMe).instance.isEmpty());
		QCOMPARE(watcher.presence(kMe).world.type, World::Type::None);
		// It is the game of kOther, if that is who asks.
		QVERIFY(watcher.presence(kOther).playing);
		QCOMPARE(watcher.presence(kOther).world.host,
				 QString("play.example.net"));
	}

	void anOfflineOrDemoGameIsNotReported()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		// No profile: offline, a demo, or a launch that does not say.
		auto task = f.start("one", QString());
		QVERIFY(!watcher.presence(kMe).playing);
		QVERIFY(!watcher.presence(QString()).playing);
	}

	void theProfileMatchesWhateverTheDashes()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto task = f.start("one", "11111111222233334444555555555555");
		QVERIFY(watcher.presence(kMe).playing);
		QVERIFY(watcher.presence("11111111222233334444555555555555").playing);
		QVERIFY(!watcher.presence(QString()).playing);
	}

	void onlyTheLatestGameCountsWhoeverPlaysIt()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		auto mine = f.start("one", kMe);
		auto alt = f.start("two", kOther);
		// The newest is an alt's: nothing is told, though an older game of
		// the signed-in account runs.
		QVERIFY(!watcher.presence(kMe).playing);
		f.list->getInstanceById("two")->setRunning(false);
		QVERIFY(watcher.presence(kMe).playing);
		QCOMPARE(watcher.presence(kMe).instance, QString("Survival"));
	}

	void aGameCountsOnceItsLaunchIsKnown()
	{
		Fixture f;
		FriendsGameWatcher watcher(f.list);
		QSignalSpy spy(&watcher, &FriendsGameWatcher::changed);
		auto inst = f.list->getInstanceById("one");
		// create() sets the instance running before anyone says who plays.
		auto task = LaunchTask::create(inst);
		QCOMPARE(spy.count(), 0);
		QVERIFY(!watcher.presence(kMe).playing);
		task->setPlayerProfile(kMe);
		emit inst->launchTaskChanged(task);
		QCOMPARE(spy.count(), 1);
		QVERIFY(watcher.presence(kMe).playing);
		// Another account's launch of the same kind is announced too (the
		// latest game changes who it is played by).
		inst->setRunning(false);
		QCOMPARE(spy.count(), 2);
		auto again = LaunchTask::create(inst);
		again->setPlayerProfile(kOther);
		emit inst->launchTaskChanged(again);
		QCOMPARE(spy.count(), 3);
		QVERIFY(!watcher.presence(kMe).playing);
	}

	void aGameAlreadyRunningWithoutALaunchIsNotReported()
	{
		Fixture f;
		// The instance of this test does not hold its launch task (a real
		// one does, and says whose game it is), so nothing says who plays.
		auto task = f.start("one");
		FriendsGameWatcher watcher(f.list);
		QVERIFY(!watcher.presence(kMe).playing);
		QVERIFY(!watcher.presence(QString()).playing);
	}
};

QTEST_GUILESS_MAIN(FriendsGameWatcherTest)

#include "FriendsGameWatcher_test.moc"
