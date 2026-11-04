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

#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>
#include <QWebSocket>
#include <QWebSocketServer>

#include <memory>

#include "FriendsSocket.h"

namespace
{
	const QString kToken = QStringLiteral("kfs_test");
	const QString kReady = QStringLiteral(
		R"({"type":"ready","user":{"uuid":"069a79f4-44e9-4726-a5be-fca90e38aaf5","name":"Notch"}})");
	/* Waits that are long next to the test timings, short next to QTRY's 5 s. */
	constexpr int kQuietMs = 300;

	FriendsSocket::Timing fastTiming()
	{
		FriendsSocket::Timing timing;
		timing.pingMs = 100;
		timing.silenceMs = 2000;
		timing.firstRetryMs = 20;
		timing.maxRetryMs = 200;
		timing.crowdedRetryMs = 1000;
		return timing;
	}

	QWebSocketProtocol::CloseCode code(int value)
	{
		return static_cast<QWebSocketProtocol::CloseCode>(value);
	}

	/* The friends server's socket as far as the client can tell: says `ready`
	 * unless told not to, and keeps what it was sent. */
	class FakeServer : public QObject
	{
	  public:
		QWebSocketServer server{QStringLiteral("fake"), QWebSocketServer::NonSecureMode};
		QList<QWebSocket*> sockets;
		QList<QByteArray> authorizations;
		QList<QUrl> urls;
		QStringList received;
		bool sayReady = true;

		FakeServer()
		{
			connect(&server, &QWebSocketServer::newConnection, this, [this]() {
				while (QWebSocket* socket = server.nextPendingConnection()) {
					sockets.append(socket);
					authorizations.append(socket->request().rawHeader("Authorization"));
					urls.append(socket->requestUrl());
					connect(socket, &QWebSocket::textMessageReceived, this,
							[this](const QString& text) { received.append(text); });
					if (sayReady) {
						socket->sendTextMessage(kReady);
					}
				}
			});
		}
		bool listen(quint16 port = 0)
		{
			return server.listen(QHostAddress::LocalHost, port);
		}
		QUrl url() const
		{
			return QUrl(QStringLiteral("ws://127.0.0.1:%1/v1/ws").arg(server.serverPort()));
		}
		QWebSocket* last() const
		{
			return sockets.isEmpty() ? nullptr : sockets.last();
		}
	};
} // namespace

class FriendsSocketTest : public QObject
{
	Q_OBJECT

	std::unique_ptr<FakeServer> m_server;
	std::unique_ptr<FriendsSocket> m_socket;

	void startClient(const FriendsSocket::Timing& timing = fastTiming())
	{
		m_socket->setTiming(timing);
		m_socket->start(m_server->url(), kToken);
	}

  private slots:
	void init()
	{
		m_server = std::make_unique<FakeServer>();
		QVERIFY(m_server->listen());
		m_socket = std::make_unique<FriendsSocket>();
	}

	void cleanup()
	{
		m_socket.reset();
		m_server.reset();
	}

	void theUrlAndDelaysFollowTheServer()
	{
		QCOMPARE(FriendsSocket::socketUrl(QStringLiteral("https://api.kiteclient.com/")),
				 QUrl(QStringLiteral("wss://api.kiteclient.com/v1/ws")));
		QCOMPARE(FriendsSocket::socketUrl(QStringLiteral("http://127.0.0.1:3921")),
				 QUrl(QStringLiteral("ws://127.0.0.1:3921/v1/ws")));
		const FriendsSocket::Timing timing;
		QCOMPARE(FriendsSocket::retryDelayMs(0, timing, 0.0), 1000);
		QCOMPARE(FriendsSocket::retryDelayMs(1, timing, 0.0), 2000);
		QCOMPARE(FriendsSocket::retryDelayMs(2, timing, 0.0), 4000);
		QCOMPARE(FriendsSocket::retryDelayMs(9, timing, 0.0), 60000);
		QCOMPARE(FriendsSocket::retryDelayMs(200, timing, 0.0), 60000);
		QCOMPARE(FriendsSocket::retryDelayMs(9, timing, 1.0), 30000);
	}

	void itSignsInWithTheTokenInTheHeader()
	{
		QSignalSpy up(m_socket.get(), &FriendsSocket::upChanged);
		startClient();
		QTRY_COMPARE(m_server->sockets.size(), 1);
		QCOMPARE(m_server->authorizations.first(), QByteArray("Bearer kfs_test"));
		QVERIFY(!m_server->urls.first().toString().contains(kToken));
		QTRY_VERIFY(m_socket->isUp());
		QCOMPARE(up.size(), 1);
	}

