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

#include <QtTest>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlExpression>
#include <QQuickItem>
#include <QStandardItemModel>
#include <QQmlEngine>
#include <QDesktopServices>
#include <QScopeGuard>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>

#include <functional>
#include <memory>

#include "models/NewsModel.h"

/*
 * Smoke test for the QML module: proves qt_add_qml_module ran, the .qml files
 * are compiled in at the expected resource prefix and the engine can
 * instantiate the root component. A missing module, a shifted prefix or a
 * qmlcachegen failure otherwise gives a green build and an application that
 * shows nothing.
 *
 * The URL is spelled out because QQmlComponent::loadFromModule() needs Qt 6.5.
 */
/* Stands in for QmlShell where Main.qml needs to see one of its signals or
 * properties; Main.qml checks for each shell member before using it. */
class FakeShell : public QObject
{
	Q_OBJECT
	/// Non-null is all Main.qml's openNewInstance() checks before opening.
	Q_PROPERTY(QObject* newInstance READ newInstance CONSTANT)
	Q_PROPERTY(int accountCount READ accountCount CONSTANT)
	Q_PROPERTY(QObject* news READ news CONSTANT)

  public:
	QObject* newInstance()
	{
		return &m_newInstance;
	}
	int accountCount() const
	{
		return m_accountCount;
	}
	QObject* news()
	{
		return &m_news;
	}
	NewsModel m_news;

	/// The last launchInstanceMode() call, as "id:mode".
	Q_INVOKABLE void launchInstanceMode(const QString& id, const QString& mode)
	{
		lastLaunch = id + QLatin1Char(':') + mode;
	}

	/// QmlShell::instancesForServer() stand-in: whatever the test set.
	Q_INVOKABLE QVariantList instancesForServer(const QString&, const QString&,
												const QStringList&) const
	{
		return serverInstances;
	}
	/// The last joinServer() call, as "id:address".
	Q_INVOKABLE bool joinServer(const QString& id, const QString& address)
	{
		lastJoin = id + QLatin1Char(':') + address;
		return true;
	}

	int m_accountCount = 1;
	QString lastLaunch;
	QVariantList serverInstances;
	QString lastJoin;

  signals:
	void importRequested(const QString& source);
	void navigateRequested(const QString& target);

  private:
	QObject m_newInstance;
};

/* Stands in for ContentDrop: ContentDropFlow only talks to it through these. */
class FakeDrop : public QObject
{
	Q_OBJECT

  public:
	Q_INVOKABLE QVariantMap prepare(const QStringList& urls)
	{
		prepared = urls;
		return plan;
	}
	Q_INVOKABLE QVariantMap commit()
	{
		++commits;
		QVariantMap done;
		done.insert(QStringLiteral("text"), QStringLiteral("Added 1 mod."));
		done.insert(QStringLiteral("tone"), QStringLiteral("success"));
		done.insert(QStringLiteral("worldTask"), QVariant());
		done.insert(QStringLiteral("worlds"), 0);
		return done;
	}
	Q_INVOKABLE void chooseWorld(const QString& folder)
	{
		world = folder;
	}
	Q_INVOKABLE void resolveConflicts(const QString& mode)
	{
		conflicts = mode;
	}
	Q_INVOKABLE void cancel()
	{
		++cancels;
	}
	Q_INVOKABLE QObject* installLink(const QString&)
	{
		return nullptr;
	}

	QVariantMap plan;
	QStringList prepared;
	QString world;
	QString conflicts;
	int commits = 0;
	int cancels = 0;

	/* What prepare() answers for a drop of one mod and nothing else. */
	static QVariantMap quietPlan()
	{
		QVariantMap plan;
		plan.insert(QStringLiteral("empty"), false);
		plan.insert(QStringLiteral("modpack"), QString());
		plan.insert(QStringLiteral("extraModpacks"), 0);
		plan.insert(QStringLiteral("links"), QStringList());
		plan.insert(QStringLiteral("otherLinks"), QStringList());
		plan.insert(QStringLiteral("unknown"), QStringList());
		plan.insert(QStringLiteral("needsWorld"), false);
		plan.insert(QStringLiteral("worldChoices"), QVariantList());
		plan.insert(QStringLiteral("hasWorld"), false);
		plan.insert(QStringLiteral("conflicts"), QVariantList());
		plan.insert(QStringLiteral("needsLoader"), false);
		plan.insert(QStringLiteral("running"), false);
		return plan;
	}
};

/* Takes what Qt.openUrlExternally() would hand to the system for the schemes it
 * is registered for (QDesktopServices::setUrlHandler), so a test opens nothing. */
class LinkRecorder : public QObject
{
	Q_OBJECT

  public:
	QList<QUrl> opened;

  public slots:
	void open(const QUrl& url)
	{
		opened.append(url);
	}
};

namespace
{
	/* Every shown item at or under @p item that draws @p text itself (it has
	 * a textFormat), for a check that the text is shown as written. */
	QList<QQuickItem*> textItemsShowing(QQuickItem* item, const QString& text)
	{
		QList<QQuickItem*> found;
		if (item->isVisible() && item->property("textFormat").isValid() &&
			item->property("text").toString() == text) {
			found.append(item);
		}
		const auto children = item->childItems();
		for (QQuickItem* child : children) {
			found += textItemsShowing(child, text);
		}
		return found;
	}

	/* A fake FriendsController for the friends views: ready, with these
	 * friends and invites. */
	QString fakeFriends(const QString& friends, const QString& invites)
	{
		return QStringLiteral(R"(QtObject {
			property bool enabled: true
			property string state: "ready"
			property bool loaded: true
			property var friends: %1
			property var invites: %2
			property var incoming: []
			property var outgoing: []
			property var blocked: []
			property int onlineCount: 0
			property int incomingCount: 0
			signal changed()
			signal actionFinished(string action, bool ok, string message)
		})").arg(friends, invites);
	}

	QString callString(QObject* object, const char* method)
	{
		QVariant result;
		QMetaObject::invokeMethod(object, method, Q_RETURN_ARG(QVariant, result));
		return result.toString();
	}
} // namespace

class QmlModuleTest : public QObject
{
	Q_OBJECT

	/* Main.qml instantiated with @p shell, or null (reason warned) if it fails
	 * to load. */
	std::unique_ptr<QObject> createRoot(QQmlEngine& engine, QObject* shell)
	{
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(
			&engine, QUrl(QStringLiteral("qrc:/qt/qml/Kite/Main.qml")));
		if (!component.isReady()) {
			qWarning() << component.errorString();
			return nullptr;
		}
		std::unique_ptr<QObject> root(component.createWithInitialProperties(
			{{QStringLiteral("instanceModel"),
			  QVariant::fromValue<QObject*>(&m_instances)},
			 {QStringLiteral("selection"),
			  QVariant::fromValue<QObject*>(&m_selection)},
			 {QStringLiteral("shell"), QVariant::fromValue<QObject*>(shell)}}));
		if (!root) {
			qWarning() << component.errorString();
		}
		return root;
	}

	/* @p expression evaluated in Main.qml's scope, where its ids resolve. */
	static QVariant evaluate(QObject* root, const QString& expression)
	{
		QQmlExpression expr(qmlContext(root), root, expression);
		const QVariant result = expr.evaluate();
		if (expr.hasError()) {
			qWarning() << expr.error().toString();
		}
		return result;
	}

	/* Main.qml's navigate(page, sub); an invalid @p sub reaches it as
	 * undefined. */
	static bool navigate(QObject* root, const QString& page,
						 const QVariant& sub = QVariant())
	{
		QVariant ok;
		const bool invoked = QMetaObject::invokeMethod(
			root, "navigate", Q_RETURN_ARG(QVariant, ok),
			Q_ARG(QVariant, page), Q_ARG(QVariant, sub));
		return invoked && ok.toBool();
	}

	QStandardItemModel m_instances;
	QObject m_selection;

  private slots:
	/* The style QmlShell::show() selects: components use its extra properties
	 * (Button.danger, ...), which Qt's default style lacks. */
	void initTestCase()
	{
		QQuickStyle::setStyle(QStringLiteral("Kite.Style"));
	}

	void rootComponentInstantiates()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

		QQmlComponent component(
			&engine, QUrl(QStringLiteral("qrc:/qt/qml/Kite/Main.qml")));

		QVERIFY2(!component.isError(), qPrintable(component.errorString()));
		QVERIFY2(component.isReady(),
				 qPrintable(QStringLiteral("component not ready: %1")
								.arg(component.errorString())));

		/* The root requires the instance model, the selection and the shell;
		 * empty stand-ins prove the component loads, and a required property
		 * left unset would itself be a load error. */
		QStandardItemModel instances;
		QObject selection;
		QObject shell;
		std::unique_ptr<QObject> root(component.createWithInitialProperties(
			{{QStringLiteral("instanceModel"),
			  QVariant::fromValue<QObject*>(&instances)},
			 {QStringLiteral("selection"),
			  QVariant::fromValue<QObject*>(&selection)},
			 {QStringLiteral("shell"),
			  QVariant::fromValue<QObject*>(&shell)}}));
		QVERIFY2(root != nullptr, qPrintable(component.errorString()));

