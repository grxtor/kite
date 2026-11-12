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
 * The 1.3.1 launcher dialogs over their real C++ side: "What's new" (WhatsNew),
 * "Edit the bar" and "Add a server" of Home's quick-join bar (QuickJoinLayout),
 * the bar's one-time note, and a drag in the bar itself.
 */

#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#include "news/WhatsNew.h"
#include "servers/FeaturedServers.h"
#include "servers/QuickJoinLayout.h"
#include "settings/INISettingsObject.h"

namespace
{
	const QByteArray kNotes = R"({
		"formatVersion": 1,
		"releases": [
			{ "version": "1.3.1", "items": [
				{ "icon": "sliders", "title": { "en": "A top bar of your own" },
				  "text": { "en": "Drag the servers into your order." } } ] },
			{ "version": "1.3.0", "items": [
				{ "icon": "server", "title": { "en": "Big servers, one click away" },
				  "text": { "en": "A click joins." } },
				{ "icon": "users", "title": { "en": "A new friends panel" },
				  "text": { "en": "Invites and requests come first." } } ] } ]
	})";

	const QByteArray kServers = R"({ "formatVersion": 1, "servers": [
		{ "id": "a", "name": "Alpha", "address": "play.alpha.example" },
		{ "id": "b", "name": "Bravo", "address": "play.bravo.example" },
		{ "id": "c", "name": "Charlie", "address": "play.charlie.example" } ] })";

	/* A ServerPinger stand-in: statuses the test sets, and the addresses asked. */
	const char* kFakePinger = R"(
		QtObject {
			id: fakePinger
			objectName: "pinger"
			property int revision: 0
			property var states: ({})
			property var asked: []
			function status(address) { return fakePinger.states[address] || ({}) }
			function ping(address) {
				fakePinger.asked = fakePinger.asked.concat([address])
				if (!fakePinger.states[address])
					fakePinger.answer(address, "pending")
			}
			function refresh(address) { fakePinger.ping(address) }
			function friendsOn(address, friends) { return [] }
			function answer(address, state) {
				var next = Object.assign({}, fakePinger.states)
				next[address] = { state: state, online: 120, ping: 30 }
				fakePinger.states = next
				fakePinger.revision++
			}
		})";

	/* Every visible item under @p item, depth first, by the visual tree (a view's delegates too). */
	void collectItems(QQuickItem* item, QList<QQuickItem*>& out)
	{
		if (!item || !item->isVisible()) {
			return;
		}
		out << item;
		for (QQuickItem* child : item->childItems()) {
			collectItems(child, out);
		}
	}

	void collectTexts(QQuickItem* item, QStringList& out)
	{
		if (!item || !item->isVisible()) {
			return;
		}
		const QVariant text = item->property("text");
		if (item->inherits("QQuickText") && !text.toString().isEmpty()) {
			out << text.toString();
		}
		for (QQuickItem* child : item->childItems()) {
			collectTexts(child, out);
		}
	}

	QStringList idsOf(const QVariantList& entries)
	{
		QStringList ids;
		for (const QVariant& entry : entries) {
			ids.append(entry.toMap().value("id").toString());
		}
		return ids;
	}
} // namespace

class LauncherUxQmlTest : public QObject
{
	Q_OBJECT

	QTemporaryDir m_dir;
	SettingsObjectPtr m_settings;
	std::unique_ptr<FeaturedServers> m_featured;
	std::unique_ptr<QuickJoinLayout> m_layout;
	std::unique_ptr<WhatsNew> m_notes;
	std::unique_ptr<QQmlEngine> m_engine;
	std::unique_ptr<QObject> m_root;

