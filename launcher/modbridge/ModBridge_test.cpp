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

#include <QTest>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QTemporaryDir>

#include <memory>

#include "InstanceList.h"
#include "ModBridge.h"
#include "NullInstance.h"
#include "settings/INISettingsObject.h"

using namespace ModBridgeProtocol;
using FriendsApi::World;

Q_DECLARE_METATYPE(FriendsApi::World)

namespace
{
	const QString kBob = QStringLiteral("66666666-7777-8888-9999-000000000000");
	const QString kCy = QStringLiteral("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
	const QString kDee = QStringLiteral("12345678-1234-1234-1234-123456789abc");

	/* A game on Minecraft 1.8.9, as far as the bridge asks. */
	class TestInstance : public NullInstance
	{
	  public:
		using NullInstance::NullInstance;
		QString gameVersion() const override
		{
			return QStringLiteral("1.8.9");
		}
	};

	SettingsObjectPtr makeGlobalSettings(QTemporaryDir& dir)
	{
		auto settings =
			std::make_shared<INISettingsObject>(dir.filePath("global.ini"));
		settings->registerSetting("PreLaunchCommand", "");
		settings->registerSetting("WrapperCommand", "");
		settings->registerSetting("PostExitCommand", "");
		settings->registerSetting("ShowConsole", true);
		settings->registerSetting("AutoCloseConsole", false);
		settings->registerSetting("ShowConsoleOnError", true);
		settings->registerSetting("LogPrePostOutput", true);
		settings->registerSetting("ConsoleMaxLines", 100000);
		settings->registerSetting("ConsoleOverflowStop", true);
		return settings;
	}

	QVariantMap friendMap(const QString& uuid, const QString& name,
						  const QString& state, const QString& host,
						  bool joinable)
	{
		QVariantMap map;
		map["uuid"] = uuid;
		map["name"] = name;
		map["state"] = state;
		map["statusText"] = host.isEmpty() ? QStringLiteral("Offline") : "On " + host;
		map["version"] = QStringLiteral("1.8.9");
		map["worldType"] = host.isEmpty() ? QString() : QStringLiteral("server");
		map["host"] = host;
		map["port"] = host.isEmpty() ? 0 : 25565;
		map["joinable"] = joinable;
		map["face"] = QStringLiteral("file:///secret/face.png");
		return map;
	}

	/* What FriendsController would say, without one. */
	class FakeSource : public ModBridgeSource
	{
	  public:
		FakeSource()
		{
			view.rev = 3;
			view.enabled = true;
			view.state = QStringLiteral("ready");
			view.meName = QStringLiteral("grxtor");
			view.meUuid = kDee;
			view.friends = {friendMap(kBob, "Ali", "playing", "mc.hypixel.net", true),
							friendMap(kCy, "Cy", "offline", "", false)};
		}
		ModBridgeJson::FriendsView friendsView() override
		{
			return view;
		}
		void requestRefresh() override
		{
			++refreshes;
		}
		QString facePath(const QString& uuid) override
		{
			return uuid == kBob ? facePng : QString();
		}

		ModBridgeJson::FriendsView view;
		int refreshes = 0;
		QString facePng;
	};

	/* One request over a real loopback socket. */
	struct Client {
		QTcpSocket socket;
		QByteArray data;
		bool closed = false;

		Client(quint16 port, const QByteArray& raw)
		{
			QObject::connect(&socket, &QTcpSocket::readyRead,
							 [this]() { data += socket.readAll(); });
			QObject::connect(&socket, &QTcpSocket::disconnected,
							 [this]() { closed = true; });
			QObject::connect(&socket, &QTcpSocket::errorOccurred,
							 [this]() { closed = true; });
			if (!raw.isEmpty()) {
				QObject::connect(&socket, &QTcpSocket::connected,
								 [this, raw]() { socket.write(raw); });
			}
			socket.connectToHost(QHostAddress::LocalHost, port);
		}

		bool answered() const
		{
			return closed && data.contains("\r\n\r\n");
		}
		int status() const
		{
			return data.mid(9, 3).toInt();
		}
		QByteArray body() const
		{
			return data.mid(data.indexOf("\r\n\r\n") + 4);
		}
		QJsonObject json() const
		{
			return QJsonDocument::fromJson(body()).object();
		}
	};

	QByteArray raw(quint16 port, const QByteArray& method, const QByteArray& path,
				   const QString& token, const QByteArray& extra = QByteArray(),
				   const QByteArray& body = QByteArray(), bool json = true)
	{
		QByteArray out = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1:" +
						 QByteArray::number(port) + "\r\n";
		if (!token.isEmpty()) {
			out += "Authorization: Bearer " + token.toLatin1() + "\r\n";
		}
		if (method == "POST") {
			if (json) {
				out += "Content-Type: application/json\r\n";
			}
			out += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
		}
		return out + extra + "Connection: close\r\n\r\n" + body;
	}
} // namespace

class ModBridgeTest : public QObject
{
	Q_OBJECT

