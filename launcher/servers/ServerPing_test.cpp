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

#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTest>

#include <memory>
#include <vector>

#include "servers/ServerPing.h"
#include "servers/ServerPinger.h"

namespace
{
	/// Short enough to keep the suite quick, long enough for a loaded CI box.
	constexpr int kTestTimeoutMs = 400;
	constexpr int kWaitMs = 3000;
	const QByteArray kStatusRequest = QByteArray::fromHex("0100");
	constexpr int kPingPacketBytes = 10;

	const QByteArray kStatus = R"({
		"version": { "name": "Fake 1.21.4", "protocol": 769 },
		"players": { "max": 100, "online": 12 },
		"description": { "text": "Hello", "color": "gold" }
	})";

	/* A Minecraft server on 127.0.0.1 that answers the way `mode` says and
	 * remembers what it was sent. */
	class FakeServer : public QObject
	{
	  public:
		enum class Mode {
			Normal,
			/// Answers the status, never the ping.
			NoPong,
			/// Answers the status and hangs up.
			HangUp,
			/// Announces a packet far over the limit.
			Oversized,
			/// A status packet whose string is not JSON.
			Malformed,
			/// Accepts and says nothing.
			Silent,
		};

		explicit FakeServer(Mode mode, QByteArray status = kStatus)
			: m_mode(mode), m_status(std::move(status))
		{
			m_server.listen(QHostAddress::LocalHost);
			connect(&m_server, &QTcpServer::newConnection, this, [this] {
				while (QTcpSocket* socket = m_server.nextPendingConnection()) {
					++connections;
					// One client at a time: a new one starts over.
					m_answered = false;
					connect(socket, &QTcpSocket::readyRead, this,
							[this, socket] { onData(socket); });
				}
			});
		}

		quint16 port() const
		{
			return m_server.serverPort();
		}
		QString address() const
		{
			return QStringLiteral("127.0.0.1:%1").arg(port());
		}

		int connections = 0;
		QByteArray received;

	  private:
		void onData(QTcpSocket* socket)
		{
			received += socket->readAll();
			if (!m_answered && received.endsWith(kStatusRequest)) {
				m_answered = true;
				answerStatus(socket);
				return;
			}
			if (m_answered && m_mode == Mode::Normal &&
				received.size() >= m_pingAt + kPingPacketBytes) {
				socket->write(received.mid(m_pingAt, kPingPacketBytes));
			}
		}

		void answerStatus(QTcpSocket* socket)
		{
			m_pingAt = received.size();
			switch (m_mode) {
				case Mode::Silent:
					return;
				case Mode::Oversized:
					socket->write(ServerPing::varInt(ServerPing::kMaxPacketBytes + 1) +
								  QByteArray(64, 'x'));
					return;
				case Mode::Malformed:
					socket->write(ServerPing::packet(0, ServerPing::varInt(8) + "not json"));
					return;
				default:
					break;
			}
			socket->write(ServerPing::packet(
				0, ServerPing::varInt(int(m_status.size())) + m_status));
			if (m_mode == Mode::HangUp) {
				socket->disconnectFromHost();
			}
		}

		Mode m_mode;
		QByteArray m_status;
		QTcpServer m_server;
		bool m_answered = false;
		int m_pingAt = 0;
	};

	struct Outcome {
		bool succeeded = false;
		ServerStatus::Status status;
		QString reason;
	};

	Outcome pingOnce(const FakeServer& server)
	{
		ServerPing ping(QStringLiteral("127.0.0.1"), server.port(), true);
		ping.setTimeoutMs(kTestTimeoutMs);
		Outcome outcome;
		int ends = 0;
		QObject::connect(&ping, &ServerPing::succeeded,
						 [&](const ServerStatus::Status& status) {
							 outcome.succeeded = true;
							 outcome.status = status;
							 ++ends;
						 });
		QObject::connect(&ping, &ServerPing::failed, [&](const QString& reason) {
			outcome.reason = reason;
			++ends;
		});
		ping.start();
		// A ping that never ends is caught by the count below.
		(void)QTest::qWaitFor([&ends] { return ends > 0; }, kWaitMs);
		// Exactly one ending, and nothing after it.
		QTest::qWait(kTestTimeoutMs / 2);
		if (ends != 1) {
			outcome.reason = QStringLiteral("ended %1 times").arg(ends);
			outcome.succeeded = false;
		}
		return outcome;
	}

	/// A port nothing listens on.
	quint16 closedPort()
	{
		QTcpServer server;
		server.listen(QHostAddress::LocalHost);
		const quint16 port = server.serverPort();
		server.close();
		return port;
	}

	QVariantMap friendOn(const QString& name, const QString& host, int port,
						 const QString& state = QStringLiteral("playing"),
						 const QString& world = QStringLiteral("server"))
	{
		return {{"name", name}, {"state", state}, {"worldType", world},
				{"host", host}, {"port", port}};
	}
} // namespace

class ServerPingTest : public QObject
{
	Q_OBJECT

