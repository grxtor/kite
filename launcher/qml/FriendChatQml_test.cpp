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

/*
 * The friends panel's conversation (FriendChatView) over a real FriendsChat
 * with a fake server: days, a deleted message and a failed send on screen,
 * the composer's Enter and limit, the rate limit, a removed friend, an empty
 * conversation and the report dialog. KITE_CHAT_SHOTS=<dir> also saves the
 * states as PNGs (FriendChatQmlHarness.h).
 */

#include "FriendChatQmlHarness.h"

using FriendChatQml::face;
using FriendChatQml::kDinnerbone;
using FriendChatQml::kPanelHeight;
using FriendChatQml::kPanelWidth;
using FriendChatQml::localIso;
using FriendsChatFake::kJeb;
using FriendsChatFake::kMe;

class FriendChatQmlTest : public FriendChatQmlHarness
{
	Q_OBJECT

	/* The view in a panel-sized window, with jeb_ and Dinnerbone as friends. */
	void build()
	{
		load(QStringLiteral(R"(
			import QtQuick
			import Kite.Components
			import Kite.Theme
			Window {
				width: %1
				height: %2
				visible: true
				color: Theme.palette.canvas
				QtObject {
					id: fakeController
					objectName: "controller"
					property var chat: fakeChat
					property string myName: "Notch"
					property var friends: [
						{ uuid: "%3", name: "jeb_", face: "%5", state: "playing", worldType: "server",
						  host: "mc.hypixel.net", port: 25565, version: "1.21.4", joinable: true,
						  statusText: "On mc.hypixel.net", unread: 0 },
						{ uuid: "%4", name: "Dinnerbone", face: "%6", state: "online", statusText: "In Kite" }]
					property string blocked: ""
					function block(uuid) { fakeController.blocked = uuid }
					function removeFriend(uuid) {}
				}
				FriendChatView {
					id: view
					anchors.fill: parent
					controller: fakeController
					myFace: "%7"
				}
			})")
							  .arg(kPanelWidth)
							  .arg(kPanelHeight)
							  .arg(kJeb, kDinnerbone, face("jeb_"), face("Dinnerbone"), face("Notch"))
							  .toUtf8());
	}

	void openJeb()
	{
		using FriendsChatFake::message;
		m_chat->open(kJeb);
		QCOMPARE(m_server.requests.size(), 1);
		QJsonObject gone = message("4", kJeb, QString(), localIso(0, 9, 15));
		gone.insert("text", QJsonValue());
		gone.insert("deleted", true);
		m_server.answer(FriendsChatFake::reply(200, FriendsChatFake::page({
			message("1", kJeb, "you on tonight?", localIso(1, 21, 40)),
			message("2", kJeb, "§6gold stays plain text", localIso(1, 21, 41)),
			message("3", kMe, "yes, after dinner. Hypixel?", localIso(0, 9, 12)),
			gone,
			message("5", kJeb, "Bed wars, I'll make a party. Bring the good pickaxe this time, the one with Efficiency V.",
					localIso(0, 9, 16))})));
	}

  private slots:
	void init()
	{
		setUp();
		build();
	}

	void cleanup()
	{
		tearDown();
	}

	void theConversationShowsDaysDeletedAndFailed()
	{
		openJeb();
		m_server.socketUp = false;
		QVERIFY(m_chat->send(QStringLiteral("see you there")));
		m_server.answer(FriendsChatFake::reply(0));
		QTRY_VERIFY(showing(QStringLiteral("Not sent. Check your connection.")));
		QVERIFY(showing(QStringLiteral("Yesterday")));
		QVERIFY(showing(QStringLiteral("Today")));
		QVERIFY(showing(QStringLiteral("Message deleted")));
		QVERIFY(showing(QStringLiteral("§6gold stays plain text")));
		QVERIFY(showing(QStringLiteral("jeb_")));
		QVERIFY(showing(QStringLiteral("Try again")));
		shot(QStringLiteral("chat-conversation"));
	}

	/* The header says where they play as the friend rows do, and its head or name opens their card. */
	void theHeaderSaysWhereTheyPlayAndOpensTheirCard()
	{
		openJeb();
		QTRY_VERIFY(showing(QStringLiteral("mc.hypixel.net")));
		QVERIFY(showing(QStringLiteral("· 1.21.4")));
		QObject* profile = find("chatProfile");
		QObject* card = find("chatProfileCard");
		QVERIFY(profile && card);
		QVERIFY(QMetaObject::invokeMethod(profile, "clicked"));
		QTRY_VERIFY(card->property("opened").toBool());
		QCOMPARE(card->property("friendUuid").toString(), kJeb);
		QVERIFY(QMetaObject::invokeMethod(profile, "clicked"));
		QTRY_VERIFY(!card->property("visible").toBool());
	}

