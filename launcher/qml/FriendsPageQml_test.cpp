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
 * The Friends page over FriendsDevController's made-up people: the tabs' counts
 * against their rows, the search, the add field's rules for a name, the
 * buttons of a request and what the page shows while friends cannot be used.
 * Nothing here talks to a server.
 */

#include "FriendChatQmlHarness.h"

class FriendsPageQmlTest : public FriendChatQmlHarness
{
	Q_OBJECT

	void loadPage(const QString& scenario, const QString& tab = QStringLiteral("friends"), int width = 1000)
	{
		load(QStringLiteral(R"(
			import QtQuick
			import Kite.Components
			import Kite.Theme
			Window {
				width: %3
				height: 700
				visible: true
				color: Theme.palette.canvas
				FriendsDevController { id: people; objectName: "people"; scenario: "%1" }
				FriendsPage {
					objectName: "page"
					anchors.fill: parent
					controller: people
					tab: "%2"
					shown: true
				}
			}
		)").arg(scenario, tab).arg(width).toUtf8());
	}

	/* The first visible button under @p item that says @p text; by what is drawn, as a Repeater's rows are not
	 * QObject children of the page. */
	static QQuickItem* buttonUnder(QQuickItem* item, const QString& text)
	{
		if (!item || !item->isVisible()) {
			return nullptr;
		}
		if (item->inherits("QQuickAbstractButton") && item->property("text").toString() == text) {
			return item;
		}
		for (QQuickItem* child : item->childItems()) {
			if (QQuickItem* found = buttonUnder(child, text)) {
				return found;
			}
		}
		return nullptr;
	}
	QQuickItem* button(const QString& text) const
	{
		return buttonUnder(window()->contentItem(), text);
	}
	void click(QQuickItem* item)
	{
		QVERIFY(item);
		const QPointF centre = item->mapToScene(QPointF(item->width() / 2, item->height() / 2));
		QTest::mouseClick(window(), Qt::LeftButton, Qt::NoModifier, centre.toPoint());
	}
	void keys(const QString& text)
	{
		for (const QChar one : text) {
			QTest::keyClick(window(), one.toLatin1());
		}
	}
	QQuickItem* field(const char* name) const
	{
		return qobject_cast<QQuickItem*>(find(name));
	}
	void type(const char* name, const QString& text)
	{
		QQuickItem* target = field(name);
		QVERIFY(target);
		target->forceActiveFocus();
		QTRY_VERIFY(target->hasActiveFocus());
		keys(text);
	}
	int listLength(const char* list) const
	{
		return find("people")->property(list).toList().size();
	}

  private slots:
	void init()
	{
		setUp();
	}
	void cleanup()
	{
		tearDown();
	}

	/* The Requests tab counts every row it shows, the ones you sent too: it said 2 over 4 rows. */
	void theRequestsTabCountsItsRows()
	{
		loadPage(QStringLiteral("full"), QStringLiteral("requests"));
		QTRY_VERIFY(showing(QStringLiteral("Incoming · 2")));
		QVERIFY(showing(QStringLiteral("Sent · 2")));
		QVERIFY(showing(QStringLiteral("4")));
		QVERIFY(showing(QStringLiteral("Notch")));
		QVERIFY(showing(QStringLiteral("slicedlime")));
	}

	/* Accept, Decline and Cancel are buttons, and the row goes with the answer. */
	void aRequestIsAnsweredFromItsRow()
	{
		loadPage(QStringLiteral("full"), QStringLiteral("requests"));
		QTRY_VERIFY(button(QStringLiteral("Accept")));
		QVERIFY(button(QStringLiteral("Decline")));
		click(button(QStringLiteral("Cancel request")));
		QTRY_COMPARE(listLength("outgoing"), 1);
		QTRY_VERIFY(showing(QStringLiteral("Sent · 1")));
		click(button(QStringLiteral("Accept")));
		QTRY_COMPARE(listLength("incoming"), 1);
		QTRY_COMPARE(listLength("friends"), 9);
		QTRY_VERIFY(showing(QStringLiteral("Incoming · 1")));
		click(button(QStringLiteral("Decline")));
		QTRY_COMPARE(listLength("incoming"), 0);
		QTRY_VERIFY(!showing(QStringLiteral("Incoming · 1")));
	}

	/* Spaces never get into a name and it stops at 16 letters; Enter sends it and the field empties. */
	void theAddFieldTakesAMinecraftName()
	{
		loadPage(QStringLiteral("full"));
		type("friendNameField", QStringLiteral(" No tch_1234567890abcdef "));
		QCOMPARE(field("friendNameField")->property("text").toString(), QStringLiteral("Notch_1234567890"));
		QVERIFY(button(QStringLiteral("Add"))->isEnabled());
		QTest::keyClick(window(), Qt::Key_Return);
		QTRY_COMPARE(listLength("outgoing"), 3);
		QTRY_COMPARE(field("friendNameField")->property("text").toString(), QString());
		QVERIFY(field("friendNameField")->hasActiveFocus());
	}

	/* Your own name, in any case, and a name that cannot be one are refused before anything is sent. */
	void aNameThatCannotBeAddedSaysWhy()
	{
		loadPage(QStringLiteral("full"));
		type("friendNameField", QStringLiteral("grxtor"));
		QTRY_VERIFY(showing(QStringLiteral("That is your own name.")));
		QVERIFY(!button(QStringLiteral("Add"))->isEnabled());
		QTest::keyClick(window(), Qt::Key_Return);
		QTest::qWait(100);
		QCOMPARE(listLength("outgoing"), 2);

		QTest::keyClick(window(), Qt::Key_Escape);
		QCOMPARE(field("friendNameField")->property("text").toString(), QString());
		keys(QStringLiteral("no-dash"));
		QTRY_VERIFY(showing(QStringLiteral("A Minecraft name has 1 to 16 letters, numbers or underscores.")));
		QVERIFY(!button(QStringLiteral("Add"))->isEnabled());
	}

	/* A name that failed stays in the field, selected, with the cursor still there. */
	void aRefusedNameStaysToBeCorrected()
	{
		loadPage(QStringLiteral("full"));
		type("friendNameField", QStringLiteral("JEB_"));
		QTest::keyClick(window(), Qt::Key_Return);
		QTRY_COMPARE(field("friendNameField")->property("selectedText").toString(), QStringLiteral("JEB_"));
		QVERIFY(field("friendNameField")->hasActiveFocus());
		QCOMPARE(listLength("outgoing"), 2);
	}

	void theSearchNarrowsTheTab()
	{
		loadPage(QStringLiteral("full"));
		QTRY_VERIFY(showing(QStringLiteral("jeb_")));
		type("friendSearch", QStringLiteral("DINNER"));
		QTRY_VERIFY(!showing(QStringLiteral("jeb_")));
		QVERIFY(showing(QStringLiteral("Dinnerbone")));
		QVERIFY(showing(QStringLiteral("In game · 1")));
		keys(QStringLiteral("zz"));
		QTRY_VERIFY(showing(QStringLiteral("No one called “DINNERzz”")));
		click(button(QStringLiteral("Clear search")));
		QTRY_VERIFY(showing(QStringLiteral("jeb_")));
		QCOMPARE(field("friendSearch")->property("text").toString(), QString());
	}

	/* With nobody yet the tabs and the add field are still there, and the empty state's button leads to the field. */
	void anEmptyListLeadsToTheAddField()
	{
		loadPage(QStringLiteral("empty"));
		QTRY_VERIFY(showing(QStringLiteral("No friends yet")));
		QVERIFY(showing(QStringLiteral("Requests")));
		QVERIFY(!field("friendSearch")->isVisible());
		click(button(QStringLiteral("Add a friend")));
		QTRY_VERIFY(field("friendNameField")->hasActiveFocus());
	}

	void withoutAnAccountItAsksToSignIn()
	{
		loadPage(QStringLiteral("noaccount"));
		QTRY_VERIFY(showing(QStringLiteral("Sign in to use Friends")));
		QVERIFY(button(QStringLiteral("Sign in")));
		QVERIFY(!field("friendNameField")->isVisible());
	}

	/* A poll that failed keeps the lists under a bar; nothing that needs the server can be pressed. */
	void aFailedReadKeepsTheListsButNotTheirButtons()
	{
		loadPage(QStringLiteral("stale"), QStringLiteral("requests"));
		QTRY_VERIFY(showing(QStringLiteral("Notch")));
		QVERIFY(button(QStringLiteral("Try again")));
		QVERIFY(!button(QStringLiteral("Accept"))->isEnabled());
		QVERIFY(!field("friendNameField")->isEnabled());
	}

	/* Narrow, the search and the add field take a line each and nothing is cut off. */
	void aNarrowPageStacksItsToolbar()
	{
		loadPage(QStringLiteral("full"), QStringLiteral("friends"), 480);
		QTRY_VERIFY(showing(QStringLiteral("jeb_")));
		QQuickItem* search = field("friendSearch");
		QQuickItem* name = field("friendNameField");
		QVERIFY(name->mapToScene(QPointF(0, 0)).y() > search->mapToScene(QPointF(0, 0)).y() + search->height() - 1);
		QQuickItem* add = button(QStringLiteral("Add"));
		QVERIFY(add->mapToScene(QPointF(add->width(), 0)).x() <= 480);
	}
};

int main(int argc, char** argv)
{
	/* No window on anybody's screen. */
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);
	FriendsPageQmlTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "FriendsPageQml_test.moc"