  private slots:
	void encodesVarIntsLikeTheGame()
	{
		QCOMPARE(ServerPing::varInt(0), QByteArray::fromHex("00"));
		QCOMPARE(ServerPing::varInt(127), QByteArray::fromHex("7f"));
		QCOMPARE(ServerPing::varInt(300), QByteArray::fromHex("ac02"));
		QCOMPARE(ServerPing::varInt(-1), QByteArray::fromHex("ffffffff0f"));
	}

	void theHandshakeNamesTheHostAndAsksForTheStatus()
	{
		const QByteArray handshake = ServerPing::handshake("mc.example.com", 25565);
		// length, id 0, protocol -1, "mc.example.com", port 25565, next state 1.
		const QByteArray expected = QByteArray::fromHex("00ffffffff0f0e") +
									"mc.example.com" + QByteArray::fromHex("63dd01");
		QCOMPARE(handshake, ServerPing::varInt(int(expected.size())) + expected);
	}

	void readsANormalAnswer()
	{
		FakeServer server(FakeServer::Mode::Normal);
		const Outcome outcome = pingOnce(server);
		QVERIFY2(outcome.succeeded, qPrintable(outcome.reason));
		QCOMPARE(outcome.status.online, 12);
		QCOMPARE(outcome.status.max, 100);
		QCOMPARE(outcome.status.version, QString("Fake 1.21.4"));
		QCOMPARE(outcome.status.motdText, QString("Hello"));
		QVERIFY(outcome.status.latencyMs >= 0);
		QVERIFY(outcome.status.latencyMs < kTestTimeoutMs);
		// What the server heard: the handshake for the host as given, then the request.
		QVERIFY(server.received.startsWith(
			ServerPing::handshake("127.0.0.1", server.port()) + kStatusRequest));
	}

	void aServerWithoutAPongIsStillTimed()
	{
		FakeServer server(FakeServer::Mode::NoPong);
		const Outcome outcome = pingOnce(server);
		QVERIFY2(outcome.succeeded, qPrintable(outcome.reason));
		QCOMPARE(outcome.status.online, 12);
		QVERIFY(outcome.status.latencyMs >= 0);
	}

	void aServerThatHangsUpAfterTheStatusSucceeds()
	{
		FakeServer server(FakeServer::Mode::HangUp);
		const Outcome outcome = pingOnce(server);
		QVERIFY2(outcome.succeeded, qPrintable(outcome.reason));
		QCOMPARE(outcome.status.motdText, QString("Hello"));
	}

	void refusesAnOversizedAnswer()
	{
		FakeServer server(FakeServer::Mode::Oversized);
		const Outcome outcome = pingOnce(server);
		QVERIFY(!outcome.succeeded);
		QVERIFY2(outcome.reason.contains("too long"), qPrintable(outcome.reason));
	}

	void refusesAMalformedAnswer()
	{
		FakeServer server(FakeServer::Mode::Malformed);
		const Outcome outcome = pingOnce(server);
		QVERIFY(!outcome.succeeded);
		QVERIFY(!outcome.reason.isEmpty());
	}

	void givesUpOnASilentServer()
	{
		FakeServer server(FakeServer::Mode::Silent);
		QElapsedTimer clock;
		clock.start();
		const Outcome outcome = pingOnce(server);
		QVERIFY(!outcome.succeeded);
		QVERIFY2(outcome.reason.contains("in time"), qPrintable(outcome.reason));
		QVERIFY(clock.elapsed() >= kTestTimeoutMs);
	}

	void failsWhenNothingListens()
	{
		ServerPing ping(QStringLiteral("127.0.0.1"), closedPort(), false);
		ping.setTimeoutMs(kTestTimeoutMs);
		QSignalSpy failed(&ping, &ServerPing::failed);
		ping.start();
		QVERIFY(failed.wait(kWaitMs));
	}

	void keysIgnoreCaseTrailingDotsAndTheDefaultPort()
	{
		const QString key = ServerPinger::keyOf("mc.example.com");
		QCOMPARE(key, QString("mc.example.com:25565"));
		QCOMPARE(ServerPinger::keyOf(" MC.Example.com. "), key);
		QCOMPARE(ServerPinger::keyOf("mc.example.com:25565"), key);
		QCOMPARE(ServerPinger::keyOf("mc.example.com", 25565), key);
		QVERIFY(ServerPinger::keyOf("mc.example.com:25570") != key);
		QCOMPARE(ServerPinger::keyOf("[2001:DB8::5]:25570"), QString("[2001:db8::5]:25570"));
		QCOMPARE(ServerPinger::keyOf("2001:db8::5", 25570), QString("[2001:db8::5]:25570"));
		QVERIFY(ServerPinger::keyOf("").isEmpty());
		QVERIFY(ServerPinger::keyOf("  ").isEmpty());
	}