	QQuickWindow* window() const
	{
		return qobject_cast<QQuickWindow*>(m_root.get());
	}
	QObject* find(const char* name) const
	{
		return m_root->findChild<QObject*>(QLatin1String(name));
	}
	static QList<QQuickItem*> visibleItems(QQuickItem* under)
	{
		QList<QQuickItem*> out;
		collectItems(under, out);
		return out;
	}
	QStringList texts() const
	{
		QStringList out;
		collectTexts(window()->contentItem(), out);
		return out;
	}
	bool showing(const QString& text) const
	{
		const QStringList all = texts();
		if (!all.contains(text) && qEnvironmentVariableIsSet("KITE_UX_DEBUG")) {
			qDebug() << "on screen:" << all;
		}
		return all.contains(text);
	}
	/* The visible button (any AbstractButton) whose text is @p text, under the window. */
	QObject* button(const QString& text) const
	{
		for (QQuickItem* item : visibleItems(window()->contentItem())) {
			if (item->inherits("QQuickAbstractButton") && item->property("text").toString() == text) {
				return item;
			}
		}
		return nullptr;
	}
	void click(const QString& text)
	{
		QObject* target = button(text);
		QVERIFY2(target, qPrintable(QStringLiteral("no button %1").arg(text)));
		QVERIFY(QMetaObject::invokeMethod(target, "clicked"));
	}
	QList<QObject*> rows() const
	{
		QList<QObject*> out;
		for (QQuickItem* item : visibleItems(window()->contentItem())) {
			if (item->objectName() == QLatin1String("quickJoinEditRow")) {
				out.append(item);
			}
		}
		std::sort(out.begin(), out.end(), [](QObject* a, QObject* b) {
			return qobject_cast<QQuickItem*>(a)->y() < qobject_cast<QQuickItem*>(b)->y();
		});
		return out;
	}

	/* @p body inside a Window, with the layout, the notes and a fake pinger as `testLayout`, `testNotes` and
	 * `fakePinger` (names no component has a property of). */
	void load(const QString& body)
	{
		m_root.reset();
		m_engine = std::make_unique<QQmlEngine>();
		m_engine->addImportPath(QStringLiteral("qrc:/qt/qml"));
		m_engine->rootContext()->setContextProperty(QStringLiteral("testLayout"), m_layout.get());
		m_engine->rootContext()->setContextProperty(QStringLiteral("testNotes"), m_notes.get());
		const QString qml = QStringLiteral(R"(
			import QtQuick
			import Kite.Components
			import Kite.Theme
			Window {
				width: 760
				height: 620
				visible: true
				color: Theme.palette.canvas
				%1
				%2
			})")
								.arg(QString::fromUtf8(kFakePinger), body);
		QQmlComponent component(m_engine.get());
		component.setData(qml.toUtf8(), QUrl());
		m_root.reset(component.create());
		QVERIFY2(m_root, qPrintable(component.errorString()));
		QVERIFY(QTest::qWaitForWindowExposed(window()));
	}

	QObject* pinger() const
	{
		return find("pinger");
	}

  private slots:
	void initTestCase()
	{
		QQuickStyle::setStyle(QStringLiteral("Kite.Style"));
	}

	void init()
	{
		QVERIFY(m_dir.isValid());
		const QString file = m_dir.filePath(QStringLiteral("kite.cfg"));
		QFile::remove(file);
		auto settings = std::make_shared<INISettingsObject>(file);
		settings->registerSetting(QuickJoinLayout::kLayoutSetting, QString());
		settings->registerSetting(QuickJoinLayout::kHintSetting, false);
		settings->registerSetting(WhatsNew::kSeenSetting, QString("1.3.0"));
		settings->registerSetting("Language", QString("en_US"));
		m_settings = settings;
		m_featured = std::make_unique<FeaturedServers>();
		QVERIFY(m_featured->loadFromJson(kServers));
		m_layout = std::make_unique<QuickJoinLayout>(m_featured.get(), m_settings);
		notesFrom(QStringLiteral("1.3.0"));
	}

	/* The notes of Kite 1.3.1 for someone who last saw @p seen's. */
	void notesFrom(const QString& seen)
	{
		m_settings->set(WhatsNew::kSeenSetting, seen);
		m_notes = std::make_unique<WhatsNew>(kNotes, QStringLiteral("1.3.1"), m_settings);
		m_notes->setLanguage(QStringLiteral("en_US"));
	}

	void cleanup()
	{
		m_root.reset();
		m_engine.reset();
		m_layout.reset();
		m_featured.reset();
		m_notes.reset();
		m_settings.reset();
	}

