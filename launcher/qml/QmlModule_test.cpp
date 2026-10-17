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
 * Smoke test for the QML user interface module.
 *
 * This does not test any behaviour; it proves the toolchain is wired up: that
 * qt_add_qml_module ran, that the .qml files were compiled into the binary at
 * the resource prefix we expect, and that the engine can instantiate the root
 * component. Those are exactly the things that break silently -- a missing
 * module, a shifted resource prefix or a qmlcachegen failure all produce a
 * perfectly green build and an application that shows nothing.
 *
 * The URL is spelled out rather than using QQmlComponent::loadFromModule(),
 * which is Qt 6.5+; the project's floor is 6.4 (Debian 12 / Ubuntu 24.04 LTS).
 * qt_add_qml_module is given RESOURCE_PREFIX "/qt/qml" so this path is the
 * same on 6.4 as it is once QTP0001 is available.
 */
/* Stands in for QmlShell where a test needs Main.qml to see one of its
 * signals or properties. Everything else stays absent, as with the plain
 * QObject rootComponentInstantiates() passes: Main.qml checks for each
 * shell member before using it. */
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

	/// What QmlShell::launchInstanceMode() was last asked, as "id:mode".
	Q_INVOKABLE void launchInstanceMode(const QString& id, const QString& mode)
	{
		lastLaunch = id + QLatin1Char(':') + mode;
	}

	int m_accountCount = 1;
	QString lastLaunch;

  signals:
	void importRequested(const QString& source);
	void navigateRequested(const QString& target);

  private:
	QObject m_newInstance;
};

class QmlModuleTest : public QObject
{
	Q_OBJECT