	void eventsArriveAndFramesGoOut()
	{
		QSignalSpy frames(m_socket.get(), &FriendsSocket::frameReceived);
		startClient();
		QTRY_VERIFY(m_socket->isUp());
		m_server->last()->sendTextMessage(
			QStringLiteral(R"({"type":"friend.status","uuid":"x","status":{"state":"online"}})"));
		m_server->last()->sendTextMessage(QStringLiteral("not json"));
		QTRY_COMPARE(frames.size(), 1);
		QCOMPARE(frames.first().first().toJsonObject().value("type").toString(),
				 QStringLiteral("friend.status"));

		QVERIFY(m_socket->send({{"type", "chat.read"}, {"with", "x"}, {"upTo", "1"}}));
		QTRY_COMPARE(m_server->received.size(), 1);
		QCOMPARE(QJsonDocument::fromJson(m_server->received.first().toUtf8())
					 .object().value("type").toString(),
				 QStringLiteral("chat.read"));
	}

	void nothingGoesOutBeforeReady()
	{
		m_server->sayReady = false;
		startClient();
		QTRY_COMPARE(m_server->sockets.size(), 1);
		QTest::qWait(kQuietMs);
		QVERIFY(!m_socket->isUp());
		QVERIFY(!m_socket->send({{"type", "chat.read"}}));
	}

	void anyOtherCloseReconnects()
	{
		QSignalSpy up(m_socket.get(), &FriendsSocket::upChanged);
		startClient();
		QTRY_VERIFY(m_socket->isUp());
		m_server->last()->close(QWebSocketProtocol::CloseCodeGoingAway);
		QTRY_COMPARE(m_server->sockets.size(), 2);
		QTRY_VERIFY(m_socket->isUp());
		QCOMPARE(up.size(), 3);
		QCOMPARE(m_server->authorizations.last(), QByteArray("Bearer kfs_test"));
	}

	void aServerThatIsDownIsTriedAgain()
	{
		startClient();
		QTRY_VERIFY(m_socket->isUp());
		const quint16 port = m_server->server.serverPort();
		m_server->server.close();
		m_server->last()->close(QWebSocketProtocol::CloseCodeGoingAway);
		QTRY_VERIFY(!m_socket->isUp());
		QTest::qWait(kQuietMs);
		QVERIFY(m_server->listen(port));
		QTRY_COMPARE(m_server->sockets.size(), 2);
		QTRY_VERIFY(m_socket->isUp());
	}

	void anEndedSessionStopsAndSaysSo()
	{
		QSignalSpy ended(m_socket.get(), &FriendsSocket::sessionEnded);
		startClient();
		QTRY_VERIFY(m_socket->isUp());
		m_server->last()->close(code(FriendsSocket::kCloseSessionEnded));
		QTRY_COMPARE(ended.size(), 1);
		QVERIFY(!m_socket->running());
		QVERIFY(!m_socket->isUp());
		QTest::qWait(kQuietMs);
		QCOMPARE(m_server->sockets.size(), 1);
	}

	void tooManySocketsWaitsLonger()
	{
		startClient();
		QTRY_VERIFY(m_socket->isUp());
		m_server->last()->close(code(FriendsSocket::kCloseTooManySockets));
		QTest::qWait(kQuietMs);
		QCOMPARE(m_server->sockets.size(), 1);
		QTRY_COMPARE(m_server->sockets.size(), 2);
	}

	void pongsKeepAQuietSocketOpen()
	{
		auto timing = fastTiming();
		timing.silenceMs = 250;
		startClient(timing);
		QTRY_VERIFY(m_socket->isUp());
		QTest::qWait(kQuietMs * 3);
		QCOMPARE(m_server->sockets.size(), 1);
		QVERIFY(m_socket->isUp());
	}

	void aSilentSocketIsDropped()
	{
		auto timing = fastTiming();
		/* No pings, so no pongs: nothing is heard after `ready`. */
		timing.pingMs = 60 * 1000;
		timing.silenceMs = 200;
		startClient(timing);
		QTRY_VERIFY(m_socket->isUp());
		QTRY_COMPARE(m_server->sockets.size(), 2);
	}

	void stopClosesForGood()
	{
		startClient();
		QTRY_VERIFY(m_socket->isUp());
		QWebSocket* server = m_server->last();
		QSignalSpy closed(server, &QWebSocket::disconnected);
		m_socket->stop();
		QVERIFY(!m_socket->isUp());
		QTRY_COMPARE(closed.size(), 1);
		QTest::qWait(kQuietMs);
		QCOMPARE(m_server->sockets.size(), 1);
	}

	void aNewTokenIsANewSocket()
	{
		startClient();
		QTRY_VERIFY(m_socket->isUp());
		m_socket->start(m_server->url(), kToken);
		QTest::qWait(kQuietMs);
		QCOMPARE(m_server->sockets.size(), 1);
		m_socket->start(m_server->url(), QStringLiteral("kfs_new"));
		QTRY_COMPARE(m_server->sockets.size(), 2);
		QCOMPARE(m_server->authorizations.last(), QByteArray("Bearer kfs_new"));
	}
};

QTEST_GUILESS_MAIN(FriendsSocketTest)

#include "FriendsSocket_test.moc"