		// Only our Main.qml has this property.
		QCOMPARE(root->property("moduleName").toString(),
				 QStringLiteral("Kite"));
	}

	/* Whole paths and the split (page, sub) form, settings sections included;
	 * an unknown target changes nothing. */
	void navigateOpensPagesAndSettingsSections()
	{
		QQmlEngine engine;
		QObject shell;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);

		QVERIFY(navigate(root.get(), QStringLiteral("settings/java")));
		QCOMPARE(root->property("page").toString(), QStringLiteral("settings"));
		QCOMPARE(evaluate(root.get(), QStringLiteral("settingsPage.section"))
					 .toString(),
				 QStringLiteral("java"));

		QVERIFY(navigate(root.get(), QStringLiteral("settings"),
						 QStringLiteral("proxy")));
		QCOMPARE(evaluate(root.get(), QStringLiteral("settingsPage.section"))
					 .toString(),
				 QStringLiteral("proxy"));

		QVERIFY(navigate(root.get(), QStringLiteral("discover")));
		QCOMPARE(root->property("page").toString(), QStringLiteral("discover"));

		QVERIFY(!navigate(root.get(), QStringLiteral("nowhere/at/all")));
		QCOMPARE(root->property("page").toString(), QStringLiteral("discover"));
	}

	/* No instance by that id (this stand-in has no instanceDetails()):
	 * refused, and the page stays. */
	void navigateRefusesAnUnknownInstance()
	{
		QQmlEngine engine;
		QObject shell;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);

		QVERIFY(navigate(root.get(), QStringLiteral("discover")));
		QVERIFY(!navigate(root.get(), QStringLiteral("instance/fabric/settings")));
		QVERIFY(!navigate(root.get(), QStringLiteral("instance")));
		QCOMPARE(root->property("page").toString(), QStringLiteral("discover"));
	}

	/* QmlShell::requestNavigate() reaches Main.qml's navigate(). */
	void navigateRequestFromTheShellIsFollowed()
	{
		QQmlEngine engine;
		FakeShell shell;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);

		emit shell.navigateRequested(QStringLiteral("settings/java"));
		QCOMPARE(root->property("page").toString(), QStringLiteral("settings"));
		QCOMPARE(evaluate(root.get(), QStringLiteral("settingsPage.section"))
					 .toString(),
				 QStringLiteral("java"));
	}

	/* With no account, "Play offline" and the demo go to the Accounts page
	 * instead of launching. */
	void launchModeNeedsAnAccount()
	{
		QQmlEngine engine;
		FakeShell shell;
		shell.m_accountCount = 0;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);

		QVariant unused;
		QVERIFY(QMetaObject::invokeMethod(
			root.get(), "launchMode", Q_RETURN_ARG(QVariant, unused),
			Q_ARG(QVariant, QStringLiteral("fabric")),
			Q_ARG(QVariant, QStringLiteral("offline"))));
		QVERIFY(shell.lastLaunch.isEmpty());
		QCOMPARE(root->property("page").toString(), QStringLiteral("accounts"));

		QVERIFY(navigate(root.get(), QStringLiteral("discover")));
		QVERIFY(QMetaObject::invokeMethod(
			root.get(), "launchMode", Q_RETURN_ARG(QVariant, unused),
			Q_ARG(QVariant, QStringLiteral("fabric")),
			Q_ARG(QVariant, QStringLiteral("demo"))));
		QVERIFY(shell.lastLaunch.isEmpty());
		QCOMPARE(root->property("page").toString(), QStringLiteral("accounts"));
	}

	/* Quick join: the selected instance joins when the server takes its
	 * Minecraft; otherwise nothing starts and the dialog lists those that fit,
	 * saying why the selected one is not used. */
	void serverJoinUsesTheSelectedInstanceOnlyWhenItFits()
	{
		const auto instance = [](const QString& id, const QString& version,
								 bool fits) {
			return QVariantMap{{"id", id},		   {"name", id.toUpper()},
							   {"iconKey", ""},	   {"version", version},
							   {"loader", ""},	   {"lastLaunch", 0},
							   {"running", false}, {"canLaunch", true},
							   {"fits", fits}};
		};
		QQmlEngine engine;
		FakeShell shell;
		shell.serverInstances = {instance("new", "1.21.4", true),
								 instance("old", "1.7.10", false)};
		auto root = createRoot(engine, &shell);
		QVERIFY(root);
		const QString hypixel = QStringLiteral(
			"({ id: 'hypixel', name: 'Hypixel', address: 'mc.hypixel.net', "
			"minVersion: '1.8', maxVersion: '', versions: [] })");

		root->setProperty("selectedId", QStringLiteral("new"));
		evaluate(root.get(), QStringLiteral("serverJoin.join(%1)").arg(hypixel));
		QCOMPARE(shell.lastJoin, QStringLiteral("new:mc.hypixel.net"));
		QCOMPARE(evaluate(root.get(), QStringLiteral("serverJoin.instanceName(%1)")
										  .arg(hypixel))
					 .toString(),
				 QStringLiteral("NEW"));

		shell.lastJoin.clear();
		root->setProperty("selectedId", QStringLiteral("old"));
		QVERIFY(evaluate(root.get(), QStringLiteral("serverJoin.instanceName(%1)")
										 .arg(hypixel))
					.toString()
					.isEmpty());
		evaluate(root.get(), QStringLiteral("serverJoin.join(%1)").arg(hypixel));
		QVERIFY(shell.lastJoin.isEmpty());
		// The flow's only child is the Loader of its dialog.
		auto* dialog = qvariant_cast<QObject*>(
			evaluate(root.get(), QStringLiteral("serverJoin.children[0].item")));
		QVERIFY(dialog);
		QVERIFY(dialog->property("visible").toBool());
		QCOMPARE(dialog->property("none").toBool(), false);
		QCOMPARE(dialog->property("rows").toList().size(), 1);
		QCOMPARE(qvariant_cast<QVariantMap>(dialog->property("refused"))
					 .value("id")
					 .toString(),
				 QStringLiteral("old"));
		QCOMPARE(dialog->property("range").toString(),
				 QStringLiteral("1.8 or newer"));
	}

	/* The quick-join bar shows the servers that fit its width and moves the
	 * rest into its ⋯ menu. */
	void quickJoinBarMovesWhatDoesNotFitIntoItsMenu()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			QuickJoinBar {
				servers: [0, 1, 2, 3, 4, 5].map((i) => ({ id: "s" + i, name: "Server " + i,
					address: "s" + i + ".example", note: "", minVersion: "", maxVersion: "" }))
			})",
						  QUrl());
		std::unique_ptr<QObject> bar(component.create());
		QVERIFY2(bar, qPrintable(component.errorString()));
		const int servers = 6;

		const int roomy = 10000;
		bar->setProperty("width", roomy);
		QTRY_VERIFY(bar->property("fullWidth").toInt() > 0);
		QTRY_COMPARE(bar->property("fitCount").toInt(), servers);
		QCOMPARE(bar->property("hidden").toList().size(), 0);
		const int full = bar->property("fullWidth").toInt();

		bar->setProperty("width", full / 2);
		QTRY_VERIFY(bar->property("fitCount").toInt() < servers);
		const int shown = bar->property("fitCount").toInt();
		QVERIFY(shown > 0);
		QCOMPARE(bar->property("hidden").toList().size(), servers - shown);
		// Sized by every entry, so moving some into the menu does not shrink it.
		QCOMPARE(bar->property("fullWidth").toInt(), full);
	}

	/* Where a friend plays: a big server by its name, any other by its
	 * address, the favicon once the server answers; servers are pinged only
	 * while a view shows them, each once. */
	void friendPlacesNameServersAndPingWhileShown()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			FriendPlaces {
				id: places
				property var asked: []
				featured: [{ id: "hypixel", name: "Hypixel", address: "mc.hypixel.net" }]
				friends: [
					{ name: "jeb_", state: "playing", worldType: "server", host: "mc.hypixel.net", port: 25565 },
					{ name: "Kai", state: "playing", worldType: "server", host: "smp.example.com", port: 25570 },
					{ name: "Alex", state: "playing", worldType: "server", host: "mc.hypixel.net", port: 0 },
					{ name: "Dinnerbone", state: "playing", worldType: "singleplayer", host: "", port: 0 },
					{ name: "Notch", state: "online", worldType: "", host: "", port: 0 }
				]
				pinger: QtObject {
					property int revision: 0
					function ping(address) { places.asked.push(address) }
					function status(address) {
						return address === "mc.hypixel.net" ? { state: "online", online: 26776, favicon: "data:x" }
															: { state: "pending" }
					}
					function friendsOn(address, people) {
						return people.filter((one) => one.host === address)
					}
				}
				function nameOfIndex(i) { return of(friends[i]).name || "" }
				function faviconOfIndex(i) { return of(friends[i]).favicon || "" }
				function onlineOfIndex(i) { return of(friends[i]).online }
			})",
						  QUrl());
		std::unique_ptr<QObject> places(component.create());
		QVERIFY2(places, qPrintable(component.errorString()));

		auto call = [&](const char* method, int index) {
			QVariant result;
			QMetaObject::invokeMethod(places.get(), method, Q_RETURN_ARG(QVariant, result),
									  Q_ARG(QVariant, index));
			return result;
		};
		QCOMPARE(call("nameOfIndex", 0).toString(), QStringLiteral("Hypixel"));
		QCOMPARE(call("faviconOfIndex", 0).toString(), QStringLiteral("data:x"));
		QCOMPARE(call("onlineOfIndex", 0).toInt(), 26776);
		// Not a server Kite knows, and no answer yet.
		QCOMPARE(call("nameOfIndex", 1).toString(), QStringLiteral("smp.example.com:25570"));
		QCOMPARE(call("faviconOfIndex", 1).toString(), QString());
		QCOMPARE(call("onlineOfIndex", 1).toInt(), -1);
		// Not on a server whose address they show.
		QCOMPARE(call("nameOfIndex", 3).toString(), QString());

		QCOMPARE(places->property("asked").toList().size(), 0);
		places->setProperty("active", true);
		const QStringList asked = places->property("asked").toStringList();
		QCOMPARE(asked, QStringList({ QStringLiteral("mc.hypixel.net"), QStringLiteral("smp.example.com:25570") }));
	}

	/* The strip lifts whoever waits on you (an invite, unread messages)
	 * above the groups, which keep their order: in game, in Kite, offline. */
	void friendsStripPutsWhoWaitsOnYouFirst()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(QStringLiteral(R"(
			import QtQuick
			import Kite.Components
			FriendsPanelStrip {
				width: 64
				height: 600
				friendsState: FriendsState { controller: %1 }
				function names() { return ordered.map((one) => one.name).join(",") }
				function groups() { return ordered.map((one) => groupOf(one)).join(",") }
			})")
							  .arg(fakeFriends(QStringLiteral(R"([
				{ uuid: "a", name: "Ann", face: "", state: "offline", statusText: "Offline" },
				{ uuid: "b", name: "Bea", face: "", state: "playing", worldType: "singleplayer", version: "1.21.4" },
				{ uuid: "c", name: "Cem", face: "", state: "online", statusText: "In Kite", unread: 2 },
				{ uuid: "d", name: "Dan", face: "", state: "online", statusText: "In Kite" },
				{ uuid: "e", name: "Eda", face: "", state: "offline", statusText: "Offline" }])"),
											   QStringLiteral(R"([{ uuid: "e", name: "Eda", host: "mc.example.net",
				port: 25565, expiresAt: "" }])")))
							  .toUtf8(),
						  QUrl());
		std::unique_ptr<QObject> strip(component.create());
		QVERIFY2(strip, qPrintable(component.errorString()));
		QCOMPARE(callString(strip.get(), "names"), QStringLiteral("Cem,Eda,Bea,Dan,Ann"));
		QCOMPARE(callString(strip.get(), "groups"), QStringLiteral("0,0,1,2,3"));
	}

	/* A friend's card says where they play: a server by its name with its
	 * players and the game version, singleplayer and a server they keep to
	 * themselves in words; nothing while they are not in a game. */
	void friendCardSaysWhereAFriendPlays()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			Item {
				FriendPlaceBlock {
					id: server
					friend: ({ name: "jeb_", state: "playing", worldType: "server", host: "mc.hypixel.net",
							   version: "1.21.4" })
					place: ({ address: "mc.hypixel.net", name: "Hypixel", favicon: "", online: 812 })
				}
				FriendPlaceBlock {
					id: single
					friend: ({ state: "playing", worldType: "singleplayer", version: "1.21.4" })
				}
				FriendPlaceBlock {
					id: hidden
					friend: ({ state: "playing", worldType: "server", version: "1.20.1" })
				}
				FriendPlaceBlock {
					id: away
					friend: ({ state: "offline" })
				}
				function lines() {
					return [server.title, server.facts, single.title, single.facts, hidden.title, away.visible].join("|")
				}
			})",
						  QUrl());
		std::unique_ptr<QObject> blocks(component.create());
		QVERIFY2(blocks, qPrintable(component.errorString()));
		QCOMPARE(callString(blocks.get(), "lines"),
				 QStringLiteral("Hypixel|812 online · 1.21.4|Singleplayer|1.21.4|On a server|false"));
	}

	/* The words of the friends views: unread messages with a real plural,
	 * an invite's time left (never under a minute), a friend's presence. */
	void friendsWordsCountAndLast()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			QtObject {
				function words() {
					var soon = new Date(Date.now() + 8 * 60000 - 5000).toISOString()
					var gone = new Date(Date.now() - 1000).toISOString()
					return [Format.unreadMessages(0), Format.unreadMessages(1), Format.unreadMessages(3),
							Format.inviteTimeLeft({ expiresAt: soon }), Format.inviteTimeLeft({ expiresAt: gone }),
							Format.friendPresence({ state: "playing" }).text,
							Format.friendPresence({ state: "online", statusText: "In Kite" }).tone,
							Format.friendPresence({ state: "offline", statusText: "Last seen yesterday" }).tone].join("|")
				}
			})",
						  QUrl());
		std::unique_ptr<QObject> words(component.create());
		QVERIFY2(words, qPrintable(component.errorString()));
		QCOMPARE(callString(words.get(), "words"),
				 QStringLiteral("|1 unread message|3 unread messages|8 min left|1 min left|In game|success|tertiary"));
	}

	/* A folder path counts as absolute with a Windows drive too, and becomes a
	 * url the folder dialog can start from. */
	void windowsDrivePathsAreAbsolute()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			QtObject {
				function words() {
					return ["/home/a", "C:/Games/Kite", "C:\\Games", "\\\\nas\\kite", "instances", "C:kite", ""]
						.map(p => Format.isAbsolutePath(p)).join("|") + "|" + Format.fileUrl("C:\\Games\\Kite")
				}
			})",
						  QUrl());
		std::unique_ptr<QObject> words(component.create());
		QVERIFY2(words, qPrintable(component.errorString()));
		QCOMPARE(callString(words.get(), "words"),
				 QStringLiteral("true|true|true|true|false|false|false|file:///C:/Games/Kite"));
	}

	/* Join on an invite goes to the invite's server through ServerJoinFlow,
	 * whatever the inviter plays now; a big server Kite knows lends its name
	 * and versions; a home-network address is confirmed first. */
	void inviteJoinGoesToTheInvitedServer()
	{
		QQmlEngine engine;
		FakeShell shell;
		shell.serverInstances = {QVariantMap{{"id", "new"},		 {"name", "NEW"},
											 {"iconKey", ""},	 {"version", "1.21.4"},
											 {"loader", ""},	 {"lastLaunch", 0},
											 {"running", false}, {"canLaunch", true},
											 {"fits", true}}};
		auto root = createRoot(engine, &shell);
		QVERIFY(root);
		root->setProperty("selectedId", QStringLiteral("new"));
		evaluate(root.get(), QStringLiteral(
			"friendJoin.featured = [{ id: 'hypixel', name: 'Hypixel', address: 'mc.hypixel.net', "
			"minVersion: '1.8', maxVersion: '', versions: [] }]"));
		QCOMPARE(evaluate(root.get(), QStringLiteral(
			"friendJoin.inviteServer({ host: 'mc.hypixel.net', port: 25565 }).name")).toString(),
				 QStringLiteral("Hypixel"));
		QCOMPARE(evaluate(root.get(), QStringLiteral(
			"friendJoin.inviteServer({ host: 'play.other.net', port: 25570 }).address")).toString(),
				 QStringLiteral("play.other.net:25570"));
		QCOMPARE(evaluate(root.get(), QStringLiteral(
			"friendJoin.inviteTip({ host: 'mc.hypixel.net', port: 25565 })")).toString(),
				 QStringLiteral("Start NEW and join Hypixel"));

		evaluate(root.get(), QStringLiteral(
			"friendJoin.joinInvite({ name: 'Ann', host: 'mc.hypixel.net', port: 25565, addressKind: 'domain' })"));
		QCOMPARE(shell.lastJoin, QStringLiteral("new:mc.hypixel.net"));

		shell.lastJoin.clear();
		evaluate(root.get(), QStringLiteral(
			"friendJoin.joinInvite({ name: 'Ann', host: '192.168.1.20', port: 25565, addressKind: 'local' })"));
		QVERIFY(shell.lastJoin.isEmpty());
		// The flow's first child is the Loader of the address question.
		auto* confirm = qvariant_cast<QObject*>(
			evaluate(root.get(), QStringLiteral("friendJoin.children[0].item")));
		QVERIFY(confirm);
		QVERIFY(confirm->property("visible").toBool());
		QCOMPARE(confirm->property("server").toMap().value("address").toString(),
				 QStringLiteral("192.168.1.20"));
		QVERIFY(QMetaObject::invokeMethod(confirm, "confirmed"));
		QCOMPARE(shell.lastJoin, QStringLiteral("new:192.168.1.20"));
	}

	/* The invite picker marks who is already invited to this server, and
	 * sends one invite at a time. */
	void invitePickerKnowsWhoIsInvited()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(QStringLiteral(R"(
			import QtQuick
			import Kite.Components
			FriendInvitePicker {
				controller: QtObject {
					property bool enabled: true
					property string state: "ready"
					property bool loaded: true
					property var friends: [{ uuid: "a", name: "Ann", state: "online" }, { uuid: "b", name: "Bea", state: "offline" }]
					property var outgoingInvites: [{ uuid: "a", name: "Ann", host: "mc.example.net", port: 25565 }]
					property var invites: []
					property var incoming: []
					property int onlineCount: 1
					property int incomingCount: 0
					property string sent: ""
					function sendInvite(uuid, address) { sent = uuid + "@" + address }
					signal changed()
					signal actionFinished(string action, bool ok, string message)
				}
				server: ({ name: "Example", address: "MC.Example.net:25565" })
				function words() {
					return [invitedHere("a"), invitedHere("b"), everyone.length].join(",")
				}
			})").toUtf8(),
						  QUrl());
		std::unique_ptr<QObject> picker(component.create());
		QVERIFY2(picker, qPrintable(component.errorString()));
		QCOMPARE(callString(picker.get(), "words"), QStringLiteral("true,false,2"));
		QVERIFY(QMetaObject::invokeMethod(picker.get(), "invite", Q_ARG(QVariant, QStringLiteral("b"))));
		QCOMPARE(picker->property("sendingUuid").toString(), QStringLiteral("b"));
		auto* controller = qvariant_cast<QObject*>(picker->property("controller"));
		QCOMPARE(controller->property("sent").toString(), QStringLiteral("b@MC.Example.net:25565"));
		QMetaObject::invokeMethod(controller, "actionFinished", Q_ARG(QString, QStringLiteral("sendInvite")),
								  Q_ARG(bool, true), Q_ARG(QString, QString()));
		QVERIFY(picker->property("sendingUuid").toString().isEmpty());
	}

	/* A share code's contents in words: singular and plural by count. */
	void shareCodeSummaryWords()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			ShareCodeSummary {
				function words() {
					return [contentText({ mods: 1, resourcePacks: 2, shaderPacks: 1 }), contentText({}),
							versionText({ minecraftVersion: "1.21.4", loader: "fabric", loaderVersion: "0.16.10" }),
							versionText({ minecraftVersion: "1.21.4", loader: "vanilla" })].join("|")
				}
			})",
						  QUrl());
		std::unique_ptr<QObject> summary(component.create());
		QVERIFY2(summary, qPrintable(component.errorString()));
		QCOMPARE(callString(summary.get(), "words"),
				 QStringLiteral("1 mod · 2 resource packs · 1 shader pack|No mods or packs|"
								"Minecraft 1.21.4 · Fabric 0.16.10|Minecraft 1.21.4"));
	}

	/* A quick-join entry's tooltip says what a click does and what is
	 * live there. */
	void quickJoinEntryTipSaysWhatAClickDoes()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			QuickJoinEntry {
				server: ({ id: "hypixel", name: "Hypixel", address: "mc.hypixel.net",
						   minVersion: "1.8", maxVersion: "", versions: [] })
				status: ({ state: "online", online: 27000, max: 200000, ping: 160, favicon: "" })
				friends: [{ name: "Steve", face: "" }, { name: "Alex", face: "" }]
				joinFlow: QtObject {
					function instanceName(server) { return "Hardcore Survival" }
				}
			})",
						  QUrl());
		std::unique_ptr<QObject> entry(component.create());
		QVERIFY2(entry, qPrintable(component.errorString()));
		QVariant tip;
		QVERIFY(QMetaObject::invokeMethod(entry.get(), "tipText",
										  Q_RETURN_ARG(QVariant, tip)));
		const QStringList lines = tip.toString().split(QLatin1Char('\n'));
		QCOMPARE(lines.size(), 3);
		QCOMPARE(lines.at(0), QStringLiteral("Join Hypixel with Hardcore Survival"));
		QVERIFY(lines.at(1).endsWith(QStringLiteral(" online · 160 ms")));
		QCOMPARE(lines.at(2), QStringLiteral("Steve and Alex are here"));
	}

	/* The macOS menu bar shows only natively (Cocoa), not in this offscreen
	 * test. */
	void nativeMenuBarStaysOffOutsideCocoa()
	{
		QQmlEngine engine;
		QObject shell;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);
		QVERIFY(!root->property("nativeMenuBar").toBool());
		QVERIFY(!evaluate(root.get(), QStringLiteral("root.menuBar.visible")).toBool());
	}

	/* "Installed plugins" draws one row per plugin of its model, with the
	 * model's own roles. */
	void pluginListSectionDrawsARowPerPlugin()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		// Plain literals: moc stops parsing the rest of this file at a raw
		// string.
		component.setData(
			"import QtQuick\n"
			"import Kite.Components\n"
			"SettingsPluginsListSection {\n"
			"  width: 800; height: 600\n"
			"  pluginList: ListModel {\n"
			"    ListElement { name: 'World Share'; version: '1.0'; author: 'Steve';\n"
			"      description: 'Shares worlds.'; license: 'MIT'; sourceUrl: '';\n"
			"      signature: 'Signed'; status: 'Loaded'; statusDetail: '';\n"
			"      enabled: true; canToggle: true; dependencies: '' }\n"
			"    ListElement { name: 'Old Thing'; version: ''; author: '';\n"
			"      description: ''; license: ''; sourceUrl: 'https://example.org';\n"
			"      signature: 'Unsigned'; status: 'Missing dependency';\n"
			"      statusDetail: 'Needs Core'; enabled: false; canToggle: true;\n"
			"      dependencies: 'Core >= 2.0' }\n"
			"  }\n"
			"}\n",
			QUrl(QStringLiteral("qrc:/test/PluginList.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> section(component.create());
		QVERIFY2(section, qPrintable(component.errorString()));
		QVERIFY(section->property("shown").toBool());
		QCOMPARE(section->property("count").toInt(), 2);
		// Every row is a SettingRow with its plugin's name and version;
		// Repeater rows live in the visual tree, not among QObject children.
		int rows = 0;
		std::function<void(QQuickItem*)> visit = [&](QQuickItem* item) {
			const QString label = item->property("label").toString();
			if (label == QStringLiteral("World Share 1.0") ||
				label == QStringLiteral("Old Thing"))
				++rows;
			for (QQuickItem* child : item->childItems())
				visit(child);
		};
		visit(qobject_cast<QQuickItem*>(section.get()));
		QCOMPARE(rows, 2);
	}

	/* A switch keeps showing what the model says after a click: a plugin may
	 * refuse, or apply the change only at the next start. */
	void pluginSwitchesKeepFollowingTheirModel()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(
			"import QtQuick\n"
			"import Kite.Components\n"
			"SettingsPluginsListSection {\n"
			"  width: 800; height: 600\n"
			"  pluginList: ListModel {\n"
			"    function setEnabled(row, on) { /* refused: model unchanged */ }\n"
			"    ListElement { name: 'World Share'; version: '1.0'; author: '';\n"
			"      description: ''; license: ''; sourceUrl: '';\n"
			"      signature: 'Signed'; status: 'Loaded'; statusDetail: '';\n"
			"      enabled: true; canToggle: true; dependencies: '' }\n"
			"  }\n"
			"}\n",
			QUrl(QStringLiteral("qrc:/test/PluginSwitch.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> section(component.create());
		QVERIFY2(section, qPrintable(component.errorString()));

		QQuickItem* sw = nullptr;
		std::function<void(QQuickItem*)> find = [&](QQuickItem* item) {
			for (const QMetaObject* mo = item->metaObject(); mo && !sw;
				 mo = mo->superClass()) {
				if (QByteArray(mo->className()) == "QQuickSwitch")
					sw = item;
			}
			for (QQuickItem* child : item->childItems())
				find(child);
		};
		find(qobject_cast<QQuickItem*>(section.get()));
		QVERIFY(sw);
		QVERIFY(sw->property("checked").toBool());

		// The user clicks; the model does not change.
		QVERIFY(QMetaObject::invokeMethod(sw, "click"));
		QVERIFY(sw->property("checked").toBool());
	}

	/* The skin editor's arm width radios show what the skin says after a
	 * click: a click only asks the library for the change (refused first, then
	 * taken), and on a default skin it picks the other texture instead. */
	void skinArmsRadiosFollowTheSkin()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(
			"import QtQuick\n"
			"import QtQuick.Controls\n"
			"import Kite.Components\n"
			"ApplicationWindow {\n"
			"  width: 1000; height: 900\n"
			"  property alias editor: editor\n"
			"  property alias library: library\n"
			"  QtObject { id: library\n"
			"    property bool accept: false\n"
			"    property int asked: 0\n"
			"    property var skins: [{ name: 'Hero', path: '/hero.png', textureUrl: '',\n"
			"      thumbnailUrl: '', bodyUrl: '', slim: false, worn: true }]\n"
			"    property var defaultSkins: [{ name: 'Alex', slim: true, variants: [\n"
			"      { name: 'Alex', path: '/alex-classic.png', slim: false, ready: true, worn: false },\n"
			"      { name: 'Alex', path: '/alex-slim.png', slim: true, ready: true, worn: false } ] }]\n"
			"    property bool defaultSkinsLoading: false\n"
			"    function setSlim(name, slim) {\n"
			"      asked++\n"
			"      if (accept) skins = [Object.assign({}, skins[0], { slim: slim })]\n"
			"    }\n"
			"    function markWorn(name) {}\n"
			"  }\n"
			"  QtObject { id: accountsStub\n"
			"    signal defaultSkinsRevisionChanged()\n"
			"    property var accounts: null\n"
			"    property string skinDemoPose: ''\n"
			"    function accountSkinInfo(row) { return { valid: true, slim: false,\n"
			"      hasSkin: false, currentCapeId: '', currentCapeUrl: '', capes: [] } }\n"
			"    function skins(row) { return library }\n"
			"  }\n"
			"  SkinCapeEditor { id: editor; controller: accountsStub; row: -1 }\n"
			"}\n",
			QUrl(QStringLiteral("qrc:/test/SkinArms.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> root(component.create());
		QVERIFY2(root, qPrintable(component.errorString()));
		auto* window = qobject_cast<QQuickWindow*>(root.get());
		QVERIFY(window);
		window->show();
		QObject* editor = root->property("editor").value<QObject*>();
		QObject* library = root->property("library").value<QObject*>();
		QVERIFY(editor && library);
		QVERIFY(QMetaObject::invokeMethod(editor, "open"));
		QTRY_VERIFY(editor->property("opened").toBool());
		// Row -1 opens on its first skin.
		QCOMPARE(editor->property("selectedName").toString(), QStringLiteral("Hero"));

		QQuickItem* classic = nullptr;
		QQuickItem* slim = nullptr;
		std::function<void(QQuickItem*)> find = [&](QQuickItem* item) {
			for (const QMetaObject* mo = item->metaObject(); mo;
				 mo = mo->superClass()) {
				if (QByteArray(mo->className()) == "QQuickRadioButton") {
					const QString text = item->property("text").toString();
					if (text == QStringLiteral("Classic"))
						classic = item;
					else if (text == QStringLiteral("Slim"))
						slim = item;
					break;
				}
			}
			for (QQuickItem* child : item->childItems())
				find(child);
		};
		QQuickItem* top = window->contentItem();
		while (top->parentItem())
			top = top->parentItem();
		find(top);
		QVERIFY(classic && slim);
		auto shows = [&](bool wantSlim) {
			return classic->property("checked").toBool() == !wantSlim &&
				   slim->property("checked").toBool() == wantSlim;
		};
		QVERIFY(shows(false));

		// Refused: the radios stay on what the skin says.
		QVERIFY(QMetaObject::invokeMethod(slim, "click"));
		QCOMPARE(library->property("asked").toInt(), 1);
		QVERIFY(shows(false));

		// Taken: they follow the skin there, and back.
		library->setProperty("accept", true);
		QVERIFY(QMetaObject::invokeMethod(slim, "click"));
		QVERIFY(shows(true));
		QVERIFY(QMetaObject::invokeMethod(classic, "click"));
		QVERIFY(shows(false));
		QCOMPARE(library->property("asked").toInt(), 3);

		// A default skin shows its own width (Alex is slim); the radios swap
		// its texture and leave the library alone.
		evaluate(editor, QStringLiteral("selectDefault(skins.defaultSkins[0])"));
		auto selectedPath = [&] {
			return evaluate(editor, QStringLiteral("selected ? selected.path : ''"))
				.toString();
		};
		QCOMPARE(selectedPath(), QStringLiteral("/alex-slim.png"));
		QVERIFY(shows(true));
		QVERIFY(QMetaObject::invokeMethod(classic, "click"));
		QCOMPARE(selectedPath(), QStringLiteral("/alex-classic.png"));
		QVERIFY(shows(false));
		QCOMPARE(library->property("asked").toInt(), 3);
	}

	/* QmlShell.news reaches the Home page's welcome card. */
	void homeShowsTheNewestHeadline()
	{
		QQmlEngine engine;
		FakeShell shell;
		shell.m_news.setEntries({ std::make_shared<NewsEntry>(
			QStringLiteral("Kite 7 is out"), QString(),
			QStringLiteral("https://example.org/7"), QString(), QDateTime()) });
		auto root = createRoot(engine, &shell);
		QVERIFY(root);
		// HomePage reads the model's rows once they settle (Qt.callLater).
		QTRY_COMPARE(evaluate(root.get(), QStringLiteral("homePage.hasNews ? homePage.featuredNews.title : ''"))
						 .toString(),
					 QStringLiteral("Kite 7 is out"));
	}

	/* A release post for the running version (or older) is skipped, and nothing
	 * older than newsMaxAgeDays gets a banner; the feed is newest first, so a
	 * stale entry ends the search. */
	void homeSkipsOutdatedReleasesAndStaleNews()
	{
		const QString runningVersion = QCoreApplication::applicationVersion();
		QCoreApplication::setApplicationVersion(QStringLiteral("11.0.0"));
		const QDateTime now = QDateTime::currentDateTimeUtc();
		auto post = [](const QString& title, const QDateTime& date) {
			return std::make_shared<NewsEntry>(title, QString(), QStringLiteral("https://example.org/post"),
											   QString(), date);
		};

		{
			QQmlEngine engine;
			FakeShell shell;
			shell.m_news.setEntries({ post(QStringLiteral("Kite Release 12.1.0, now available"), now),
									  post(QStringLiteral("Kite Release 11.0.0"), now.addDays(-1)) });
			auto root = createRoot(engine, &shell);
			QVERIFY(root);
			QTRY_COMPARE(evaluate(root.get(), QStringLiteral("homePage.hasNews ? homePage.featuredNews.title : ''"))
							 .toString(),
						 QStringLiteral("Kite Release 12.1.0, now available"));
		}
		{
			QQmlEngine engine;
			FakeShell shell;
			shell.m_news.setEntries({ post(QStringLiteral("Kite Release 11.0.0"), now),
									  post(QStringLiteral("Kite Release 9.1.0, now available"), now.addDays(-2)),
									  post(QStringLiteral("Welcome to Kite"), now.addDays(-200)) });
			auto root = createRoot(engine, &shell);
			QVERIFY(root);
			QTRY_COMPARE(evaluate(root.get(), QStringLiteral("homePage.newsEntries.length")).toInt(), 3);
			QCOMPARE(evaluate(root.get(), QStringLiteral("homePage.hasNews")).toBool(), false);
		}
		QCoreApplication::setApplicationVersion(runningVersion);
	}

	/* `kite --import` and the IPC "import" message reach QML as
	 * importRequested(): the New instance dialog opens in import mode with the
	 * source filled in. The stand-in controller is a bare QObject, so the dialog
	 * logs TypeErrors for the members it reads; only the two properties checked
	 * here matter. */
	void importRequestOpensNewInstanceDialogInImportMode()
	{
		QQmlEngine engine;
		FakeShell shell;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);

		const QString source = QStringLiteral("/tmp/Some Pack.mrpack");
		emit shell.importRequested(source);

		QCOMPARE(evaluate(root.get(), QStringLiteral("newInstanceDialog.mode"))
					 .toString(),
				 QStringLiteral("import"));
		QCOMPARE(evaluate(root.get(),
						  QStringLiteral("newInstanceDialog.presetSource"))
					 .toString(),
				 source);
	}

	std::unique_ptr<QObject> createFlow(QQmlEngine& engine)
	{
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData("import QtQuick\n"
						  "import Kite.Components\n"
						  "ContentDropFlow {}\n",
						  QUrl(QStringLiteral("qrc:/test/Flow.qml")));
		if (!component.isReady()) {
			qWarning() << component.errorString();
			return nullptr;
		}
		return std::unique_ptr<QObject>(component.create());
	}

	void handle(QObject* flow, FakeDrop* drop, const QStringList& urls,
				const QVariant& gate = QVariant())
	{
		QVERIFY(QMetaObject::invokeMethod(
			flow, "handle", Q_ARG(QVariant, QVariant::fromValue<QObject*>(drop)),
			Q_ARG(QVariant, QVariant(urls)), Q_ARG(QVariant, gate)));
	}

	/* Dragging or double-clicking the top bar reaches the window (it used to
	 * fail with "Cannot call method 'startSystemMove' of null"). */
	void windowDragBandReachesItsWindow()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData("import QtQuick\n"
						  "import Kite.Components\n"
						  "Window {\n"
						  "  width: 300; height: 200; visible: true\n"
						  "  WindowDragBand { anchors.fill: parent }\n"
						  "}\n",
						  QUrl(QStringLiteral("qrc:/test/Band.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> object(component.create());
		auto* window = qobject_cast<QQuickWindow*>(object.get());
		QVERIFY(window);
		QVERIFY(QTest::qWaitForWindowExposed(window));

		QTest::failOnWarning(QRegularExpression(QStringLiteral("TypeError")));
		const QPoint at(150, 100);
		QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier, at);
		QTest::mousePress(window, Qt::LeftButton, Qt::NoModifier, at);
		QTest::mouseMove(window, at + QPoint(40, 5));
		QTest::mouseRelease(window, Qt::LeftButton, Qt::NoModifier, at + QPoint(40, 5));
	}

	/* The "From another launcher" pane against a stand-in controller: its
	 * footer line counts the pick (singular and plural), Import is off when the
	 * copies do not fit, and start() hands over to the controller. */
	void launcherImportPaneFollowsItsController()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(
			"import QtQuick\n"
			"import Kite.Components\n"
			"Item {\n"
			"  width: 800; height: 600\n"
			"  property alias pane: pane\n"
			"  property alias fake: fake\n"
			"  QtObject { id: fake\n"
			"    property bool running: false; property bool scanning: false; property bool scanned: true\n"
			"    property int availableCount: 2; property var unsupported: ['ATLauncher']; property var unreadable: []\n"
			"    property int selectedCount: 1; property bool allSelected: false; property bool anyLinked: false\n"
			"    property int linkedCount: 0; property int foundCount: 2\n"
			"    property string selectedSizeText: '1.2 GB'; property string freeSpaceText: '10 GB'\n"
			"    property string space: 'ok'; property real progress: -1; property string status: ''\n"
			"    property int importedCount: 0; property int failedCount: 0; property var errors: []\n"
			"    property int started: 0\n"
			"    property var model: ListModel { ListElement { key: 'prism:a'; launcher: 'prism';\n"
			"      launcherName: 'Prism Launcher'; name: 'Pack'; minecraftVersion: '1.20.1'; loader: 'fabric';\n"
			"      loaderVersion: '0.15.11'; sizeText: '1.2 GB'; lastPlayed: 0; checked: true; linked: false;\n"
			"      linkable: true; linkBlocker: ''; problem: ''; imported: false; leftBehind: '' } }\n"
			"    function scan() {} function setChecked(k, c) {} function setAllChecked(c) {}\n"
			"    function setMode(k, m) {} function openFolder(k) {} function cancel() {}\n"
			"    function start() { started++; running = true; return true }\n"
			"  }\n"
			"  LauncherImportPane { id: pane; anchors.fill: parent; controller: fake }\n"
			"}\n",
			QUrl(QStringLiteral("qrc:/test/LauncherImport.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		QTest::failOnWarning(QRegularExpression(QStringLiteral("TypeError|ReferenceError")));
		std::unique_ptr<QObject> root(component.create());
		QVERIFY2(root, qPrintable(component.errorString()));
		auto* pane = root->property("pane").value<QObject*>();
		auto* fake = root->property("fake").value<QObject*>();
		QVERIFY(pane && fake);

		QCOMPARE(pane->property("summary").toString(),
				 QStringLiteral("1 instance selected, 1.2 GB to copy"));
		QVERIFY(pane->property("canImport").toBool());
		fake->setProperty("selectedCount", 3);
		QCOMPARE(pane->property("summary").toString(),
				 QStringLiteral("3 instances selected, 1.2 GB to copy"));
		fake->setProperty("linkedCount", 1);
		QCOMPARE(pane->property("summary").toString(),
				 QStringLiteral("3 instances selected, 1.2 GB to copy, 1 linked"));
		fake->setProperty("selectedCount", 1);
		QCOMPARE(pane->property("summary").toString(), QStringLiteral("1 instance selected, 1 linked"));
		fake->setProperty("linkedCount", 0);
		fake->setProperty("space", QStringLiteral("short"));
		QVERIFY(!pane->property("canImport").toBool());
		fake->setProperty("space", QStringLiteral("ok"));

		QVERIFY(QMetaObject::invokeMethod(pane, "start"));
		QCOMPARE(fake->property("started").toInt(), 1);
		QVERIFY(pane->property("running").toBool());
		QVERIFY(!pane->property("canImport").toBool());
		QCOMPARE(pane->property("summary").toString(), QString());
	}

	/* The overlay's card says "Drop to add" and one line per kind dragged; the
	 * data pack line adds that a world will be asked for. */
	void dropOverlayNamesWhatIsDragged()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData("import QtQuick\n"
						  "import Kite.Components\n"
						  "DropOverlay { width: 600; height: 400 }\n",
						  QUrl(QStringLiteral("qrc:/test/Overlay.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> overlay(component.create());
		QVERIFY2(overlay, qPrintable(component.errorString()));

		QCOMPARE(overlay->property("title").toString(),
				 QStringLiteral("Drop to add"));

		QVariantMap mods;
		mods.insert(QStringLiteral("kind"), QStringLiteral("mods"));
		mods.insert(QStringLiteral("text"), QStringLiteral("2 mods"));
		QVariantMap data;
		data.insert(QStringLiteral("kind"), QStringLiteral("datapacks"));
		data.insert(QStringLiteral("text"), QStringLiteral("1 data pack"));
		overlay->setProperty("rows", QVariantList{mods, data});
		overlay->setProperty("unknown",
							 QStringLiteral("1 file Kite doesn't recognise"));

		// Every line is drawn: the title, both rows, the world hint and the
		// unknown-file line.
		QStringList texts;
		std::function<void(QQuickItem*)> visit = [&](QQuickItem* item) {
			const QString text = item->property("text").toString();
			if (!text.isEmpty() && item->isVisible())
				texts << text;
			for (QQuickItem* child : item->childItems())
				visit(child);
		};
		visit(qobject_cast<QQuickItem*>(overlay.get()));
		QVERIFY2(texts.contains(QStringLiteral("Drop to add")), qPrintable(texts.join('|')));
		QVERIFY(texts.contains(QStringLiteral("2 mods")));
		QVERIFY(texts.contains(QStringLiteral("1 data pack")));
		QVERIFY(texts.contains(QStringLiteral("1 file Kite doesn't recognise")));
		QVERIFY2(texts.filter(QStringLiteral("pick a world")).size() == 1,
				 qPrintable(texts.join('|')));
	}

	/* A drop with nothing to ask is installed at once and reported in one
	 * toast. */
	void aPlainDropIsInstalledAndReported()
	{
		QQmlEngine engine;
		auto flow = createFlow(engine);
		QVERIFY(flow);
		QSignalSpy notified(flow.get(), SIGNAL(notify(QString, QString)));
		FakeDrop drop;
		drop.plan = FakeDrop::quietPlan();

		handle(flow.get(), &drop, {QStringLiteral("file:///tmp/a.jar")});

		QCOMPARE(drop.prepared, QStringList{QStringLiteral("file:///tmp/a.jar")});
		QCOMPARE(drop.commits, 1);
		QCOMPARE(notified.count(), 1);
		QCOMPARE(notified.first().at(0).toString(), QStringLiteral("Added 1 mod."));
		QCOMPARE(notified.first().at(1).toString(), QStringLiteral("success"));
	}

	/* A modpack goes to Import, and a drop of nothing else does not install. */
	void aModpackGoesToImport()
	{
		QQmlEngine engine;
		auto flow = createFlow(engine);
		QVERIFY(flow);
		QSignalSpy imports(flow.get(), SIGNAL(importRequested(QString)));
		QSignalSpy notified(flow.get(), SIGNAL(notify(QString, QString)));
		FakeDrop drop;
		drop.plan = FakeDrop::quietPlan();
		drop.plan.insert(QStringLiteral("empty"), true);
		drop.plan.insert(QStringLiteral("modpack"), QStringLiteral("/tmp/pack.mrpack"));

		handle(flow.get(), &drop, {QStringLiteral("file:///tmp/pack.mrpack")});

		QCOMPARE(imports.count(), 1);
		QCOMPARE(imports.first().at(0).toString(), QStringLiteral("/tmp/pack.mrpack"));
		QCOMPARE(drop.commits, 0);
		QCOMPARE(notified.count(), 0);
	}

	/* A drop of files nobody recognises still answers, by naming them. */
	void unknownFilesStillGetAnAnswer()
	{
		QQmlEngine engine;
		auto flow = createFlow(engine);
		QVERIFY(flow);
		FakeDrop drop;
		drop.plan = FakeDrop::quietPlan();
		drop.plan.insert(QStringLiteral("empty"), true);
		drop.plan.insert(QStringLiteral("unknown"), QStringList{QStringLiteral("notes.txt")});

		handle(flow.get(), &drop, {QStringLiteral("file:///tmp/notes.txt")});

		QCOMPARE(drop.commits, 1);
	}

	/* An instance without a mod loader asks for one first; the install waits
	 * for the answer and goes ahead when the gate resumes it. */
	void aMissingLoaderIsAskedForBeforeInstalling()
	{
		QQmlEngine engine;
		auto flow = createFlow(engine);
		QVERIFY(flow);
		FakeDrop drop;
		drop.plan = FakeDrop::quietPlan();
		drop.plan.insert(QStringLiteral("needsLoader"), true);

		QQmlComponent holder(&engine);
		holder.setData("import QtQuick\n"
					   "QtObject {\n"
					   "  property var pending: null\n"
					   "  function gate(action) { pending = action; return true }\n"
					   "  function resume() { pending() }\n"
					   "}\n",
					   QUrl(QStringLiteral("qrc:/test/Gate.qml")));
		QVERIFY2(holder.isReady(), qPrintable(holder.errorString()));
		std::unique_ptr<QObject> gate(holder.create());
		QVERIFY(gate);
		QQmlExpression make(engine.rootContext(), gate.get(),
							QStringLiteral("gate"));
		handle(flow.get(), &drop, {QStringLiteral("file:///tmp/a.jar")},
			   make.evaluate());
		QCOMPARE(drop.commits, 0);

		QVERIFY(QMetaObject::invokeMethod(gate.get(), "resume"));
		QCOMPARE(drop.commits, 1);
	}

	/* Without a way to ask (a tile of the library), mods still go in and the
	 * toast says why they may not load. */
	void aMissingLoaderIsWarnedAboutWhenNobodyCanAsk()
	{
		QQmlEngine engine;
		auto flow = createFlow(engine);
		QVERIFY(flow);
		QSignalSpy notified(flow.get(), SIGNAL(notify(QString, QString)));
		FakeDrop drop;
		drop.plan = FakeDrop::quietPlan();
		drop.plan.insert(QStringLiteral("needsLoader"), true);

		handle(flow.get(), &drop, {QStringLiteral("file:///tmp/a.jar")});

		QCOMPARE(drop.commits, 1);
		QCOMPARE(notified.count(), 1);
		QVERIFY(notified.first().at(0).toString().contains(QStringLiteral("no mod loader")));
		QCOMPARE(notified.first().at(1).toString(), QStringLiteral("warning"));
	}

	/* A file dropped on the window opens import with it, unless a drop area
	 * inside a page (the Content tab's mods, the Worlds tab's worlds) takes it:
	 * Main.qml's window-wide one sits under all of them. The inner area here
	 * stands in for those, stacked as a page's would be. */
	void windowDropOpensImportUnlessAPageAreaTakesIt()
	{
		QQmlEngine engine;
		FakeShell shell;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);
		auto* window = qobject_cast<QQuickWindow*>(root.get());
		QVERIFY(window);
		window->show();
		QVERIFY(QTest::qWaitForWindowExposed(window));

		const QUrl file = QUrl::fromLocalFile(QStringLiteral("/tmp/Some Pack.mrpack"));
		const auto dropFile = [&]() {
			QMimeData mime;
			mime.setUrls({file});
			const QPoint pos(window->width() / 2, window->height() / 2);
			QDragEnterEvent enter(pos, Qt::CopyAction, &mime, Qt::LeftButton,
								  Qt::NoModifier);
			QCoreApplication::sendEvent(window, &enter);
			QDragMoveEvent move(pos, Qt::CopyAction, &mime, Qt::LeftButton,
								Qt::NoModifier);
			QCoreApplication::sendEvent(window, &move);
			QDropEvent drop(pos, Qt::CopyAction, &mime, Qt::LeftButton,
							Qt::NoModifier);
			QCoreApplication::sendEvent(window, &drop);
		};

		QQmlComponent component(&engine);
		component.setData("import QtQuick\n"
						  "DropArea {\n"
						  "  anchors.fill: parent\n"
						  "  keys: ['text/uri-list']\n"
						  "  property string received: ''\n"
						  "  onDropped: (drop) => {\n"
						  "    received = drop.urls[0].toString()\n"
						  "    drop.accept()\n"
						  "  }\n"
						  "}\n",
						  QUrl(QStringLiteral("qrc:/test/PageDropArea.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> pageArea(component.create());
		QVERIFY2(pageArea, qPrintable(component.errorString()));
		qobject_cast<QQuickItem*>(pageArea.get())
			->setParentItem(window->contentItem());

		dropFile();
		QCOMPARE(pageArea->property("received").toString(), file.toString());
		QVERIFY(!evaluate(root.get(), QStringLiteral("newInstanceDialog.visible"))
					 .toBool());

		pageArea.reset();
		dropFile();
		QVERIFY(evaluate(root.get(), QStringLiteral("newInstanceDialog.visible"))
					.toBool());
		QCOMPARE(evaluate(root.get(), QStringLiteral("newInstanceDialog.mode"))
					 .toString(),
				 QStringLiteral("import"));
		QCOMPARE(evaluate(root.get(),
						  QStringLiteral("newInstanceDialog.presetSource"))
					 .toString(),
				 file.toString());
	}

	/* Names from other people reach every one of these: each draws its text as
	 * written, tags and ampersand included, and a window showing them all
	 * renders. */
	void textViewsShowMarkupAsWritten()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import QtQuick.Controls
			import Kite.Components
			Window {
				width: 900; height: 900; visible: true
				readonly property string sample: "<b>x</b> & <i>y</i>"
				Column {
					Copy { objectName: "copy"; text: sample }
					Label { objectName: "label"; text: sample }
					Button { objectName: "button"; text: sample; icon.source: Icons.url("play") }
					ToolButton { objectName: "toolButton"; text: sample }
					ItemDelegate { objectName: "itemDelegate"; width: 400; text: sample }
					TabBar { TabButton { objectName: "tabButton"; text: sample } }
					CheckBox { objectName: "checkBox"; text: sample }
					RadioButton { objectName: "radioButton"; text: sample }
					Switch { objectName: "switch"; text: sample }
					ComboBox { objectName: "comboBox"; model: [sample] }
					TextField { objectName: "textField"; placeholderText: sample }
					TabListRow { objectName: "tabListRow"; width: 400; title: sample; meta: sample }
				}
				ToolTip { id: tip; objectName: "toolTip"; text: sample }
				Menu { id: menu; objectName: "menu"; MenuItem { objectName: "menuItem"; text: sample } }
				Component.onCompleted: { tip.open(); menu.popup(0, 0) }
			})",
						  QUrl(QStringLiteral("qrc:/test/Markup.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> object(component.create());
		auto* window = qobject_cast<QQuickWindow*>(object.get());
		QVERIFY2(window, qPrintable(component.errorString()));
		QVERIFY(QTest::qWaitForWindowExposed(window));
		const QString sample = window->property("sample").toString();

		const auto shownAsWritten = [&](QQuickItem* item, int expected) {
			const auto items = textItemsShowing(item, sample);
			if (items.size() != expected) {
				qWarning() << item << "draws the sample" << items.size() << "times";
				return false;
			}
			for (QQuickItem* text : items) {
				if (text->property("textFormat").toInt() != Qt::PlainText) {
					qWarning() << text << "under" << item << "is not plain text";
					return false;
				}
			}
			return true;
		};
		for (const char* name : {"copy", "label", "button", "toolButton", "itemDelegate", "tabButton",
								 "checkBox", "radioButton", "switch", "textField"}) {
			auto* item = window->findChild<QQuickItem*>(QLatin1String(name));
			QVERIFY2(item, name);
			QVERIFY2(shownAsWritten(item, 1), name);
		}
		QVERIFY(shownAsWritten(window->findChild<QQuickItem*>(QStringLiteral("tabListRow")), 2));
		auto* comboPopup = window->findChild<QQuickItem*>(QStringLiteral("comboBox"))
							   ->property("popup").value<QObject*>();
		QVERIFY(comboPopup && QMetaObject::invokeMethod(comboPopup, "open"));
		QTRY_VERIFY(comboPopup->property("opened").toBool());
		QTRY_VERIFY(shownAsWritten(comboPopup->property("contentItem").value<QQuickItem*>(), 1));
		auto* tip = window->findChild<QObject*>(QStringLiteral("toolTip"));
		QTRY_VERIFY(tip->property("opened").toBool());
		QVERIFY(shownAsWritten(tip->property("contentItem").value<QQuickItem*>(), 1));
		auto* menuItem = window->findChild<QQuickItem*>(QStringLiteral("menuItem"));
		QVERIFY(menuItem);
		QVERIFY(shownAsWritten(menuItem, 1));

		QVERIFY(!window->grabWindow().isNull());
	}

	/* The style's IconText sizes itself as Qt's IconLabel does in every
	 * display, including a button that starts icon-only and then shows its
	 * text (a toolbar that has room again). */
	void iconTextSizesLikeIconLabel()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import QtQuick.Controls.impl
			import QtQuick.Templates as T
			import Kite.Components
			import Kite.Style as KiteStyle
			Item {
				property int display: T.AbstractButton.IconOnly
				IconLabel {
					id: qt
					display: parent.display
					icon.source: Icons.url("folder"); icon.width: 16; icon.height: 16
					text: "Open folder"; spacing: 8; font.pixelSize: 14
				}
				KiteStyle.IconText {
					id: ours
					display: parent.display
					iconSource: Icons.url("folder"); iconWidth: 16; iconHeight: 16
					text: "Open folder"; spacing: 8; font.pixelSize: 14
				}
				readonly property size qtSize: Qt.size(qt.implicitWidth, qt.implicitHeight)
				readonly property size ourSize: Qt.size(ours.implicitWidth, ours.implicitHeight)
			})",
						  QUrl(QStringLiteral("qrc:/test/IconText.qml")));
		std::unique_ptr<QObject> item(component.create());
		QVERIFY2(item, qPrintable(component.errorString()));

		const int iconOnly = 0;
		const int textOnly = 1;
		const int textBesideIcon = 2;
		const int textUnderIcon = 3;
		for (const int display : {iconOnly, textBesideIcon, textOnly, iconOnly, textUnderIcon, textBesideIcon}) {
			item->setProperty("display", display);
			QTRY_VERIFY(item->property("qtSize").toSizeF().width() > 0);
			QTRY_COMPARE(item->property("ourSize").toSizeF(), item->property("qtSize").toSizeF());
		}
	}

	/* ExternalLinks lets only http, https and mail links out. One from the
	 * network asks first, naming the host it really leads to; Kite's own
	 * links open straight away. */
	void externalLinksOpenOnlyWebAndMailAndAskFirst()
	{
		LinkRecorder links;
		const QStringList schemes{QStringLiteral("http"), QStringLiteral("https"),
								  QStringLiteral("mailto"), QStringLiteral("kite-test")};
		for (const QString& scheme : schemes) {
			QDesktopServices::setUrlHandler(scheme, &links, "open");
		}
		const auto unset = qScopeGuard([&schemes] {
			for (const QString& scheme : schemes) {
				QDesktopServices::unsetUrlHandler(scheme);
			}
		});

		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			Window {
				width: 640; height: 480; visible: true
				Item { id: anchor; anchors.fill: parent }
				function open(url, trusted) { return ExternalLinks.open(anchor, url, trusted) }
				function destination(url) {
					const link = ExternalLinks.parse(url)
					return link ? link.destination : ""
				}
			})",
						  QUrl(QStringLiteral("qrc:/test/Links.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> object(component.create());
		auto* window = qobject_cast<QQuickWindow*>(object.get());
		QVERIFY2(window, qPrintable(component.errorString()));
		QVERIFY(QTest::qWaitForWindowExposed(window));

		const auto open = [&](const QString& url, bool trusted) {
			QVariant result;
			QMetaObject::invokeMethod(window, "open", Q_RETURN_ARG(QVariant, result),
									  Q_ARG(QVariant, url), Q_ARG(QVariant, trusted));
			return result.toBool();
		};
		const auto destination = [&](const QString& url) {
			QVariant result;
			QMetaObject::invokeMethod(window, "destination", Q_RETURN_ARG(QVariant, result),
									  Q_ARG(QVariant, url));
			return result.toString();
		};

		// Any other scheme, or no link at all, is refused, trusted or not.
		const QRegularExpression refused(QStringLiteral("ExternalLinks: refusing"));
		for (const QString& url : {QStringLiteral("kite-test:anything"), QStringLiteral("relative/page.html"),
								   QString()}) {
			for (const bool trusted : {false, true}) {
				QTest::ignoreMessage(QtWarningMsg, refused);
				QVERIFY2(!open(url, trusted), qPrintable(url));
			}
		}
		QVERIFY(!window->findChild<QObject*>(QStringLiteral("externalLinkDialog")));
		QCOMPARE(links.opened.size(), 0);

		QCOMPARE(destination(QStringLiteral("https://kite.example@other.example/page")),
				 QStringLiteral("other.example"));
		QCOMPARE(destination(QStringLiteral("mailto:help@kite.example")), QStringLiteral("help@kite.example"));

		// A web link opens only once the question is answered.
		const QUrl page(QStringLiteral("https://example.com/mods?id=1"));
		QVERIFY(open(page.toString(), false));
		QObject* dialog = nullptr;
		QTRY_VERIFY((dialog = window->findChild<QObject*>(QStringLiteral("externalLinkDialog"))));
		QTRY_VERIFY(dialog->property("opened").toBool());
		QCOMPARE(dialog->property("destination").toString(), QStringLiteral("example.com"));
		QCOMPARE(links.opened.size(), 0);
		QVERIFY(QMetaObject::invokeMethod(dialog, "primaryClicked"));
		QCOMPARE(links.opened, QList<QUrl>{page});

		// Kite's own link does not ask.
		const QUrl help(QStringLiteral("https://kite.example/help"));
		QVERIFY(open(help.toString(), true));
		QCOMPARE(links.opened, (QList<QUrl>{page, help}));
	}

	/* The core's message is plain text: its lines are newlines, a tag in it is
	 * shown as written, and nothing in it is a link. */
	void uiRequestDialogShowsLinesAsPlainText()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			Window {
				width: 900; height: 700; visible: true
				QtObject {
					id: host
					property var current: null
				}
				QtObject {
					id: message
					property string kind: "message"
					property string title: "Check your instance folder"
					property string text: "The folder is a problem.\n\nYou have two options:\n - one\n - <b>two</b>"
					property string severity: "warning"
					property string acceptLabel: ""
					property string rejectLabel: ""
					property string filePickerMode: ""
					property string filePickerFilter: ""
					property string profileNameStatus: ""
					property string profileNameError: ""
					property int confirmDelayMs: 0
					function accept() {}
					function reject() {}
				}
				UiRequestDialog { host: host }
				function show() { host.current = message }
			})",
						  QUrl(QStringLiteral("qrc:/test/Request.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> object(component.create());
		auto* window = qobject_cast<QQuickWindow*>(object.get());
		QVERIFY2(window, qPrintable(component.errorString()));
		QVERIFY(QTest::qWaitForWindowExposed(window));
		QVERIFY(QMetaObject::invokeMethod(window, "show"));

		QQuickItem* text = nullptr;
		QTRY_VERIFY((text = window->findChild<QQuickItem*>(QStringLiteral("requestText"))) &&
					text->isVisible() && text->width() > 0);
		QCOMPARE(text->property("textFormat").toInt(), int(Qt::PlainText));
		QVERIFY(text->property("text").toString().endsWith(QStringLiteral("<b>two</b>")));
		const int lines = 5;
		QTRY_COMPARE(text->property("lineCount").toInt(), lines);
	}

	/* An update on offer: "Restart and update" only while no game started from Kite runs (Kite is
	 * never replaced under one), "Update when I close Kite" unless a package manager installs it,
	 * "Later" at the footer's other end and "Skip this version" under the notes, and each button
	 * answers what it says. */
	void updateOfferButtonsFollowTheSituation()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(R"(
			import QtQuick
			import Kite.Components
			Window {
				id: win
				width: 900; height: 700; visible: true
				property string answer: ""
				QtObject {
					id: host
					property var current: null
				}
				Component {
					id: offer
					QtObject {
						property string kind: "update"
						property string title: "Kite 1.3.0 is available"
						property string text: "Kite restarts to install it."
						property string severity: "info"
						property string acceptLabel: ""
						property string rejectLabel: ""
						property string filePickerMode: ""
						property string filePickerFilter: ""
						property string profileNameStatus: ""
						property string profileNameError: ""
						property int confirmDelayMs: 0
						property var updateInfo: ({})
						function answerUpdate(choice) { win.answer = choice; host.current = null }
						function reject() { win.answer = "rejected"; host.current = null }
					}
				}
				UiRequestDialog { host: host }
				function show(info) {
					win.answer = ""
					host.current = offer.createObject(win, { updateInfo: info })
				}
			})",
						  QUrl(QStringLiteral("qrc:/test/Update.qml")));
		QVERIFY2(component.isReady(), qPrintable(component.errorString()));
		std::unique_ptr<QObject> object(component.create());
		auto* window = qobject_cast<QQuickWindow*>(object.get());
		QVERIFY2(window, qPrintable(component.errorString()));
		QVERIFY(QTest::qWaitForWindowExposed(window));

		const auto show = [&](bool canInstallNow, const QString& managedHint) {
			const QVariant info = QVariantMap{{"currentVersion", "1.2.0"},
											  {"availableVersion", "1.3.0"},
											  {"releaseNotes", ""},
											  {"canInstallNow", canInstallNow},
											  {"managedHint", managedHint}};
			return QMetaObject::invokeMethod(window, "show", Q_ARG(QVariant, info));
		};
		const auto button = [&](const char* name) {
			return window->findChild<QQuickItem*>(QLatin1String(name));
		};

		// No game running: restart now, highlighted, or once Kite closes.
		QVERIFY(show(true, QString()));
		QQuickItem* now = nullptr;
		QQuickItem* onExit = nullptr;
		QTRY_VERIFY((now = button("updateNowButton")) && now->isVisible());
		QVERIFY((onExit = button("updateOnExitButton")) && onExit->isVisible());
		QVERIFY(now->property("highlighted").toBool());
		QVERIFY(!onExit->property("highlighted").toBool());
		QQuickItem* later = button("updateLaterButton");
		QQuickItem* skip = button("updateSkipButton");
		QVERIFY(later && later->isVisible());
		QVERIFY(skip && skip->isVisible());
		// "Later" sits at the left end, the installing answers at the right.
		// Once the footer row has laid out.
		QTRY_VERIFY(later->mapToScene(QPointF(0, 0)).x() < onExit->mapToScene(QPointF(0, 0)).x() &&
					onExit->mapToScene(QPointF(0, 0)).x() < now->mapToScene(QPointF(0, 0)).x());
		QVERIFY(QMetaObject::invokeMethod(now, "clicked"));
		QTRY_COMPARE(window->property("answer").toString(), QStringLiteral("install"));

		QVERIFY(show(true, QString()));
		QTRY_VERIFY(skip->isVisible());
		QVERIFY(QMetaObject::invokeMethod(skip, "clicked"));
		QTRY_COMPARE(window->property("answer").toString(), QStringLiteral("skip"));

		QVERIFY(show(true, QString()));
		QTRY_VERIFY(later->isVisible());
		QVERIFY(QMetaObject::invokeMethod(later, "clicked"));
		QTRY_COMPARE(window->property("answer").toString(), QStringLiteral("later"));

		// A game is running: no restart, and waiting for Kite to close is the main choice.
		QVERIFY(show(false, QString()));
		QTRY_VERIFY(onExit->isVisible());
		QVERIFY(!now->isVisible());
		QVERIFY(onExit->property("highlighted").toBool());
		QVERIFY(QMetaObject::invokeMethod(onExit, "clicked"));
		QTRY_COMPARE(window->property("answer").toString(),
					 QStringLiteral("installOnExit"));

		// Scoop installs it: Kite only tells, and installs nothing.
		QVERIFY(show(true, QStringLiteral("Scoop keeps Kite up to date.")));
		QQuickItem* ok = nullptr;
		QTRY_VERIFY((ok = button("updateOkButton")) && ok->isVisible());
		QVERIFY(!now->isVisible());
		QVERIFY(!onExit->isVisible());
		QVERIFY(!later->isVisible());
		QVERIFY(skip->isVisible());
		QVERIFY(QMetaObject::invokeMethod(ok, "clicked"));
		QTRY_COMPARE(window->property("answer").toString(), QStringLiteral("later"));
	}
};

int main(int argc, char* argv[])
{
	/* Offscreen keeps it runnable on a headless runner. Qt Quick needs a
	 * QGuiApplication, so QTEST_GUILESS_MAIN is not an option. */
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);
	QmlModuleTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "QmlModule_test.moc"
