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

#include <QJsonDocument>
#include <QTest>

#include "FriendsApi.h"
#include "FriendsDisplay.h"
#include "FriendsEvents.h"

using FriendsEvents::Kind;

namespace
{
	const QString kJeb = QStringLiteral("853c80ef-3c37-49fd-aa49-938b674adae6");
	const QString kGrumm = QStringLiteral("11111111-1111-4111-8111-111111111111");

	QJsonObject frame(const QByteArray& json)
	{
		return QJsonDocument::fromJson(json).object();
	}

	/* jeb_ is a friend, online, with 2 unread messages; Grumm was asked. */
	FriendsApi::FriendsSnapshot snapshot()
	{
		const auto parsed = FriendsApi::parseFriends(R"({
			"friends": [ { "uuid": "853c80ef-3c37-49fd-aa49-938b674adae6", "name": "jeb_",
						   "since": "2026-10-20T12:00:00.000Z", "status": { "state": "online" },
						   "unread": 2 } ],
			"requests": { "incoming": [],
						  "outgoing": [ { "uuid": "11111111-1111-4111-8111-111111111111", "name": "Grumm",
										  "createdAt": "2026-10-24T17:50:00.000Z" } ] } })");
		return parsed ? *parsed : FriendsApi::FriendsSnapshot();
	}
} // namespace

class FriendsEventsTest : public QObject
{
	Q_OBJECT

  private slots:
	void framesGoWhereTheyBelong()
	{
		QCOMPARE(FriendsEvents::kindOf(frame(R"({"type":"friend.status"})")), Kind::Friend);
		QCOMPARE(FriendsEvents::kindOf(frame(R"({"type":"chat.message"})")), Kind::Chat);
		QCOMPARE(FriendsEvents::kindOf(frame(R"({"type":"error"})")), Kind::Chat);
		QCOMPARE(FriendsEvents::kindOf(frame(R"({"type":"invite.cancelled"})")), Kind::Invite);
		QCOMPARE(FriendsEvents::kindOf(frame(R"({"type":"later.thing"})")), Kind::Other);
		QVERIFY(FriendsEvents::forInvites(frame(R"({"type":"invite"})")));
		QVERIFY(FriendsEvents::forInvites(frame(R"({"type":"invite.cancelled"})")));
		QVERIFY(FriendsEvents::forInvites(frame(R"({"type":"friend.removed"})")));
		QVERIFY(!FriendsEvents::forInvites(frame(R"({"type":"friend.status"})")));
		QVERIFY(!FriendsEvents::forInvites(frame(R"({"type":"chat.message"})")));
	}

	void unreadComesWithTheFriendsList()
	{
		const auto lists = snapshot();
		QCOMPARE(lists.friends.size(), 1);
		QCOMPARE(lists.friends.first().unread, 2);
		const auto shown = FriendsDisplay::friendMap(lists.friends.first(), QDateTime::currentDateTimeUtc());
		QCOMPARE(shown.value("unread").toInt(), 2);
	}

	void aStatusChangesThatFriend()
	{
		auto lists = snapshot();
		const auto outcome = FriendsEvents::apply(lists, frame(R"({"type":"friend.status",
			"uuid":"853c80ef-3c37-49fd-aa49-938b674adae6",
			"status":{"state":"playing","instance":"Hypixel","version":"1.8.9",
					  "world":{"type":"server","host":"mc.hypixel.net","port":25565},"joinable":true}})"));
		QVERIFY(outcome.changed);
		QVERIFY(!outcome.poll);
		QCOMPARE(lists.friends.first().status.state, FriendsApi::Status::State::Playing);
		QCOMPARE(lists.friends.first().status.world.host, QStringLiteral("mc.hypixel.net"));
		QCOMPARE(lists.friends.first().unread, 2);

		const auto stranger = FriendsEvents::apply(lists, frame(R"({"type":"friend.status",
			"uuid":"22222222-2222-4222-8222-222222222222","status":{"state":"online"}})"));
		QVERIFY(!stranger.changed);
		QVERIFY(stranger.poll);
	}

	void aRequestIsAddedOnce()
	{
		auto lists = snapshot();
		const QByteArray request = R"({"type":"friend.request",
			"user":{"uuid":"33333333-3333-4333-8333-333333333333","name":"Dinnerbone"},
			"createdAt":"2026-10-24T17:58:00.000Z"})";
		QVERIFY(FriendsEvents::apply(lists, frame(request)).changed);
		QCOMPARE(lists.incoming.size(), 1);
		QCOMPARE(lists.incoming.first().name, QStringLiteral("Dinnerbone"));
		QVERIFY(!FriendsEvents::apply(lists, frame(request)).changed);
		QCOMPARE(lists.incoming.size(), 1);
	}

	void acceptedAndRemovedAskForAFullRead()
	{
		auto lists = snapshot();
		const auto accepted = FriendsEvents::apply(lists, frame(R"({"type":"friend.accepted",
			"user":{"uuid":"11111111-1111-4111-8111-111111111111","name":"Grumm"}})"));
		QVERIFY(accepted.changed);
		QVERIFY(accepted.poll);
		QVERIFY(lists.outgoing.isEmpty());

		const auto removed = FriendsEvents::apply(lists, frame(R"({"type":"friend.removed",
			"uuid":"853C80EF-3C37-49FD-AA49-938B674ADAE6"})"));
		QVERIFY(removed.changed);
		QVERIFY(removed.poll);
		QVERIFY(lists.friends.isEmpty());
	}

	void unreadCountsMoveWithTheChat()
	{
		auto lists = snapshot();
		QVERIFY(FriendsEvents::addUnread(lists, kJeb));
		QCOMPARE(lists.friends.first().unread, 3);
		QVERIFY(FriendsEvents::setUnread(lists, kJeb, 0));
		QVERIFY(!FriendsEvents::setUnread(lists, kJeb, 0));
		QVERIFY(!FriendsEvents::addUnread(lists, kGrumm));
	}
};

QTEST_GUILESS_MAIN(FriendsEventsTest)

#include "FriendsEvents_test.moc"
