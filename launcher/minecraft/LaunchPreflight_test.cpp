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

#include "minecraft/LaunchProfile.h"
#include "minecraft/MinecraftLoadAndCheck.h"
#include "minecraft/auth/AuthSession.h"
#include "minecraft/auth/MinecraftAccount.h"

using Reach = QNetworkInformation::Reachability;

/*
 * Two launch fixes found by a real launch on a fresh data folder:
 *  - an offline account must still be allowed to download the game's files
 *    (AuthSession::fetch_game_files), unlike an explicit offline launch;
 *  - a failed offline metadata load must fail the launch with a real
 *    reason instead of starting java with an empty profile.
 */
class LaunchPreflightTest : public QObject
{
	Q_OBJECT
  private slots:

	void offlineAccountFetchesGameFilesWhenOnlineWasRequested()
	{
		for (Reach r : {Reach::Unknown, Reach::Online, Reach::Local, Reach::Site}) {
			QVERIFY(MinecraftAccount::shouldFetchGameFiles(true, r));
		}
	}

	void anExplicitOfflineLaunchStaysOffline()
	{
		for (Reach r : {Reach::Unknown, Reach::Online, Reach::Disconnected}) {
			QVERIFY(!MinecraftAccount::shouldFetchGameFiles(false, r));
		}
	}

	void noNetworkMeansNoDownloadAttempt()
	{
		QVERIFY(!MinecraftAccount::shouldFetchGameFiles(true, Reach::Disconnected));
	}

	void fillSessionKeepsAnOfflineAccountOfflineButAllowsTheDownload()
	{
		auto account = MinecraftAccount::createOffline("Steve");

		auto online = std::make_shared<AuthSession>();
		online->wants_online = true;
		account->fillSession(online);
		QCOMPARE(online->status, AuthSession::PlayableOffline);
		QVERIFY(!online->wants_online); // never "signed in"
		QCOMPARE(online->fetch_game_files,
				 MinecraftAccount::shouldFetchGameFiles(
					 true, MinecraftAccount::networkReachability()));

		auto offline = std::make_shared<AuthSession>();
		offline->wants_online = false; // "Play offline"
		account->fillSession(offline);
		QCOMPARE(offline->status, AuthSession::PlayableOffline);
		QVERIFY(!offline->fetch_game_files);
	}

	void aFreshProfileIsUsableAndABrokenOneIsNot()
	{
		auto fresh = std::make_shared<LaunchProfile>();
		QVERIFY(MinecraftLoadAndCheck::profileUsable(fresh));

		auto warned = std::make_shared<LaunchProfile>();
		warned->applyProblemSeverity(ProblemSeverity::Warning);
		QVERIFY(MinecraftLoadAndCheck::profileUsable(warned));

		auto broken = std::make_shared<LaunchProfile>();
		broken->applyProblemSeverity(ProblemSeverity::Error);
		QVERIFY(!MinecraftLoadAndCheck::profileUsable(broken));

		// PackProfile::getProfile() gives null when patches cannot be applied.
		QVERIFY(!MinecraftLoadAndCheck::profileUsable(nullptr));
	}

	void theFailureMessageSaysWhatToDo()
	{
		const QString plain = MinecraftLoadAndCheck::notDownloadedMessage();
		QVERIFY(plain.contains("aren't downloaded yet"));
		QVERIFY(plain.contains("Connect to the internet"));
		QVERIFY(!plain.contains("\n"));

		const QString detailed = MinecraftLoadAndCheck::notDownloadedMessage(
			"Some component metadata load tasks failed.");
		QVERIFY(detailed.startsWith(plain));
		QVERIFY(detailed.endsWith("Some component metadata load tasks failed."));
		QCOMPARE(MinecraftLoadAndCheck::notDownloadedMessage("  \n"), plain);
	}
};

QTEST_GUILESS_MAIN(LaunchPreflightTest)

#include "LaunchPreflight_test.moc"