	void whatsNewShowsTheNotesAndGreatMarksThemSeen()
	{
		QVERIFY(m_notes->pending());
		load(QStringLiteral(R"(
			WhatsNewDialog { id: dialog; objectName: "dialog"; notes: testNotes }
			Component.onCompleted: dialog.open()
		)"));
		QObject* dialog = find("dialog");
		QTRY_VERIFY(dialog->property("opened").toBool());
		// From the last release: its notes alone, no number over them.
		QVERIFY(showing(QStringLiteral("What's new in Kite 1.3.1")));
		QVERIFY(showing(QStringLiteral("A top bar of your own")));
		QVERIFY(showing(QStringLiteral("Drag the servers into your order.")));
		QVERIFY(!showing(QStringLiteral("Big servers, one click away")));
		QVERIFY(!showing(QStringLiteral("Kite 1.3.1")));
		QVERIFY(showing(QStringLiteral("All notes")));
		// One way out, no Cancel beside it.
		QVERIFY(button(QStringLiteral("Great")));
		QVERIFY(!button(QStringLiteral("Cancel")));

		click(QStringLiteral("Great"));
		QTRY_VERIFY(!dialog->property("visible").toBool());
		QVERIFY(!m_notes->pending());
		QCOMPARE(m_settings->get(WhatsNew::kSeenSetting).toString(), QString("1.3.1"));
	}

	void whatsNewFromFurtherBackListsEachReleaseSince()
	{
		notesFrom(QStringLiteral("1.2.0"));
		load(QStringLiteral(R"(
			WhatsNewDialog { id: dialog; objectName: "dialog"; notes: testNotes }
			Component.onCompleted: dialog.open()
		)"));
		QObject* dialog = find("dialog");
		QTRY_VERIFY(dialog->property("opened").toBool());
		QVERIFY(showing(QStringLiteral("What's new since 1.2.0")));
		QVERIFY(showing(QStringLiteral("Kite 1.3.1")));
		QVERIFY(showing(QStringLiteral("Kite 1.3.0")));
		QVERIFY(showing(QStringLiteral("A top bar of your own")));
		QVERIFY(showing(QStringLiteral("Big servers, one click away")));
		QVERIFY(showing(QStringLiteral("A new friends panel")));
		// About's "What's new" afterwards: this version's alone.
		click(QStringLiteral("Great"));
		QTRY_VERIFY(!dialog->property("visible").toBool());
		m_notes->showLatest();
		QVERIFY(QMetaObject::invokeMethod(dialog, "open"));
		QTRY_VERIFY(dialog->property("opened").toBool());
		QTRY_VERIFY(showing(QStringLiteral("What's new in Kite 1.3.1")));
		QVERIFY(!showing(QStringLiteral("Big servers, one click away")));
	}

	void whatsNewClosedAnyWayCountsAsSeen()
	{
		load(QStringLiteral(R"(
			WhatsNewDialog { id: dialog; objectName: "dialog"; notes: testNotes }
			Component.onCompleted: dialog.open()
		)"));
		QObject* dialog = find("dialog");
		QTRY_VERIFY(dialog->property("opened").toBool());
		QVERIFY(QMetaObject::invokeMethod(dialog, "close"));
		QTRY_VERIFY(!m_notes->pending());
	}

	void editTheBarSavesOrderHiddenAndRemovals()
	{
		QVERIFY(m_layout->pin(QStringLiteral("Friends"), QStringLiteral("mc.friends.example")));
		const QString own = QuickJoinLayout::ownId(QStringLiteral("mc.friends.example"));
		load(QStringLiteral(R"(
			QuickJoinEditDialog { id: dialog; objectName: "dialog"; layout: testLayout; pinger: fakePinger }
			Component.onCompleted: dialog.edit()
		)"));
		QObject* dialog = find("dialog");
		QTRY_VERIFY(dialog->property("opened").toBool());
		QTRY_COMPARE(rows().size(), 4);
		QVERIFY(showing(QStringLiteral("Alpha")));
		QVERIFY(showing(QStringLiteral("Your server · mc.friends.example")));
		// Each row's favicon was asked for.
		QTRY_COMPARE(pinger()->property("asked").toStringList().size(), 4);

		// Alpha dragged two rows down, Bravo unticked, their own server removed.
		QList<QObject*> list = rows();
		QVERIFY(QMetaObject::invokeMethod(list.at(0), "dragged", Q_ARG(int, 1)));
		QVERIFY(QMetaObject::invokeMethod(list.at(0), "dragged", Q_ARG(int, 2)));
		QVERIFY(QMetaObject::invokeMethod(list.at(0), "dragFinished"));
		QTRY_COMPARE(rows().at(2)->property("name").toString(), QString("Alpha"));
		list = rows();
		QCOMPARE(list.at(0)->property("name").toString(), QString("Bravo"));
		QVERIFY(QMetaObject::invokeMethod(list.at(0), "shownToggled", Q_ARG(bool, false)));
		QVERIFY(QMetaObject::invokeMethod(list.at(3), "removeRequested"));
		QTRY_COMPARE(rows().size(), 3);
		// Nothing changes before Save.
		QCOMPARE(idsOf(m_layout->shown()), QStringList({"a", "b", "c", own}));

		click(QStringLiteral("Save"));
		QTRY_VERIFY(!dialog->property("visible").toBool());
		QCOMPARE(idsOf(m_layout->entries()), QStringList({"b", "c", "a"}));
		QCOMPARE(idsOf(m_layout->shown()), QStringList({"c", "a"}));
	}

	void editTheBarAddsAServerThatWaitsForSave()
	{
		load(QStringLiteral(R"(
			QuickJoinEditDialog { id: dialog; objectName: "dialog"; layout: testLayout; pinger: fakePinger }
			Component.onCompleted: dialog.edit()
		)"));
		QObject* dialog = find("dialog");
		QTRY_VERIFY(dialog->property("opened").toBool());
		QTRY_COMPARE(rows().size(), 3);
		click(QStringLiteral("Add a server…"));
		QObject* address = nullptr;
		QTRY_VERIFY((address = find("quickJoinAddAddress")) && address->property("visible").toBool());
		address->setProperty("text", QStringLiteral("mc.friends.example"));
		find("quickJoinAddName")->setProperty("text", QStringLiteral("Friends"));
		click(QStringLiteral("Add"));
		QVERIFY(QMetaObject::invokeMethod(pinger(), "answer", Q_ARG(QVariant, "mc.friends.example"),
										  Q_ARG(QVariant, "online")));
		// A row at the end, with a remove of its own; the bar itself waits for Save.
		QTRY_COMPARE(rows().size(), 4);
		QObject* added = rows().last();
		QCOMPARE(added->property("name").toString(), QString("Friends"));
		QVERIFY(added->property("own").toBool());
		QCOMPARE(m_layout->shown().size(), 3);
		// The address of a big server already listed shows that row again instead.
		QVERIFY(QMetaObject::invokeMethod(rows().at(1), "shownToggled", Q_ARG(bool, false)));
		click(QStringLiteral("Add a server…"));
		QTRY_VERIFY(find("quickJoinAddAddress")->property("visible").toBool());
		find("quickJoinAddAddress")->setProperty("text", QStringLiteral("PLAY.bravo.example:25565"));
		click(QStringLiteral("Add"));
		QVERIFY(QMetaObject::invokeMethod(pinger(), "answer", Q_ARG(QVariant, "PLAY.bravo.example:25565"),
										  Q_ARG(QVariant, "online")));
		QTRY_VERIFY(rows().at(1)->property("shown").toBool());
		QCOMPARE(rows().size(), 4);

		click(QStringLiteral("Save"));
		QTRY_VERIFY(!dialog->property("visible").toBool());
		const QString own = QuickJoinLayout::ownId(QStringLiteral("mc.friends.example"));
		QCOMPARE(idsOf(m_layout->shown()), QStringList({"a", "b", "c", own}));
		QVERIFY(m_layout->shown().at(1).toMap().value("own").toBool());
	}

	void editTheBarResetsAndCancelKeepsTheBar()
	{
		m_layout->move(QStringLiteral("c"), 0);
		m_layout->remove(QStringLiteral("a"));
		load(QStringLiteral(R"(
			QuickJoinEditDialog { id: dialog; objectName: "dialog"; layout: testLayout; pinger: fakePinger }
			Component.onCompleted: dialog.edit()
		)"));
		QObject* dialog = find("dialog");
		QTRY_VERIFY(dialog->property("opened").toBool());
		QTRY_COMPARE(rows().size(), 3);
		QCOMPARE(rows().at(0)->property("name").toString(), QString("Charlie"));

		click(QStringLiteral("Reset to default"));
		QTRY_COMPARE(rows().at(0)->property("name").toString(), QString("Alpha"));
		QVERIFY(rows().at(0)->property("shown").toBool());
		click(QStringLiteral("Cancel"));
		QTRY_VERIFY(!dialog->property("visible").toBool());
		QCOMPARE(idsOf(m_layout->shown()), QStringList({"c", "b"}));

		// Reset and Save is the default bar.
		QVERIFY(QMetaObject::invokeMethod(dialog, "edit"));
		QTRY_VERIFY(dialog->property("opened").toBool());
		click(QStringLiteral("Reset to default"));
		click(QStringLiteral("Save"));
		QTRY_COMPARE(idsOf(m_layout->shown()), QStringList({"a", "b", "c"}));
	}

	void addAServerPingsItFirst()
	{
		load(QStringLiteral(R"(
			QuickJoinAddDialog { id: dialog; objectName: "dialog"; layout: testLayout; pinger: fakePinger }
			Component.onCompleted: dialog.start()
		)"));
		QObject* dialog = find("dialog");
		QObject* address = find("quickJoinAddAddress");
		QObject* name = find("quickJoinAddName");
		QTRY_VERIFY(dialog->property("opened").toBool());
		QVERIFY(!button(QStringLiteral("Add"))->property("enabled").toBool());

		// Not an address at all.
		address->setProperty("text", QStringLiteral(":25565"));
		QTRY_VERIFY(showing(QStringLiteral("That is not a server address.")));
		QVERIFY(!button(QStringLiteral("Add"))->property("enabled").toBool());

		// A server that answers joins the bar at once, named as asked.
		address->setProperty("text", QStringLiteral("mc.friends.example"));
		name->setProperty("text", QStringLiteral("Friends"));
		click(QStringLiteral("Add"));
		QTRY_VERIFY(showing(QStringLiteral("Asking the server…")));
		QCOMPARE(pinger()->property("asked").toStringList(), QStringList({"mc.friends.example"}));
		QVERIFY(QMetaObject::invokeMethod(pinger(), "answer", Q_ARG(QVariant, "mc.friends.example"),
										  Q_ARG(QVariant, "online")));
		QTRY_VERIFY(!dialog->property("visible").toBool());
		const QVariantMap added = m_layout->shown().last().toMap();
		QCOMPARE(added.value("name").toString(), QString("Friends"));
		QVERIFY(added.value("own").toBool());
	}

	void aServerThatDoesNotAnswerIsAddedOnlyOnASecondPress()
	{
		load(QStringLiteral(R"(
			QuickJoinAddDialog { id: dialog; objectName: "dialog"; layout: testLayout; pinger: fakePinger }
			Component.onCompleted: dialog.start()
		)"));
		QObject* dialog = find("dialog");
		QTRY_VERIFY(dialog->property("opened").toBool());
		find("quickJoinAddAddress")->setProperty("text", QStringLiteral("down.example:25570"));
		click(QStringLiteral("Add"));
		QVERIFY(QMetaObject::invokeMethod(pinger(), "answer", Q_ARG(QVariant, "down.example:25570"),
										  Q_ARG(QVariant, "offline")));
		QTRY_VERIFY(showing(QStringLiteral("This server is not answering. Check the address, or add it anyway.")));
		QVERIFY(dialog->property("visible").toBool());
		QCOMPARE(m_layout->shown().size(), 3);

		click(QStringLiteral("Add anyway"));
		QTRY_VERIFY(!dialog->property("visible").toBool());
		QCOMPARE(m_layout->shown().size(), 4);
		// Without a name it goes by its address.
		QCOMPARE(m_layout->shown().last().toMap().value("name").toString(), QString("down.example:25570"));
	}

	void theBarsNoteShowsOnceAndCloseIsForGood()
	{
		load(QStringLiteral(R"(
			QuickJoinBar {
				objectName: "quickJoin"
				x: 100; y: 40; width: 600
				layout: testLayout
				pinger: fakePinger
				hintAllowed: true
			}
		)"));
		QObject* hint = find("quickJoinHint");
		QVERIFY(hint);
		QTRY_VERIFY(hint->property("opened").toBool());
		QVERIFY(showing(QStringLiteral(
			"Drag these servers to reorder them, right-click one to remove it, or add your own server.")));
		QVERIFY(QMetaObject::invokeMethod(find("quickJoinHintClose"), "clicked"));
		QTRY_VERIFY(!hint->property("visible").toBool());
		QVERIFY(m_layout->hintSeen());
		// Not again, here or on the next start.
		QTest::qWait(1200);
		QVERIFY(!hint->property("visible").toBool());
		QuickJoinLayout again(m_featured.get(), m_settings);
		QVERIFY(again.hintSeen());
	}

	void theBarsNoteWaitsForOtherDialogs()
	{
		load(QStringLiteral(R"(
			QuickJoinBar {
				objectName: "quickJoin"
				x: 100; y: 40; width: 600
				layout: testLayout
				pinger: fakePinger
				hintAllowed: false
			}
		)"));
		QTest::qWait(1200);
		QVERIFY(!find("quickJoinHint")->property("visible").toBool());
		QVERIFY(!m_layout->hintSeen());
	}

	void draggingAnEntryInTheBarMovesIt()
	{
		m_layout->dismissHint();
		load(QStringLiteral(R"(
			QuickJoinBar {
				objectName: "quickJoin"
				x: 20; y: 40; width: 700
				layout: testLayout
				pinger: fakePinger
			}
		)"));
		QTRY_COMPARE(find("quickJoin")->property("fitCount").toInt(), 3);
		QQuickItem* barItem = qobject_cast<QQuickItem*>(find("quickJoin"));
		QList<QQuickItem*> entries;
		for (QQuickItem* item : visibleItems(barItem)) {
			if (item->inherits("QQuickAbstractButton") && item->property("server").isValid()) {
				entries.append(item);
			}
		}
		std::sort(entries.begin(), entries.end(), [](QQuickItem* a, QQuickItem* b) { return a->x() < b->x(); });
		QCOMPARE(entries.size(), 3);
		const auto centre = [](QQuickItem* item) {
			return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint();
		};
		// Charlie, the last, dragged to before Alpha.
		const QPoint from = centre(entries.at(2));
		const QPoint to = entries.at(0)->mapToScene(QPointF(4, entries.at(0)->height() / 2)).toPoint();
		constexpr int kSteps = 12;
		QTest::mousePress(window(), Qt::LeftButton, Qt::NoModifier, from);
		for (int i = 1; i <= kSteps; ++i) {
			QTest::mouseMove(window(), from + (to - from) * i / kSteps, 10);
		}
		QTRY_VERIFY(find("quickJoinDropMark")->property("visible").toBool());
		QTest::mouseRelease(window(), Qt::LeftButton, Qt::NoModifier, to);
		QTRY_COMPARE(idsOf(m_layout->shown()), QStringList({"c", "a", "b"}));
		QCOMPARE(QuickJoinLayout::parse(m_settings->get(QuickJoinLayout::kLayoutSetting).toString()).order,
				 QStringList({"c", "a", "b"}));
		// Off the bar, so no tooltip outlives the window.
		QTest::mouseMove(window(), QPoint(2, window()->height() - 2));
		QTest::qWait(100);
	}
};

QTEST_MAIN(LauncherUxQmlTest)

#include "LauncherUxQml_test.moc"