	/* Main.qml instantiated with @p shell, or null (with the reason
	 * already reported through QVERIFY) if it fails to load. */
	std::unique_ptr<QObject> createRoot(QQmlEngine& engine, QObject* shell)
	{
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(
			&engine, QUrl(QStringLiteral("qrc:/qt/qml/MeshMC/Main.qml")));
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

	/* @p expression evaluated in Main.qml's own scope, where its ids
	 * (settingsPage, newInstanceDialog, ...) resolve. */
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
	 * undefined, the way a one-argument call from QML would. */
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
	/* The same controls style QmlShell::show() selects: components use its
	 * extra properties (Button.danger, ...), which Qt's default style lacks,
	 * so without it Main.qml would not load here although it does in the
	 * application. */
	void initTestCase()
	{
		QQuickStyle::setStyle(QStringLiteral("MeshMC.Style"));
	}

	void rootComponentInstantiates()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));

		QQmlComponent component(
			&engine, QUrl(QStringLiteral("qrc:/qt/qml/MeshMC/Main.qml")));

		QVERIFY2(!component.isError(), qPrintable(component.errorString()));
		QVERIFY2(component.isReady(),
				 qPrintable(QStringLiteral("component not ready: %1")
								.arg(component.errorString())));

		/* The root requires the instance model, the selection and the shell;
		 * an empty stand-in is enough to prove the component loads, and a
		 * required property left unset would itself be a load error worth
		 * catching here. The shell is a plain QObject: this test only proves
		 * Main.qml accepts something for the property, not what QmlShell
		 * itself does. */
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

		/* Guards against the component resolving to something default
		 * constructed: the property only exists on our Main.qml. */
		QCOMPARE(root->property("moduleName").toString(),
				 QStringLiteral("MeshMC"));
	}

	/* navigate() is how pages link across the app: whole paths and the
	 * split (page, sub) form alike, settings sections included, and an
	 * unknown target changes nothing. */
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

		QVERIFY(navigate(root.get(), QStringLiteral("library")));
		QCOMPARE(root->property("page").toString(), QStringLiteral("library"));

		QVERIFY(!navigate(root.get(), QStringLiteral("nowhere/at/all")));
		QCOMPARE(root->property("page").toString(), QStringLiteral("library"));
	}

	/* No instance by that id (this stand-in shell has no instanceDetails()
	 * at all): refused, and the current page stays. */
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

	/* QmlShell::requestNavigate() (a launch that needs an account, say)
	 * reaches Main.qml's navigate(). */
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

	/* The Play options: with no account at all, "Play offline" goes to the
	 * Accounts page instead of launching; the demo needs no account and
	 * launches either way. */
	void launchModeNeedsAnAccountExceptForTheDemo()
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

		QVERIFY(QMetaObject::invokeMethod(
			root.get(), "launchMode", Q_RETURN_ARG(QVariant, unused),
			Q_ARG(QVariant, QStringLiteral("fabric")),
			Q_ARG(QVariant, QStringLiteral("demo"))));
		QCOMPARE(shell.lastLaunch, QStringLiteral("fabric:demo"));
	}

	/* The macOS menu bar only ever shows natively (Cocoa); anywhere else
	 * -- this offscreen test included -- it stays out of the window. */
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
	 * model's own roles (the preview data has no plugins to render). */
	void pluginListSectionDrawsARowPerPlugin()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		// Plain literals rather than a raw string: moc stops parsing the
		// rest of this file at one.
		component.setData(
			"import QtQuick\n"
			"import MeshMC.Components\n"
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
		// Every row is a SettingRow carrying its plugin's name and version;
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

	/* A switch must go on showing what the model says, also after the user
	 * has flipped it: a plugin may refuse (or only apply the change at the
	 * next start), and a switch left showing the click instead of the state
	 * is the "buttons are wrong until a restart" bug. */
	void pluginSwitchesKeepFollowingTheirModel()
	{
		QQmlEngine engine;
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData(
			"import QtQuick\n"
			"import MeshMC.Components\n"
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

	/* QmlShell.news reaches the Home page's welcome card. */
	void homeShowsTheNewestHeadline()
	{
		QQmlEngine engine;
		FakeShell shell;
		shell.m_news.setEntries({ std::make_shared<NewsEntry>(
			QStringLiteral("MeshMC 7 is out"), QString(),
			QStringLiteral("https://example.org/7"), QString(), QDateTime()) });
		auto root = createRoot(engine, &shell);
		QVERIFY(root);
		// HomePage reads the model's rows once they settle (Qt.callLater).
		QTRY_COMPARE(evaluate(root.get(), QStringLiteral("homePage.hasNews ? homePage.featuredNews.title : ''"))
						 .toString(),
					 QStringLiteral("MeshMC 7 is out"));
	}

	/* Home's banner is for news: a release post for the version running
	 * (or older) is skipped, and nothing older than newsMaxAgeDays gets a
	 * banner at all -- the feed is newest first, so a stale entry ends the
	 * search. */
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
			shell.m_news.setEntries({ post(QStringLiteral("MeshMC Release 12.1.0, now available"), now),
									  post(QStringLiteral("MeshMC Release 11.0.0"), now.addDays(-1)) });
			auto root = createRoot(engine, &shell);
			QVERIFY(root);
			QTRY_COMPARE(evaluate(root.get(), QStringLiteral("homePage.hasNews ? homePage.featuredNews.title : ''"))
							 .toString(),
						 QStringLiteral("MeshMC Release 12.1.0, now available"));
		}
		{
			QQmlEngine engine;
			FakeShell shell;
			shell.m_news.setEntries({ post(QStringLiteral("MeshMC Release 11.0.0"), now),
									  post(QStringLiteral("MeshMC Release 9.1.0, now available"), now.addDays(-2)),
									  post(QStringLiteral("Welcome to MeshMC"), now.addDays(-200)) });
			auto root = createRoot(engine, &shell);
			QVERIFY(root);
			QTRY_COMPARE(evaluate(root.get(), QStringLiteral("homePage.newsEntries.length")).toInt(), 3);
			QCOMPARE(evaluate(root.get(), QStringLiteral("homePage.hasNews")).toBool(), false);
		}
		QCoreApplication::setApplicationVersion(runningVersion);
	}

	/* `meshmc --import` and the IPC "import" message reach QML as
	 * QmlShell::importRequested(): the New instance dialog opens in import
	 * mode with the source already filled in (the A9/A10 fix). The
	 * stand-in controller is a bare QObject, so the dialog logs TypeErrors
	 * for the NewInstanceController members it reads; only the two
	 * properties checked here matter. */
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

	/* A file dropped on the window opens import with it -- unless a drop
	 * area inside the pages (the Content tab's mods, the Worlds tab's
	 * worlds) takes it: Main.qml's window-wide one sits under all of them.
	 * The inner area here stands in for those, made the way a page's own
	 * would be stacked: a later item above the window's first child. */
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
	/* The test instantiates QML objects but never shows a window. Forcing the
	 * offscreen platform keeps it runnable on a headless CI runner without
	 * depending on the harness to set QT_QPA_PLATFORM for us. Qt Quick types
	 * need a QGuiApplication, so QTEST_GUILESS_MAIN (QCoreApplication) -- what
	 * the other tests in this tree use -- is not an option here. */
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);
	QmlModuleTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "QmlModule_test.moc"
