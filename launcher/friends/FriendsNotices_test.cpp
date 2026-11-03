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

#include <QTest>

#include "FriendsNotices.h"

using FriendsApi::FriendsSnapshot;
using FriendsNotices::Memory;
using FriendsNotices::Notice;
using State = FriendsApi::Status::State;

namespace
{
	constexpr qint64 kPollSeconds = 60;

	FriendsApi::Friend friendIn(const QString& name, State state)
	{
		FriendsApi::Friend one;
		one.uuid = name + "-uuid";
		one.name = name;
		one.status.state = state;
		return one;
	}

	FriendsApi::Person person(const QString& name)
	{
		return {name + "-uuid", name, QDateTime()};
	}

	const QDateTime kStart =
		QDateTime(QDate(2026, 11, 3), QTime(12, 0), Qt::UTC);

	/// Feeds @p snapshots one poll apart and returns the notices of the last.
	QList<Notice> afterPolls(const QList<FriendsSnapshot>& snapshots,
							 qint64 gapSeconds = kPollSeconds)
	{
		Memory memory;
		QList<Notice> notices;
		QDateTime now = kStart;
		for (const FriendsSnapshot& snapshot : snapshots) {
			auto result = FriendsNotices::diff(memory, snapshot, now);
			memory = result.memory;
			notices = result.notices;
			now = now.addSecs(gapSeconds);
		}
		return notices;
	}
} // namespace

class FriendsNoticesTest : public QObject
{
	Q_OBJECT

  private slots:
	void firstSnapshotOnlySetsTheBaseline()
	{
		const FriendsSnapshot first{{friendIn("Alex", State::Online),
									 friendIn("Steve", State::Playing)},
									{person("Notch")},
									{}};
		const auto result = FriendsNotices::diff(Memory(), first, kStart);
		QVERIFY(result.notices.isEmpty());
		QVERIFY(result.memory.primed);
		QVERIFY(result.memory.requests.contains("Notch-uuid"));
	}

	void offlineToOnlineIsAnnounced()
	{
		const auto notices =
			afterPolls({{{friendIn("Alex", State::Offline)}, {}, {}},
						{{friendIn("Alex", State::Online)}, {}, {}}});
		QCOMPARE(notices.size(), 1);
		QCOMPARE(notices[0].kind, Notice::Kind::Online);
		QCOMPARE(notices[0].name, QString("Alex"));
		QCOMPARE(notices[0].uuid, QString("Alex-uuid"));
		QCOMPARE(notices[0].status.state, State::Online);
	}

	void offlineToPlayingIsAnnounced()
	{
		const auto notices =
			afterPolls({{{friendIn("Alex", State::Offline)}, {}, {}},
						{{friendIn("Alex", State::Playing)}, {}, {}}});
		QCOMPARE(notices.size(), 1);
		QCOMPARE(notices[0].status.state, State::Playing);
	}

	void onlineToPlayingIsNot()
	{
		const auto notices =
			afterPolls({{{friendIn("Alex", State::Online)}, {}, {}},
						{{friendIn("Alex", State::Playing)}, {}, {}}});
		QVERIFY(notices.isEmpty());
	}

	void stayingOnlineIsNot()
	{
		const auto notices =
			afterPolls({{{friendIn("Alex", State::Offline)}, {}, {}},
						{{friendIn("Alex", State::Online)}, {}, {}},
						{{friendIn("Alex", State::Online)}, {}, {}}});
		QVERIFY(notices.isEmpty());
	}

	void aNewFriendAlreadyOnlineIsNot()
	{
		/* Just accepted: in the list for the first time, and online. */
		const auto notices =
			afterPolls({{{}, {person("Alex")}, {}},
						{{friendIn("Alex", State::Online)}, {}, {}}});
		QVERIFY(notices.isEmpty());
	}

	void flappingIsAnnouncedOnce()
	{
		const FriendsSnapshot offline{{friendIn("Alex", State::Offline)}, {}, {}};
		const FriendsSnapshot online{{friendIn("Alex", State::Online)}, {}, {}};
		Memory memory;
		QDateTime now = kStart;
		int announced = 0;
		for (const auto& snapshot :
			 {offline, online, offline, online, offline, online}) {
			auto result = FriendsNotices::diff(memory, snapshot, now);
			announced += result.notices.size();
			memory = result.memory;
			now = now.addSecs(kPollSeconds);
		}
		QCOMPARE(announced, 1);
	}

	void aBlipOfAFriendOnlineSinceStartIsNot()
	{
		const auto notices =
			afterPolls({{{friendIn("Alex", State::Online)}, {}, {}},
						{{friendIn("Alex", State::Offline)}, {}, {}},
						{{friendIn("Alex", State::Online)}, {}, {}}});
		QVERIFY(notices.isEmpty());
	}

	void backAfterTheQuietPeriodIsAnnouncedAgain()
	{
		const auto notices =
			afterPolls({{{friendIn("Alex", State::Online)}, {}, {}},
						{{friendIn("Alex", State::Offline)}, {}, {}},
						{{friendIn("Alex", State::Online)}, {}, {}}},
					   FriendsNotices::kQuietSeconds);
		QCOMPARE(notices.size(), 1);
	}

	void eachNewRequestIsAnnounced()
	{
		const auto notices = afterPolls(
			{{{}, {person("Notch")}, {}},
			 {{}, {person("Notch"), person("Jeb"), person("Dinnerbone")}, {}}});
		QCOMPARE(notices.size(), 2);
		QCOMPARE(notices[0].kind, Notice::Kind::Request);
		QCOMPARE(notices[0].name, QString("Jeb"));
		QCOMPARE(notices[1].name, QString("Dinnerbone"));
	}

	void aRequestIsAnnouncedOnlyOnce()
	{
		/* Cancelled and sent again: seen already. */
		const auto notices = afterPolls({{{}, {}, {}},
										 {{}, {person("Jeb")}, {}},
										 {{}, {}, {}},
										 {{}, {person("Jeb")}, {}}});
		QVERIFY(notices.isEmpty());
	}

	void outgoingRequestsAreNotAnnounced()
	{
		const auto notices =
			afterPolls({{{}, {}, {}}, {{}, {}, {person("Jeb")}}});
		QVERIFY(notices.isEmpty());
	}

	void aFreshMemoryStartsOver()
	{
		/* Signing in again resets the memory: the first snapshot after it is
		 * a baseline again, whatever it holds. */
		const auto result = FriendsNotices::diff(
			Memory(),
			{{friendIn("Alex", State::Online)}, {person("Jeb")}, {}}, kStart);
		QVERIFY(result.notices.isEmpty());
	}
};

QTEST_GUILESS_MAIN(FriendsNoticesTest)

#include "FriendsNotices_test.moc"
