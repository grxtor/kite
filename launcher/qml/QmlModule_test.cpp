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