  private:
	QTemporaryDir m_dir;
	SettingsObjectPtr m_global;
	std::shared_ptr<TestInstance> m_game;
	std::shared_ptr<FakeSource> m_source;
	std::unique_ptr<ModBridge> m_bridge;
	ModBridge::Grant m_grant;

	std::shared_ptr<TestInstance> makeGame(const QString& id, const QString& name)
	{
		const QString root = m_dir.filePath(id);
		QDir().mkpath(root);
		auto settings = std::make_shared<INISettingsObject>(root + "/instance.cfg");
		auto game = std::make_shared<TestInstance>(m_global, settings, root);
		game->settings()->set("name", name);
		return game;
	}

	/* One answer to one request, with the session's own token. */
	Client ask(const QByteArray& method, const QByteArray& path,
			   const QByteArray& body = QByteArray(),
			   const QByteArray& extra = QByteArray())
	{
		return Client(m_grant.port,
					  raw(m_grant.port, method, path, m_grant.token, extra, body));
	}

  private slots:
	void init()
	{
		qRegisterMetaType<FriendsApi::World>();
		m_global = makeGlobalSettings(m_dir);
		m_game = makeGame("abc", "Minecraft 26.3");
		m_source = std::make_shared<FakeSource>();
		m_bridge = std::make_unique<ModBridge>("Kite 11.0.0");
		m_bridge->setSource(m_source);
		m_grant = m_bridge->registerSession(m_game.get());
		QVERIFY(m_grant.valid());
	}

	void cleanup()
	{
		m_bridge.reset();
		m_game.reset();
	}

	// ---- Sessions
	void aSessionOpensTheLoopbackSocketAndTheLastOneClosesIt()
	{
		auto other = makeGame("def", "Other");
		QVERIFY(m_bridge->listening());
		QCOMPARE(m_bridge->port(), m_grant.port);
		QCOMPARE(m_grant.token.size(), 43);
		const auto second = m_bridge->registerSession(other.get());
		QCOMPARE(second.port, m_grant.port);
		QVERIFY(second.token != m_grant.token);
		QCOMPARE(m_bridge->sessionCount(), 2);
		m_bridge->unregisterSession(m_game.get());
		QVERIFY(m_bridge->listening());
		m_bridge->unregisterSession(other.get());
		QVERIFY(!m_bridge->listening());
		QCOMPARE(m_bridge->port(), quint16(0));
		/* Idempotent. */
		m_bridge->unregisterSession(other.get());
	}

	void theSocketIsGoneWithTheLastSession()
	{
		m_bridge->unregisterSession(m_game.get());
		Client client(m_grant.port, "x");
		QTRY_VERIFY(client.closed);
		QVERIFY(client.data.isEmpty());
	}

