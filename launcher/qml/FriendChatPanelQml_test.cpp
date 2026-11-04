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
 * The friends panel around a conversation: Message in a friend's ⋯ (only for
 * friends, not where the conversation is already open) and FriendsSidePanel's
 * openChat(), which opens the panel out and marks the conversation read only
 * while it is on screen in the active window.
 */

#include "FriendChatQmlHarness.h"

using FriendChatQml::face;
using FriendChatQml::kPanelHeight;
using FriendChatQml::localIso;
using FriendsChatFake::kJeb;

/* FriendsController's `chat` as QML meets it: a property typed FriendsChat*. */
class ChatHolder : public QObject
{
	Q_OBJECT
	Q_PROPERTY(FriendsChat* chat READ chat CONSTANT)

  public:
	explicit ChatHolder(FriendsChat* chat) : m_chat(chat) {}
	FriendsChat* chat() const
	{
		return m_chat;
	}

  private:
	FriendsChat* m_chat;
};

class FriendChatPanelQmlTest : public FriendChatQmlHarness
{
	Q_OBJECT

	std::unique_ptr<ChatHolder> m_holder;

  private slots:
	void init()
	{
		setUp();
		m_holder = std::make_unique<ChatHolder>(m_chat.get());
	}

	void cleanup()
	{
		tearDown();
		m_holder.reset();
	}

	/* Message in a friend's ⋯ opens the conversation in the friends panel, opening it out; it counts as read only
	 * while the panel shows it in the active window. */
	void messageInTheMenuOpensTheConversationInThePanel()
	{
		load(QStringLiteral(R"(
			import QtQuick
			import Kite.Components
			import Kite.Theme
			Window {
				width: 900
				height: %1
				visible: true
				color: Theme.palette.canvas
				QtObject {
					id: fakeController
					objectName: "controller"
					property var chat: chatHolder.chat
					property bool enabled: true
					property string state: "ready"
					property bool loaded: true
					property string myName: "Notch"
					property var friends: [{ uuid: "%2", name: "jeb_", face: "%3", state: "playing",
											 worldType: "server", host: "mc.hypixel.net", port: 25565,
											 version: "1.21.4", joinable: true, statusText: "On mc.hypixel.net",
											 unread: 2 }]
					property var invites: []
					property var incoming: []
					property var outgoing: []
					property var blocked: []
					property int onlineCount: 1
					property int incomingCount: 0
					property string error: ""
					property var settings: ({ visibility: "friends", hideServer: false })
					signal changed()
					signal actionFinished(string action, bool ok, string message)
					function refresh() {}
				}
				FriendsSidePanel {
					id: panel
					objectName: "panel"
					anchors.right: parent.right
					anchors.top: parent.top
					anchors.bottom: parent.bottom
					roomWidth: 900
					open: true
					controller: fakeController
					myFace: "%4"
					onExpandedRequested: (wanted) => panel.expanded = wanted
				}
				FriendPersonMenu { id: menu; objectName: "menu" }
				function menuEntries(uuid, removable, messagable) {
					menu.openFor(panel, uuid, "jeb_", removable, messagable)
					var shown = menu.canMessage
					menu.close()
					return shown
				}
			})")
							  .arg(kPanelHeight)
							  .arg(kJeb, face("jeb_"), face("Notch"))
							  .toUtf8(),
			 {{QStringLiteral("chatHolder"), QVariant::fromValue<QObject*>(m_holder.get())}});

		const auto entries = [&](bool removable, const QVariant& messagable) {
			QVariant result;
			QMetaObject::invokeMethod(m_root.get(), "menuEntries", Q_RETURN_ARG(QVariant, result),
									  Q_ARG(QVariant, kJeb), Q_ARG(QVariant, removable), Q_ARG(QVariant, messagable));
			return result.toBool();
		};
		QVERIFY(entries(true, QVariant()));
		QVERIFY(!entries(false, QVariant()));
		QVERIFY(!entries(true, false));

		QObject* panel = find("panel");
		QVERIFY(!panel->property("expanded").toBool());
		QVERIFY(QMetaObject::invokeMethod(panel, "openChat", Q_ARG(QVariant, kJeb)));
		QCOMPARE(m_chat->openUuid(), kJeb);
		QTRY_VERIFY(panel->property("expanded").toBool());
		m_server.answer(FriendsChatFake::reply(200, FriendsChatFake::page(
			{FriendsChatFake::message("1", kJeb, "you on tonight?", localIso(0, 9, 10))})));
		QTRY_VERIFY(showing(QStringLiteral("you on tonight?")));
		QCOMPARE(m_chat->viewing(), QGuiApplication::applicationState() == Qt::ApplicationActive);
		shot(QStringLiteral("chat-in-panel"));

		QObject* profile = find("chatProfile");
		QVERIFY(profile);
		QVERIFY(QMetaObject::invokeMethod(profile, "clicked"));
		QTRY_VERIFY(find("chatProfileCard")->property("opened").toBool());
		shot(QStringLiteral("chat-header-card"));
	}
};

QTEST_MAIN(FriendChatPanelQmlTest)

#include "FriendChatPanelQml_test.moc"
