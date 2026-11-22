/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 */

#include <QtTest>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickItem>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardItemModel>
#include <functional>
#include <memory>

// QtGui's own entry to the shortcut map (declared by QtTest for widgets only).
Q_GUI_EXPORT bool qt_sendShortcutOverrideEvent(QObject* o, ulong timestamp, int k,
											   Qt::KeyboardModifiers mods,
											   const QString& text,
											   bool autorep, ushort count);

/* NetworkStatus as the shell hands it to QML. */
class FakeNetwork : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool online READ online NOTIFY onlineChanged)

  public:
	bool online() const
	{
		return m_online;
	}
	void setOnline(bool online)
	{
		m_online = online;
		emit onlineChanged();
	}
	Q_INVOKABLE bool recheck()
	{
		++rechecks;
		return m_online;
	}
	int rechecks = 0;

  signals:
	void onlineChanged();

  private:
	bool m_online = true;
};

/* The part of AccountsController the profile button reads. */
class FakeAccounts : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString defaultStateKey MEMBER stateKey NOTIFY changed)
	Q_PROPERTY(QString defaultError MEMBER error NOTIFY changed)
	Q_PROPERTY(QObject* accounts READ accounts CONSTANT)

  public:
	QObject* accounts() const
	{
		return nullptr;
	}
	Q_INVOKABLE void refreshDefault()
	{
		++refreshes;
	}
	QString stateKey;
	QString error;
	int refreshes = 0;

  signals:
	void changed();
};

class FakeShell : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QObject* network READ network CONSTANT)

  public:
	QObject* network()
	{
		return &net;
	}
	FakeNetwork net;
};

/*
 * The app shell's own behaviour in QML: the page history behind Back and
 * Forward, the shortcut list and the keys the window answers to, the offline
 * notice and the account warning on the profile button.
 */
class AppShellQmlTest : public QObject
{
	Q_OBJECT

	std::unique_ptr<QObject> createRoot(QQmlEngine& engine, QObject* shell)
	{
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(
			&engine, QUrl(QStringLiteral("qrc:/qt/qml/Kite/Main.qml")));
		std::unique_ptr<QObject> root(component.createWithInitialProperties(
			{ { QStringLiteral("instanceModel"),
				QVariant::fromValue<QObject*>(&m_instances) },
			  { QStringLiteral("selection"),
				QVariant::fromValue<QObject*>(&m_selection) },
			  { QStringLiteral("shell"),
				QVariant::fromValue<QObject*>(shell) } }));
		if (!root) {
			qWarning() << component.errorString();
		}
		return root;
	}

	static std::unique_ptr<QObject> create(QQmlEngine& engine,
										   const QByteArray& qml)
	{
		engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
		QQmlComponent component(&engine);
		component.setData("import QtQuick\nimport Kite.Components\n" + qml,
						  QUrl());
		std::unique_ptr<QObject> object(component.create());
		if (!object) {
			qWarning() << component.errorString();
		}
		return object;
	}

	/* @p expression evaluated in @p object's scope, where its ids resolve. */
	static QVariant evaluate(QObject* object, const QString& expression)
	{
		QQmlExpression expr(qmlContext(object), object, expression);
		const QVariant result = expr.evaluate();
		if (expr.hasError()) {
			qWarning() << expr.error().toString();
		}
		return result;
	}

	/* A key as the platform delivers it: to the shortcut map first, and to
	 * the focused item when no shortcut took it. (QTest::keyClick() leaves
	 * the shortcut map out on macOS.) */
	static void press(QWindow* window, Qt::Key key,
					  Qt::KeyboardModifiers modifiers = Qt::NoModifier)
	{
		if (!qt_sendShortcutOverrideEvent(window, 0, key, modifiers, QString(),
										  false, 1)) {
			QTest::keyClick(window, key, modifiers);
		}
		QCoreApplication::processEvents();
	}

	static QString page(const std::unique_ptr<QObject>& root)
	{
		return root->property("page").toString();
	}