	void anotherGamesTokenDiesWithItsSession()
	{
		auto other = makeGame("def", "Other");
		const auto second = m_bridge->registerSession(other.get());
		m_bridge->unregisterSession(m_game.get());
		auto old = ask("GET", "/v1/hello");
		QTRY_VERIFY(old.answered());
		QCOMPARE(old.status(), 401);
		Client fresh(second.port,
					 raw(second.port, "GET", "/v1/hello", second.token));
		QTRY_VERIFY(fresh.answered());
		QCOMPARE(fresh.status(), 200);
		QCOMPARE(fresh.json().value("instance").toObject().value("id").toString(),
				 QString("def"));
	}

	void aStoppedGameEndsItsSession()
	{
		emit m_game->runningStatusChanged(true);
		QCOMPARE(m_bridge->sessionCount(), 1);
		emit m_game->runningStatusChanged(false);
		QCOMPARE(m_bridge->sessionCount(), 0);
		QVERIFY(!m_bridge->listening());
	}

	void aDestroyedInstanceEndsItsSession()
	{
		m_game.reset();
		QCOMPARE(m_bridge->sessionCount(), 0);
		QVERIFY(!m_bridge->listening());
	}

	void registeringAgainReplacesTheToken()
	{
		const auto again = m_bridge->registerSession(m_game.get());
		QCOMPARE(m_bridge->sessionCount(), 1);
		QVERIFY(again.token != m_grant.token);
		auto old = ask("GET", "/v1/hello");
		QTRY_VERIFY(old.answered());
		QCOMPARE(old.status(), 401);
	}

	// ---- Authentication and the rules
	void helloAnswersWithTheGame()
	{
		auto client = ask("GET", "/v1/hello");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 200);
		QVERIFY(client.data.contains("Cache-Control: no-store"));
		QVERIFY(client.data.contains("application/json"));
		QVERIFY(!client.data.toLower().contains("access-control"));
		const auto json = client.json();
		QCOMPARE(json.value("bridge").toInt(), 1);
		QCOMPARE(json.value("launcher").toString(), QString("Kite 11.0.0"));
		const auto instance = json.value("instance").toObject();
		QCOMPARE(instance.value("id").toString(), QString("abc"));
		QCOMPARE(instance.value("name").toString(), QString("Minecraft 26.3"));
		QCOMPARE(instance.value("version").toString(), QString("1.8.9"));
		QCOMPARE(json.value("friends").toObject().value("enabled").toBool(), true);
		QCOMPARE(json.value("friends").toObject().value("state").toString(),
				 QString("ready"));
	}

	void aMissingOrWrongTokenIs401_data()
	{
		QTest::addColumn<QString>("token");
		QTest::newRow("none") << QString();
		QTest::newRow("wrong") << QString("x").repeated(43);
		QTest::newRow("prefix of the right one") << QStringLiteral("PREFIX");
	}