	void enterSendsAndTheFieldStopsAtTheLimit()
	{
		openJeb();
		auto* field = qobject_cast<QQuickItem*>(find("chatField"));
		QVERIFY(field);
		field->forceActiveFocus();
		field->setProperty("text", QStringLiteral("gg\nwp"));
		QCOMPARE(field->property("text").toString(), QStringLiteral("gg wp"));
		QTest::keyClick(window(), Qt::Key_Return, Qt::ShiftModifier);
		QVERIFY(m_server.frames.isEmpty());
		QCOMPARE(field->property("text").toString(), QStringLiteral("gg wp"));
		QTest::keyClick(window(), Qt::Key_Return);
		QCOMPARE(m_server.frames.size(), 1);
		QCOMPARE(m_server.frames.first().value("text").toString(), QStringLiteral("gg wp"));
		QCOMPARE(field->property("text").toString(), QString());

		field->setProperty("text", QString(FriendsChatApi::kMaxTextLength + 5, QLatin1Char('a')));
		QCOMPARE(field->property("text").toString().size(), FriendsChatApi::kMaxTextLength);
		QTRY_VERIFY(showing(QStringLiteral("No characters left")));
		field->setProperty("text", QString(FriendsChatApi::kMaxTextLength - 40, QLatin1Char('a')));
		QTRY_VERIFY(showing(QStringLiteral("40 characters left")));
		field->setProperty("text", QStringLiteral("Bring the good pickaxe this time. ").repeated(30).left(FriendsChatApi::kMaxTextLength));
		QTRY_VERIFY(showing(QStringLiteral("No characters left")));
		shot(QStringLiteral("chat-composer-limit"));
	}

	void aRateLimitSaysHowLongToWait()
	{
		openJeb();
		m_chat->send(QStringLiteral("one more"));
		m_chat->handleFrame({{"type", "error"}, {"ref", m_server.frames.first().value("ref")},
							 {"error", "rate_limited"}, {"message", "Too many"}, {"retryAfter", 20}});
		QTRY_VERIFY(showing(QStringLiteral("You are sending messages too fast. Wait 20 s.")));
		QTRY_VERIFY(showing(QStringLiteral("Not sent: you are sending messages too fast.")));
	}

	void aRemovedFriendKeepsTheHistoryButNoField()
	{
		openJeb();
		QObject* controller = find("controller");
		controller->setProperty("friends", QVariantList());
		QTRY_VERIFY(showing(QStringLiteral("jeb_ is not on your friends list any more, so you can't message them.")));
		QVERIFY(showing(QStringLiteral("Not your friend any more")));
		QVERIFY(!qobject_cast<QQuickItem*>(find("chatField"))->isVisible());
	}

	void anEmptyConversationSaysHi()
	{
		m_chat->open(kDinnerbone);
		QVERIFY(showing(QStringLiteral("Loading messages…")));
		m_server.answer(FriendsChatFake::reply(200, FriendsChatFake::page({})));
		QTRY_VERIFY(showing(QStringLiteral("No messages yet. Say hi to Dinnerbone.")));
		shot(QStringLiteral("chat-empty"));
	}

	void aReportSaysWhyAndCanBlockToo()
	{
		openJeb();
		QObject* menu = find("chatMessageMenu");
		QObject* dialog = find("chatReportDialog");
		QVERIFY(menu && dialog);
		QVERIFY(QMetaObject::invokeMethod(menu, "reportRequested", Q_ARG(QString, QStringLiteral("5")),
										  Q_ARG(QString, QStringLiteral("Bed wars, I'll make a party."))));
		QTRY_VERIFY(dialog->property("opened").toBool());
		QVERIFY(showing(QStringLiteral("Also block jeb_")));
		shot(QStringLiteral("chat-report-dialog"));

		dialog->setProperty("reason", QStringLiteral("harassment"));
		QVERIFY(QMetaObject::invokeMethod(dialog, "primaryClicked"));
		QCOMPARE(m_server.requests.size(), 1);
		QCOMPARE(m_server.requests.first().path,
				 QStringLiteral("/v1/chats/%1/messages/5/report").arg(kJeb));
		QCOMPARE(m_server.requests.first().body, QByteArray(R"({"reason":"harassment"})"));
		QVERIFY(find("controller")->property("blocked").toString().isEmpty());
	}
};

QTEST_MAIN(FriendChatQmlTest)

#include "FriendChatQml_test.moc"