	/* A history whose go() refuses the routes in `gone` and records the rest
	 * in `went`. */
	static QByteArray historyQml()
	{
		return R"(
			NavHistory {
				property var gone: []
				property var went: []
				go: function (route) {
					if (gone.indexOf(route) >= 0)
						return false
					went = went.concat([route])
					return true
				}
			})";
	}

	QStandardItemModel m_instances;
	QObject m_selection;

  private slots:
	void initTestCase()
	{
		QQuickStyle::setStyle(QStringLiteral("Kite.Style"));
	}

	/* A new page is added; another tab of the same page replaces its entry;
	 * going back and then elsewhere drops what lay forward. */
	void historyAddsPagesAndReplacesTabs()
	{
		QQmlEngine engine;
		auto history = create(engine, historyQml());
		QVERIFY(history);
		for (const char* route :
			 { "home", "instance/a/", "instance/a/mods", "settings/general",
			   "settings/java" }) {
			evaluate(history.get(), QStringLiteral("visit('%1')").arg(route));
		}
		QCOMPARE(history->property("entries").toStringList(),
				 (QStringList{ "home", "instance/a/mods", "settings/java" }));
		QCOMPARE(history->property("index").toInt(), 2);
		QVERIFY(history->property("canGoBack").toBool());
		QVERIFY(!history->property("canGoForward").toBool());

		QVERIFY(evaluate(history.get(), QStringLiteral("back()")).toBool());
		QVERIFY(evaluate(history.get(), QStringLiteral("back()")).toBool());
		QVERIFY(!evaluate(history.get(), QStringLiteral("back()")).toBool());
		QCOMPARE(history->property("went").toStringList(),
				 (QStringList{ "instance/a/mods", "home" }));
		QVERIFY(evaluate(history.get(), QStringLiteral("forward()")).toBool());
		QCOMPARE(history->property("index").toInt(), 1);
		// Walking adds nothing, even when the page reports where it now is.
		evaluate(history.get(), QStringLiteral("visit('instance/a/mods')"));
		QCOMPARE(history->property("entries").toStringList().size(), 3);

		evaluate(history.get(), QStringLiteral("visit('discover')"));
		QCOMPARE(history->property("entries").toStringList(),
				 (QStringList{ "home", "instance/a/mods", "discover" }));
		QVERIFY(!history->property("canGoForward").toBool());
	}

	/* An instance deleted since is skipped and forgotten, and the two visits
	 * to Home around it become one. */
	void historyDropsAnEntryThatNoLongerOpens()
	{
		QQmlEngine engine;
		auto history = create(engine, historyQml());
		QVERIFY(history);
		for (const char* route :
			 { "discover", "home", "instance/a/mods", "home" }) {
			evaluate(history.get(), QStringLiteral("visit('%1')").arg(route));
		}
		evaluate(history.get(), QStringLiteral("gone = ['instance/a/mods']"));
		QVERIFY(evaluate(history.get(), QStringLiteral("back()")).toBool());
		QCOMPARE(history->property("went").toStringList(),
				 QStringList{ "discover" });
		QCOMPARE(history->property("entries").toStringList(),
				 (QStringList{ "discover", "home" }));
		QCOMPARE(history->property("index").toInt(), 0);

		// Nothing left that opens: the place is kept.
		evaluate(history.get(), QStringLiteral("gone = ['home']"));
		QVERIFY(!evaluate(history.get(), QStringLiteral("forward()")).toBool());
		QCOMPARE(history->property("entries").toStringList(),
				 QStringList{ "discover" });
		QCOMPARE(history->property("index").toInt(), 0);
	}

	/* A modpack opened in Discover is a page of its own: Back returns to the
	 * results, Forward to the pack, and a second pack is a third page. */
	void historyKeepsDiscoverPacksAsPages()
	{
		QQmlEngine engine;
		auto history = create(engine, historyQml());
		QVERIFY(history);
		for (const char* route :
			 { "home", "discover", "discover/modrinth/abc",
			   "discover/modrinth/xyz" }) {
			evaluate(history.get(), QStringLiteral("visit('%1')").arg(route));
		}
		QCOMPARE(history->property("entries").toStringList(),
				 (QStringList{ "home", "discover", "discover/modrinth/abc",
							   "discover/modrinth/xyz" }));
		QVERIFY(evaluate(history.get(), QStringLiteral("back()")).toBool());
		QVERIFY(evaluate(history.get(), QStringLiteral("back()")).toBool());
		QVERIFY(evaluate(history.get(), QStringLiteral("forward()")).toBool());
		QCOMPARE(history->property("went").toStringList(),
				 (QStringList{ "discover/modrinth/abc", "discover",
							   "discover/modrinth/abc" }));
		/* A pack that can no longer be reopened is dropped like a deleted instance. */
		evaluate(history.get(),
				 QStringLiteral("gone = ['discover/modrinth/xyz']"));
		QVERIFY(!evaluate(history.get(), QStringLiteral("forward()")).toBool());
		QCOMPARE(history->property("entries").toStringList().size(), 3);
	}

	/* Several ticked results install one after another through the view's own
	 * install; one that cannot be installed is counted and skipped. */
	void batchInstallRunsOneAfterAnother()
	{
		QQmlEngine engine;
		auto batch = create(engine, R"(
			ContentBatchInstall {
				id: batch
				property var started: []
				property var result: null
				onFinished: (installed, failed) => result = [installed, failed]
				owner: QtObject {
					signal installSettled(string key, bool ok)
					function projectKey(title, author) { return title + "/" + author }
					function startInstall(key, row, versionId) {
						if (versionId.length === 0)
							return false
						batch.started = batch.started.concat([key + "@" + row + ":" + versionId])
						return true
					}
				}
				browser: QtObject {
					signal rowPrepared(int row)
					property var rows: ["a", "b", "c"]
					function rowOfProject(id) { return rows.indexOf(id) }
					function prepareRow(row) { Qt.callLater(rowPrepared, row) }
					function bestVersionId(row) { return row === 1 ? "" : "v" + row }
				}
			})");
		QVERIFY(batch);
		evaluate(batch.get(), QStringLiteral("toggle('a', 'A', 'x')"));
		evaluate(batch.get(), QStringLiteral("toggle('b', 'B', 'x')"));
		evaluate(batch.get(), QStringLiteral("toggle('c', 'C', 'x')"));
		evaluate(batch.get(), QStringLiteral("toggle('gone', 'Gone', 'x')"));
		evaluate(batch.get(), QStringLiteral("toggle('gone', 'Gone', 'x')"));
		QCOMPARE(evaluate(batch.get(), QStringLiteral("selected.length")).toInt(), 3);
		QVERIFY(evaluate(batch.get(), QStringLiteral("isSelected('b')")).toBool());

		evaluate(batch.get(), QStringLiteral("start()"));
		QVERIFY(batch->property("running").toBool());
		QCOMPARE(evaluate(batch.get(), QStringLiteral("selected.length")).toInt(), 0);
		/* Only the first has begun; the second waits for it. */
		QTRY_COMPARE(batch->property("started").toStringList(),
					 QStringList{ "A/x@0:v0" });
		QCoreApplication::processEvents();
		QCOMPARE(batch->property("started").toStringList().size(), 1);

		/* Done: "b" has no version and is skipped, "c" begins. */
		evaluate(batch.get(), QStringLiteral("owner.installSettled('A/x', true)"));
		QTRY_COMPARE(batch->property("started").toStringList(),
					 (QStringList{ "A/x@0:v0", "C/x@2:v2" }));
		QVERIFY(batch->property("running").toBool());
		evaluate(batch.get(), QStringLiteral("owner.installSettled('C/x', true)"));
		QTRY_VERIFY(!batch->property("running").toBool());
		QCOMPARE(evaluate(batch.get(), QStringLiteral("result[0]")).toInt(), 2);
		QCOMPARE(evaluate(batch.get(), QStringLiteral("result[1]")).toInt(), 1);
	}

	void historyKeepsAtMostFiftyPages()
	{
		QQmlEngine engine;
		auto history = create(engine, historyQml());
		QVERIFY(history);
		for (int i = 0; i < 70; ++i) {
			evaluate(history.get(),
					 QStringLiteral("visit('instance/%1/')").arg(i));
		}
		const QStringList entries = history->property("entries").toStringList();
		QCOMPARE(entries.size(), 50);
		QCOMPARE(entries.first(), QStringLiteral("instance/20/"));
		QCOMPARE(entries.last(), QStringLiteral("instance/69/"));
		QCOMPARE(history->property("index").toInt(), 49);
	}

	/* Main.qml's pages reach the history by themselves, a settings section
	 * with them, and back/forward lead through them. */
	void windowGoesBackAndForwardThroughItsPages()
	{
		QQmlEngine engine;
		QObject shell;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);
		const auto entries = [&root] {
			return evaluate(root.get(), QStringLiteral("history.entries"))
				.toStringList();
		};
		QCOMPARE(entries(), QStringList{ "home" });

		evaluate(root.get(), QStringLiteral("root.navigate('discover')"));
		QTRY_COMPARE(entries(), (QStringList{ "home", "discover" }));
		evaluate(root.get(), QStringLiteral("root.navigate('settings/java')"));
		QTRY_COMPARE(entries(),
					 (QStringList{ "home", "discover", "settings/java" }));
		evaluate(root.get(), QStringLiteral("settingsPage.section = 'proxy'"));
		QTRY_COMPARE(entries().last(), QStringLiteral("settings/proxy"));

		QVERIFY(evaluate(root.get(), QStringLiteral("history.back()")).toBool());
		QCOMPARE(page(root), QStringLiteral("discover"));
		QVERIFY(evaluate(root.get(), QStringLiteral("history.back()")).toBool());
		QCOMPARE(page(root), QStringLiteral("home"));
		QVERIFY(!evaluate(root.get(), QStringLiteral("history.back()")).toBool());
		QVERIFY(
			evaluate(root.get(), QStringLiteral("history.forward()")).toBool());
		QVERIFY(
			evaluate(root.get(), QStringLiteral("history.forward()")).toBool());
		QCOMPARE(page(root), QStringLiteral("settings"));
		QCOMPARE(evaluate(root.get(), QStringLiteral("settingsPage.section"))
					 .toString(),
				 QStringLiteral("proxy"));
		// Walking added nothing.
		QTest::qWait(20);
		QCOMPARE(entries().size(), 3);
	}

	/* One list, two platforms: the sequences a Shortcut listens to and the
	 * caps the sheet draws come from the same entry. */
	void keyMapNamesEachPlatformsKeys()
	{
		QQmlEngine engine;
		auto probe = create(engine, R"(
			QtObject {
				function keys(mac, id) { KeyMap.mac = mac; return KeyMap.keys(id) }
				function caps(mac, id) {
					KeyMap.mac = mac
					return KeyMap.combos(KeyMap.entry(id)).map((combo) => combo.join(" ")).join(" | ")
				}
				function listed(mac, group) {
					KeyMap.mac = mac
					return KeyMap.entriesOf(group).map((one) => one.id)
				}
				// Every action the window itself answers to has a key on both platforms.
				function windowKeysMissing() {
					var missing = []
					for (var mac of [false, true]) {
						KeyMap.mac = mac
						for (var id of ["back", "forward", "home", "discover", "settings", "play", "help",
										"find", "new"])
							if (KeyMap.keys(id).length === 0 || !KeyMap.entry(id) || KeyMap.entry(id).elsewhere)
								missing.push(id)
					}
					return missing
				}
				// Every entry belongs to a group the sheet draws.
				function strays() {
					var groups = KeyMap.groups.map((one) => one.id)
					return KeyMap.entries.filter((one) => groups.indexOf(one.group) < 0).map((one) => one.id)
				}
			})");
		QVERIFY(probe);
		const auto call = [&probe](const QString& expression) {
			return evaluate(probe.get(), expression);
		};
		QCOMPARE(call("keys(false, 'back')").toStringList(),
				 QStringList{ "Alt+Left" });
		QCOMPARE(call("keys(true, 'back')").toStringList(),
				 QStringList{ "Ctrl+[" });
		QCOMPARE(call("keys(true, 'forward')").toStringList(),
				 QStringList{ "Ctrl+]" });
		QCOMPARE(call("keys(false, 'settings')").toStringList(),
				 QStringList{ "Ctrl+," });
		QCOMPARE(call("keys(false, 'help')").toStringList(), QStringList{ "?" });

		QCOMPARE(call("caps(false, 'back')").toString(),
				 QStringLiteral("Alt ←"));
		QCOMPARE(call("caps(true, 'back')").toString(),
				 QStringLiteral("⌘ ["));
		QCOMPARE(call("caps(false, 'play')").toString(),
				 QStringLiteral("Ctrl Enter"));
		QCOMPARE(call("caps(true, 'play')").toString(),
				 QStringLiteral("⌘ Return"));
		QCOMPARE(call("caps(false, 'move')").toString(),
				 QStringLiteral("← ↑ ↓ →"));

		// No Menu key on a Mac keyboard.
		QVERIFY(call("listed(false, 'library')").toStringList().contains("menu"));
		QVERIFY(!call("listed(true, 'library')").toStringList().contains("menu"));

		QCOMPARE(call("windowKeysMissing()").toStringList(), QStringList());
		QCOMPARE(call("strays()").toStringList(), QStringList());
	}

	/* The keys reach the window: pages, the sheet on "?", and nothing but
	 * Esc while a dialog is open. */
	void windowAnswersToItsKeys()
	{
		QQmlEngine engine;
		QObject shell;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);
		auto* window = qobject_cast<QQuickWindow*>(root.get());
		QVERIFY(window);
		window->show();
		window->requestActivate();
		if (!QTest::qWaitForWindowActive(window, 2000)) {
			QSKIP("This platform does not activate windows.");
		}
		const bool mac =
			evaluate(root.get(), QStringLiteral("KeyMap.mac")).toBool();
		const auto sheetOpen = [&root] {
			return evaluate(root.get(), QStringLiteral(
											"!!shortcutsLoader.item && shortcutsLoader.item.opened"))
				.toBool();
		};

		press(window, Qt::Key_2, Qt::ControlModifier);
		QCOMPARE(page(root), QStringLiteral("discover"));
		press(window, Qt::Key_Comma, Qt::ControlModifier);
		QCOMPARE(page(root), QStringLiteral("settings"));
		press(window, Qt::Key_1, Qt::ControlModifier);
		QCOMPARE(page(root), QStringLiteral("home"));
		QTRY_COMPARE(evaluate(root.get(), QStringLiteral("history.entries.length"))
						 .toInt(),
					 4);

		if (mac) {
			press(window, Qt::Key_BracketLeft, Qt::ControlModifier);
		} else {
			press(window, Qt::Key_Left, Qt::AltModifier);
		}
		QCOMPARE(page(root), QStringLiteral("settings"));
		if (mac) {
			press(window, Qt::Key_BracketRight, Qt::ControlModifier);
		} else {
			press(window, Qt::Key_Right, Qt::AltModifier);
		}
		QCOMPARE(page(root), QStringLiteral("home"));

		QTest::mouseClick(window, Qt::BackButton, Qt::NoModifier,
						  QPoint(window->width() / 2, window->height() / 2));
		QCOMPARE(page(root), QStringLiteral("settings"));
		QTest::mouseClick(window, Qt::ForwardButton, Qt::NoModifier,
						  QPoint(window->width() / 2, window->height() / 2));
		QCOMPARE(page(root), QStringLiteral("home"));

		QVERIFY(!sheetOpen());
		// As a keyboard's own key map reports Shift+/: the character, Shift folded in.
		press(window, Qt::Key_Question);
		QTRY_VERIFY(sheetOpen());
		// Under the dialog the window's shortcuts wait.
		press(window, Qt::Key_2, Qt::ControlModifier);
		QCOMPARE(page(root), QStringLiteral("home"));
		press(window, Qt::Key_Escape);
		// The keys come back once the dialog has faded out.
		QTRY_VERIFY(!evaluate(root.get(),
							  QStringLiteral("shortcutsLoader.item.visible"))
						 .toBool());
		press(window, Qt::Key_2, Qt::ControlModifier);
		QCOMPARE(page(root), QStringLiteral("discover"));
	}

	/* The notice shows only while the machine is known to be offline, and
	 * "Try again" asks again. */
	void offlineNoticeFollowsTheNetwork()
	{
		QQmlEngine engine;
		FakeNetwork network;
		engine.rootContext()->setContextProperty(QStringLiteral("fakeNetwork"),
												 &network);
		auto notice = create(engine, R"(
			OfflineNotice {
				width: 600
				network: fakeNetwork
				property int complaints: 0
				onStillOffline: complaints += 1
			})");
		QVERIFY(notice);
		QVERIFY(!notice->property("shown").toBool());
		QCOMPARE(notice->property("implicitHeight").toInt(), 0);

		network.setOnline(false);
		QVERIFY(notice->property("shown").toBool());
		QTRY_VERIFY(notice->property("implicitHeight").toInt() > 0);

		QObject* retry = nullptr;
		for (QObject* child : notice->findChildren<QObject*>()) {
			if (child->property("text").toString() == QLatin1String("Try again") &&
				child->metaObject()->indexOfSignal("clicked()") >= 0) {
				retry = child;
			}
		}
		QVERIFY(retry);
		QVERIFY(QMetaObject::invokeMethod(retry, "clicked"));
		QCOMPARE(network.rechecks, 1);
		QCOMPARE(notice->property("complaints").toInt(), 1);

		network.setOnline(true);
		QVERIFY(!notice->property("shown").toBool());

		// With no NetworkStatus at all (a build or platform without one): never.
		evaluate(notice.get(), QStringLiteral("network = null"));
		QVERIFY(!notice->property("shown").toBool());
	}

	/* Main.qml hands the shell's answer to LauncherStore.online, and while it
	 * is false a server that does not answer is not called "Offline". */
	void serversAreNotCalledOfflineWhileTheMachineIs()
	{
		QQmlEngine engine;
		FakeShell shell;
		auto root = createRoot(engine, &shell);
		QVERIFY(root);
		auto summary = create(engine, R"(
			QuickJoinSummary {
				server: ({ name: "Hypixel" })
				status: ({ state: "offline" })
			})");
		QVERIFY(summary);
		QVERIFY(evaluate(root.get(), QStringLiteral("LauncherStore.online"))
					.toBool());
		QCOMPARE(summary->property("countText").toString(),
				 QStringLiteral("Offline"));

		shell.net.setOnline(false);
		QVERIFY(!evaluate(root.get(), QStringLiteral("LauncherStore.online"))
					 .toBool());
		QCOMPARE(summary->property("countText").toString(),
				 QStringLiteral("–"));

		shell.net.setOnline(true);
		QCOMPARE(summary->property("countText").toString(),
				 QStringLiteral("Offline"));
	}

	/* An expired account marks the profile button and puts "Sign in again"
	 * first in its menu; a gone one leads to the Accounts page instead. */
	void profileButtonWarnsAboutTheAccount()
	{
		QQmlEngine engine;
		FakeAccounts accounts;
		engine.rootContext()->setContextProperty(QStringLiteral("fakeAccounts"),
												 &accounts);
		auto button = create(engine, R"(
			ProfileButton {
				name: "Steve"
				kind: "Microsoft"
				controller: fakeAccounts
				property int opened: 0
				onOpenAccountsRequested: opened += 1
			})");
		QVERIFY(button);
		QVERIFY(!button->property("attention").toBool());

		const auto firstAction = [&button]() -> QObject* {
			for (QObject* child : button->findChildren<QObject*>()) {
				const QString text = child->property("text").toString();
				if (child->inherits("QQuickItemDelegate") &&
					(text == QLatin1String("Sign in again") ||
					 text == QLatin1String("Open Accounts"))) {
					return child;
				}
			}
			return nullptr;
		};
		QVERIFY(firstAction());
		QVERIFY(!firstAction()->property("visible").toBool());

		accounts.stateKey = QStringLiteral("expired");
		accounts.error = QStringLiteral("Token expired.");
		emit accounts.changed();
		QVERIFY(button->property("attention").toBool());
		QCOMPARE(firstAction()->property("text").toString(),
				 QStringLiteral("Sign in again"));
		QVERIFY(QMetaObject::invokeMethod(firstAction(), "clicked"));
		QCOMPARE(accounts.refreshes, 1);
		QCOMPARE(button->property("opened").toInt(), 0);

		accounts.stateKey = QStringLiteral("gone");
		emit accounts.changed();
		QVERIFY(button->property("attention").toBool());
		QCOMPARE(firstAction()->property("text").toString(),
				 QStringLiteral("Open Accounts"));
		QVERIFY(QMetaObject::invokeMethod(firstAction(), "clicked"));
		QCOMPARE(accounts.refreshes, 1);
		QCOMPARE(button->property("opened").toInt(), 1);

		accounts.stateKey = QStringLiteral("online");
		emit accounts.changed();
		QVERIFY(!button->property("attention").toBool());
	}

	/* The Accounts page: the default account on its card with its capes, the
	 * others as rows that say what kind of account each is (two with one name
	 * are told apart by it), and the actions as buttons that reach the
	 * controller. */
	void accountsPageShowsKindsAndActs()
	{
		QQmlEngine engine;
		auto root = create(engine, R"(
			Item {
				id: host
				width: 1000; height: 900
				property int madeDefault: -1
				property int refreshed: -1
				property string worn: "-"
				ListModel {
					id: list
					ListElement { profileName: "GRXTOR"; name: "a@b.c"; isDefault: true; isMSA: true
						stateKey: "online"; status: "Online"; accountId: "ms"; lastError: "" }
					ListElement { profileName: ""; name: "GRXTOR"; isDefault: false; isMSA: false
						stateKey: "offline"; status: "Offline"; accountId: "off"; lastError: "" }
					ListElement { profileName: "Alex"; name: "d@e.f"; isDefault: false; isMSA: true
						stateKey: "expired"; status: "Expired"; accountId: "old"; lastError: "Token expired." }
				}
				QtObject {
					id: stub
					property var accounts: list
					property string defaultAccountId: "ms"
					property int defaultSkinsRevision: 0
					property bool skinDemoRequested: false
					property string skinDemoPose: ""
					function accountSkinInfo(row) {
						return { valid: row === 0, slim: false, hasSkin: false, defaultSkin: false,
								 currentCapeId: "", currentCapeUrl: "",
								 capes: row === 0 ? [{ id: "c1", alias: "Migrator", url: "" }] : [] }
					}
					function setDefault(row) { host.madeDefault = row }
					function refresh(row) { host.refreshed = row }
					function changeCape(row, id) { host.worn = id; return null }
					function skins(row) { return null }
				}
				AccountsPage { anchors.fill: parent; controller: stub }
			})");
		QVERIFY(root);
		auto* host = qobject_cast<QQuickItem*>(root.get());
		QVERIFY(host);

		QList<QQuickItem*> shown;
		std::function<void(QQuickItem*)> walk = [&](QQuickItem* item) {
			if (!item->isVisible()) {
				return;
			}
			shown.append(item);
			for (QQuickItem* child : item->childItems()) {
				walk(child);
			}
		};
		walk(host);
		const auto withText = [&shown](const char* type, const QString& text) {
			QList<QQuickItem*> found;
			for (QQuickItem* item : shown) {
				if (item->inherits(type) &&
					item->property("text").toString() == text) {
					found.append(item);
				}
			}
			return found;
		};

		// The hero and the expired row are Microsoft accounts, the namesake
		// is the offline one.
		QCOMPARE(withText("QQuickText", QStringLiteral("Microsoft account")).size(), 2);
		QCOMPARE(withText("QQuickText", QStringLiteral("Offline account")).size(), 1);
		QCOMPARE(withText("QQuickText", QStringLiteral("Token expired.")).size(), 1);

		// Every row but the default's offers to become it.
		const auto makeDefault =
			withText("QQuickAbstractButton", QStringLiteral("Make default"));
		QCOMPARE(makeDefault.size(), 2);
		QVERIFY(QMetaObject::invokeMethod(makeDefault.first(), "clicked"));
		QCOMPARE(host->property("madeDefault").toInt(), 1);

		// Only the expired account has "Sign in again" in the open.
		const auto signIn =
			withText("QQuickAbstractButton", QStringLiteral("Sign in again"));
		QCOMPARE(signIn.size(), 1);
		QVERIFY(QMetaObject::invokeMethod(signIn.first(), "clicked"));
		QCOMPARE(host->property("refreshed").toInt(), 2);
		QCOMPARE(withText("QQuickAbstractButton", QStringLiteral("Change skin")).size(), 1);

		// The capes of the default account: "No cape" is the one worn, and
		// picking the other asks the controller for it.
		QQuickItem* noCape = nullptr;
		QQuickItem* cape = nullptr;
		for (QQuickItem* item : shown) {
			if (!item->property("isNoCapeOption").isValid()) {
				continue;
			}
			(item->property("isNoCapeOption").toBool() ? noCape : cape) = item;
		}
		QVERIFY(noCape && cape);
		QVERIFY(noCape->property("selected").toBool());
		QCOMPARE(cape->property("label").toString(), QStringLiteral("Migrator"));
		QVERIFY(QMetaObject::invokeMethod(cape, "clicked"));
		QCOMPARE(host->property("worn").toString(), QStringLiteral("c1"));
	}
};

int main(int argc, char* argv[])
{
	qputenv("QT_QPA_PLATFORM", "offscreen");

	QGuiApplication app(argc, argv);
	AppShellQmlTest test;
	return QTest::qExec(&test, argc, argv);
}

#include "AppShellQml_test.moc"