	void aMissingOrWrongTokenIs401()
	{
		QFETCH(QString, token);
		QString used = token == "PREFIX" ? m_grant.token.left(20) : token;
		Client client(m_grant.port, raw(m_grant.port, "GET", "/v1/hello", used));
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 401);
		QVERIFY(!client.body().contains(m_grant.token.toLatin1()));
		QVERIFY(client.data.contains("Cache-Control: no-store"));
	}

	void twentyWrongTokensLockTheBridge()
	{
		for (int i = 0; i < kMaxBadTokens; ++i) {
			Client bad(m_grant.port,
					   raw(m_grant.port, "GET", "/v1/hello", "wrong"));
			QTRY_VERIFY(bad.answered());
			QCOMPARE(bad.status(), 401);
		}
		/* Even the right token is refused now. */
		auto good = ask("GET", "/v1/hello");
		QTRY_VERIFY(good.answered());
		QCOMPARE(good.status(), 429);
	}

	void anotherHostIs421()
	{
		Client client(m_grant.port,
					  "GET /v1/hello HTTP/1.1\r\nHost: localhost:" +
						  QByteArray::number(m_grant.port) +
						  "\r\nAuthorization: Bearer " + m_grant.token.toLatin1() +
						  "\r\n\r\n");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 421);
	}

	void aBrowserIs403()
	{
		auto origin = ask("GET", "/v1/hello", {}, "Origin: http://evil.example\r\n");
		auto fetch = ask("GET", "/v1/hello", {}, "Sec-Fetch-Site: cross-site\r\n");
		QTRY_VERIFY(origin.answered());
		QTRY_VERIFY(fetch.answered());
		QCOMPARE(origin.status(), 403);
		QCOMPARE(fetch.status(), 403);
		QVERIFY(!origin.data.toLower().contains("access-control"));
	}

	void otherMethodsAre405()
	{
		auto options = ask("OPTIONS", "/v1/hello",
						   {}, "Access-Control-Request-Method: GET\r\n");
		auto put = ask("PUT", "/v1/world");
		QTRY_VERIFY(options.answered());
		QTRY_VERIFY(put.answered());
		QCOMPARE(options.status(), 405);
		QCOMPARE(put.status(), 405);
		QVERIFY(!options.data.toLower().contains("access-control"));
	}

	void aPostWithoutJsonIs415()
	{
		Client client(m_grant.port,
					  raw(m_grant.port, "POST", "/v1/world", m_grant.token,
						  "", "type=none", false));
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 415);
	}

	void transferEncodingIs400()
	{
		auto client = ask("POST", "/v1/world", {},
						  "Transfer-Encoding: chunked\r\n");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 400);
	}

	void aHugeHeadIs413()
	{
		auto client = ask("GET", "/v1/hello", {},
						  "X-Pad: " + QByteArray(kMaxHeadBytes, 'a') + "\r\n");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 413);
	}

	void aHugeBodyIs413()
	{
		auto client = ask("POST", "/v1/world", QByteArray(kMaxBodyBytes + 1, ' '));
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 413);
	}

	void unknownPathsAreNotFoundAndWrongMethodsNot()
	{
		auto missing = ask("GET", "/v1/nothing");
		auto wrong = ask("POST", "/v1/hello");
		auto wrongToo = ask("GET", "/v1/world");
		QTRY_VERIFY(missing.answered());
		QTRY_VERIFY(wrong.answered());
		QTRY_VERIFY(wrongToo.answered());
		QCOMPARE(missing.status(), 404);
		QCOMPARE(wrong.status(), 405);
		QCOMPARE(wrongToo.status(), 405);
	}

	void aSlowRequestIsDropped()
	{
		m_bridge->setRequestTimeoutMs(150);
		Client client(m_grant.port, "GET /v1/hello HTTP/1.1\r\nHost: 127.");
		QTRY_VERIFY(client.closed);
		QVERIFY(client.data.isEmpty());
		QTRY_COMPARE(m_bridge->connectionCount(), 0);
	}

	void aNinthConnectionIsClosed()
	{
		std::vector<std::unique_ptr<Client>> idle;
		for (int i = 0; i < kMaxConnections; ++i) {
			idle.push_back(std::make_unique<Client>(m_grant.port, QByteArray()));
		}
		QTRY_COMPARE(m_bridge->connectionCount(), kMaxConnections);
		Client ninth(m_grant.port, raw(m_grant.port, "GET", "/v1/hello", m_grant.token));
		QTRY_VERIFY(ninth.closed);
		QVERIFY(ninth.data.isEmpty());
		for (const auto& client : idle) {
			QVERIFY(!client->closed);
		}
		/* A place frees up. */
		idle.front()->socket.abort();
		QTRY_COMPARE(m_bridge->connectionCount(), kMaxConnections - 1);
		auto again = ask("GET", "/v1/hello");
		QTRY_VERIFY(again.answered());
		QCOMPARE(again.status(), 200);
	}

	// ---- Friends
	void friendsAreServedFromTheCacheWithTheWhitelistOnly()
	{
		auto client = ask("GET", "/v1/friends");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 200);
		QVERIFY(!client.body().contains("secret"));
		const auto json = client.json();
		QCOMPARE(json.value("rev").toInt(), 3);
		QCOMPARE(json.value("state").toString(), QString("ready"));
		QCOMPARE(json.value("me").toObject().value("name").toString(),
				 QString("grxtor"));
		const auto friends = json.value("friends").toArray();
		QCOMPARE(friends.size(), 2);
		QCOMPARE(friends[0].toObject().value("name").toString(), QString("Ali"));
		QCOMPARE(friends[0].toObject().value("world").toObject().value("host").toString(),
				 QString("mc.hypixel.net"));
		QCOMPARE(m_source->refreshes, 0);
	}

	void refreshIsAcceptedAndPassedOn()
	{
		auto client = ask("POST", "/v1/friends/refresh", "{}");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 202);
		QCOMPARE(m_source->refreshes, 1);
	}

	void withoutFriendsEverythingSaysOff()
	{
		m_bridge->setFriends(nullptr);
		auto hello = ask("GET", "/v1/hello");
		auto friends = ask("GET", "/v1/friends");
		auto join = ask("POST", "/v1/join", "{\"uuid\":\"" + kBob.toLatin1() + "\"}");
		auto refresh = ask("POST", "/v1/friends/refresh", "{}");
		QTRY_VERIFY(hello.answered());
		QTRY_VERIFY(friends.answered());
		QTRY_VERIFY(join.answered());
		QTRY_VERIFY(refresh.answered());
		QCOMPARE(hello.json().value("friends").toObject().value("enabled").toBool(),
				 false);
		QCOMPARE(hello.json().value("friends").toObject().value("state").toString(),
				 QString("off"));
		QCOMPARE(friends.json().value("enabled").toBool(), false);
		QCOMPARE(friends.json().value("friends").toArray().size(), 0);
		QCOMPARE(join.status(), 409);
		QCOMPARE(join.json().value("reason").toString(), QString("friends_off"));
		QCOMPARE(refresh.status(), 202);
	}

	// ---- Worlds
	void aReportedServerIsNormalisedAndPassedOn()
	{
		QSignalSpy spy(m_bridge.get(), &ModBridge::worldReported);
		auto client = ask("POST", "/v1/world",
						  "{\"type\":\"server\",\"host\":\"MC.Hypixel.NET\",\"port\":25565}");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 204);
		QVERIFY(client.body().isEmpty());
		QCOMPARE(spy.count(), 1);
		QCOMPARE(spy.at(0).at(0).value<BaseInstance*>(), m_game.get());
		const auto world = spy.at(0).at(1).value<World>();
		QCOMPARE(world.type, World::Type::Server);
		QCOMPARE(world.host, QString("mc.hypixel.net"));
		QCOMPARE(world.port, 25565);
	}

	void singleplayerNoneAndLanAreReported()
	{
		QSignalSpy spy(m_bridge.get(), &ModBridge::worldReported);
		for (const char* body :
			 {"{\"type\":\"singleplayer\"}", "{\"type\":\"none\"}",
			  "{\"type\":\"server\",\"host\":\"192.168.0.4\",\"port\":25565}"}) {
			auto client = ask("POST", "/v1/world", body);
			QTRY_VERIFY(client.answered());
			QCOMPARE(client.status(), 204);
		}
		QCOMPARE(spy.count(), 3);
		QCOMPARE(spy.at(0).at(1).value<World>().type, World::Type::Singleplayer);
		QCOMPARE(spy.at(1).at(1).value<World>().type, World::Type::None);
		QCOMPARE(spy.at(2).at(1).value<World>().type, World::Type::None);
	}

	void aBadWorldIs400AndReportsNothing()
	{
		QSignalSpy spy(m_bridge.get(), &ModBridge::worldReported);
		auto client = ask("POST", "/v1/world", "{\"type\":\"realm\"}");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 400);
		QCOMPARE(spy.count(), 0);
	}

	// ---- Joining
	void joinAnswersFromTheCache()
	{
		auto ok = ask("POST", "/v1/join", "{\"uuid\":\"" + kBob.toLatin1() + "\"}");
		QTRY_VERIFY(ok.answered());
		QCOMPARE(ok.status(), 200);
		QCOMPARE(ok.json().value("ok").toBool(), true);
		QCOMPARE(ok.json().value("host").toString(), QString("mc.hypixel.net"));
		QCOMPARE(ok.json().value("port").toInt(), 25565);
		QCOMPARE(ok.json().value("version").toString(), QString("1.8.9"));
		/* The game runs 1.8.9 too. */
		QCOMPARE(ok.json().value("versionMatch").toBool(), true);
	}

	void joinRefusalsAre409WithAReason()
	{
		auto offline = ask("POST", "/v1/join", "{\"uuid\":\"" + kCy.toLatin1() + "\"}");
		auto unknown = ask("POST", "/v1/join", "{\"uuid\":\"" + kDee.toLatin1() + "\"}");
		QTRY_VERIFY(offline.answered());
		QTRY_VERIFY(unknown.answered());
		QCOMPARE(offline.status(), 409);
		QCOMPARE(offline.json().value("ok").toBool(), false);
		QCOMPARE(offline.json().value("reason").toString(), QString("offline"));
		QCOMPARE(unknown.status(), 409);
		QCOMPARE(unknown.json().value("reason").toString(), QString("unknown_friend"));

		m_source->view.friends = {friendMap(kBob, "Ali", "playing", "mc.hypixel.net", false)};
		auto notJoinable = ask("POST", "/v1/join", "{\"uuid\":\"" + kBob.toLatin1() + "\"}");
		QTRY_VERIFY(notJoinable.answered());
		QCOMPARE(notJoinable.json().value("reason").toString(), QString("not_joinable"));

		m_source->view.enabled = false;
		m_source->view.state = "off";
		auto off = ask("POST", "/v1/join", "{\"uuid\":\"" + kBob.toLatin1() + "\"}");
		QTRY_VERIFY(off.answered());
		QCOMPARE(off.json().value("reason").toString(), QString("friends_off"));
	}

	void aBadJoinIs400()
	{
		auto client = ask("POST", "/v1/join", "{\"uuid\":\"../../etc\"}");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 400);
	}

	// ---- Faces
	void aFaceIsServedAsAPng()
	{
		QImage image(8, 8, QImage::Format_ARGB32);
		image.fill(Qt::red);
		m_source->facePng = m_dir.filePath("face.png");
		QVERIFY(image.save(m_source->facePng, "PNG"));
		auto client = ask("GET", "/v1/faces/" + kBob.toLatin1() + ".png");
		QTRY_VERIFY(client.answered());
		QCOMPARE(client.status(), 200);
		QVERIFY(client.data.contains("Content-Type: image/png"));
		QImage got;
		QVERIFY(got.loadFromData(client.body(), "PNG"));
		QCOMPARE(got.size(), QSize(8, 8));
	}

	void noFaceIs404()
	{
		auto none = ask("GET", "/v1/faces/" + kCy.toLatin1() + ".png");
		auto traversal = ask("GET", "/v1/faces/../../etc/passwd.png");
		QTRY_VERIFY(none.answered());
		QTRY_VERIFY(traversal.answered());
		QCOMPARE(none.status(), 404);
		QCOMPARE(traversal.status(), 404);
	}
};

QTEST_GUILESS_MAIN(ModBridgeTest)

#include "ModBridge_test.moc"