	void anAnswerServesEveryAskerForAWhile()
	{
		FakeServer server(FakeServer::Mode::Normal);
		ServerPinger pinger;
		pinger.setTimeoutMs(kTestTimeoutMs);
		QCOMPARE(pinger.status(server.address()).value("state").toString(), QString());

		pinger.ping(server.address());
		QCOMPARE(pinger.status(server.address()).value("state").toString(), QString("pending"));
		// A second ask while the first is out starts nothing.
		pinger.ping(server.address());
		QTRY_COMPARE_WITH_TIMEOUT(pinger.status(server.address()).value("state").toString(),
								  QString("online"), kWaitMs);
		const QVariantMap status = pinger.status(server.address());
		QCOMPARE(status.value("online").toInt(), 12);
		QCOMPARE(status.value("max").toInt(), 100);
		QCOMPARE(status.value("motdText").toString(), QString("Hello"));
		QVERIFY(status.value("motd").toString().contains("color"));
		QCOMPARE(status.value("refreshing").toBool(), false);

		// Cached: asking again, by another spelling, does not reach the server.
		pinger.ping(QStringLiteral("127.0.0.1.:%1").arg(server.port()));
		pinger.ping(server.address());
		QTest::qWait(kTestTimeoutMs / 2);
		QCOMPARE(server.connections, 1);
	}

	void refreshAsksAgainButNotTooOften()
	{
		FakeServer server(FakeServer::Mode::HangUp);
		ServerPinger pinger;
		pinger.setTimeoutMs(kTestTimeoutMs);
		pinger.ping(server.address());
		QTRY_COMPARE_WITH_TIMEOUT(pinger.status(server.address()).value("state").toString(),
								  QString("online"), kWaitMs);
		// Within the floor: nothing.
		pinger.refresh(server.address());
		QTest::qWait(kTestTimeoutMs / 2);
		QCOMPARE(server.connections, 1);

		pinger.setRefreshFloorMs(0);
		const int revision = pinger.revision();
		pinger.refresh(server.address());
		// What was shown stays while the new ping is out.
		QCOMPARE(pinger.status(server.address()).value("state").toString(), QString("online"));
		QCOMPARE(pinger.status(server.address()).value("refreshing").toBool(), true);
		QVERIFY(pinger.revision() > revision);
		QTRY_COMPARE_WITH_TIMEOUT(server.connections, 2, kWaitMs);
	}

	void anUnreachableServerIsOffline()
	{
		ServerPinger pinger;
		pinger.setTimeoutMs(kTestTimeoutMs);
		const QString address = QStringLiteral("127.0.0.1:%1").arg(closedPort());
		pinger.ping(address);
		QTRY_COMPARE_WITH_TIMEOUT(pinger.status(address).value("state").toString(),
								  QString("offline"), kWaitMs);
		QVERIFY(!pinger.status(address).contains("online"));
	}

	void onlyAFewPingsRunAtOnce()
	{
		std::vector<std::unique_ptr<FakeServer>> servers;
		const int count = ServerPinger::kMaxConcurrent + 2;
		ServerPinger pinger;
		pinger.setTimeoutMs(kTestTimeoutMs);
		for (int i = 0; i < count; ++i) {
			servers.push_back(std::make_unique<FakeServer>(FakeServer::Mode::Silent));
			pinger.ping(servers.back()->address());
		}
		const auto connected = [&servers] {
			int total = 0;
			for (const auto& server : servers) {
				total += server->connections;
			}
			return total;
		};
		QTRY_COMPARE_WITH_TIMEOUT(connected(), ServerPinger::kMaxConcurrent, kWaitMs);
		QTest::qWait(kTestTimeoutMs / 4);
		QCOMPARE(connected(), ServerPinger::kMaxConcurrent);
		// The rest start as the first ones time out.
		QTRY_COMPARE_WITH_TIMEOUT(connected(), count, kWaitMs);
		for (const auto& server : servers) {
			QTRY_COMPARE_WITH_TIMEOUT(pinger.status(server->address()).value("state").toString(),
									  QString("offline"), kWaitMs);
		}
	}

	void findsTheFriendsOnAServer()
	{
		ServerPinger pinger;
		const QVariantList friends = {
			friendOn("Alex", "mc.example.com", 0),
			friendOn("Steve", "MC.EXAMPLE.COM.", 25565),
			friendOn("Kai", "mc.example.com", 25570),
			friendOn("Noor", "other.example.com", 0),
			friendOn("Ari", "", 0, "online", ""),
			friendOn("Efe", "mc.example.com", 0, "playing", "singleplayer"),
			friendOn("Zuri", "mc.example.com", 0, "online", "server"),
		};
		const QVariantList here = pinger.friendsOn("mc.example.com", friends);
		QCOMPARE(here.size(), 2);
		QCOMPARE(here.at(0).toMap().value("name").toString(), QString("Alex"));
		QCOMPARE(here.at(1).toMap().value("name").toString(), QString("Steve"));
		const QVariantList other = pinger.friendsOn("mc.example.com:25570", friends);
		QCOMPARE(other.size(), 1);
		QCOMPARE(other.at(0).toMap().value("name").toString(), QString("Kai"));
		QVERIFY(pinger.friendsOn("", friends).isEmpty());
	}
};

QTEST_GUILESS_MAIN(ServerPingTest)

#include "ServerPing_test.moc"
