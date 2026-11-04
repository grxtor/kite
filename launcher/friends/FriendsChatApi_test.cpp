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

#include <QJsonArray>
#include <QTest>

#include "FriendsChatApi.h"
#include "FriendsChatFakeServer.h"

using FriendsChatFake::kJeb;
using FriendsChatFake::kMe;
using FriendsChatFake::page;

namespace
{
	const QString kTime = QStringLiteral("2026-11-04T10:00:00.000Z");
} // namespace

/* What the launcher sends and reads of the chat (server/friends/README.md "Chat"). */
class FriendsChatApiTest : public QObject
{
	Q_OBJECT

  private slots:
	void textIsCleanedLikeTheServerDoes()
	{
		using namespace FriendsChatApi;
		QCOMPARE(cleanText(QStringLiteral("  gg\n wp\u202e §6gold  ")), QStringLiteral("gg wp §6gold"));
		QVERIFY(!sendable(QStringLiteral(" \n ")));
		QVERIFY(sendable(QString(kMaxTextLength, QLatin1Char('a'))));
		QVERIFY(!sendable(QString(kMaxTextLength + 1, QLatin1Char('a'))));
		QVERIFY(idLess(QStringLiteral("9"), QStringLiteral("10")));
		QVERIFY(idLess(QString(), QStringLiteral("1")));
		QVERIFY(!idLess(QStringLiteral("10"), QStringLiteral("10")));
		QVERIFY(isRef(QStringLiteral("s1")));
		QVERIFY(!isRef(QStringLiteral("bad ref")));
		QCOMPARE(Path::page(kJeb, QStringLiteral("42"), QString(), kPageLimit),
				 QStringLiteral("/v1/chats/%1/messages?before=42&limit=50").arg(kJeb));
	}

	void aDeletedMessageHasNoText()
	{
		const auto parsed = FriendsChatApi::parsePage(page(
			{QJsonObject{{"id", "3"}, {"from", kJeb.toUpper()}, {"to", kMe}, {"text", QJsonValue()},
						 {"createdAt", kTime}, {"deleted", true}}}));
		QVERIFY(parsed);
		QCOMPARE(parsed->messages.size(), 1);
		QVERIFY(parsed->messages.first().deleted);
		QVERIFY(parsed->messages.first().text.isEmpty());
		QCOMPARE(parsed->messages.first().from, kJeb);
		QVERIFY(!FriendsChatApi::parseMessage(QJsonObject{{"from", kJeb}}));
	}
};

QTEST_GUILESS_MAIN(FriendsChatApiTest)

#include "FriendsChatApi_test.moc"
