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
 * Two players chatting through a real friends server, each with the
 * launcher's own FriendsSocket and FriendsChat wired as FriendsRealtime.cpp
 * wires them. Skipped unless KITE_FRIENDS_E2E_URL is the server's URL; the
 * server's MOJANG_SESSION_URL must say yes to every hasJoined (a fake on
 * loopback), so a challenge alone signs a name in.
 */

#include <QEventLoop>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QTest>

#include <memory>

#include "FriendsApi.h"
#include "FriendsChat.h"
#include "FriendsSocket.h"

namespace
{
	struct Player {
		QString name;
		QString uuid;
		QString token;
		std::unique_ptr<FriendsSocket> socket;
		std::unique_ptr<FriendsChat> chat;
		QStringList finished;
	};
} // namespace

class FriendsChatE2ETest : public QObject
{
	Q_OBJECT

	QString m_url;
	QNetworkAccessManager m_network;
	Player m_alex;
	Player m_jeb;

	void send(const QByteArray& verb, const QString& path, const QByteArray& body,
			  const QString& token, std::function<void(const FriendsApi::Reply&)> done)
	{
		QNetworkRequest request(FriendsApi::url(m_url, path));
		if (!body.isEmpty()) {
			request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
		}
		if (!token.isEmpty()) {
			request.setRawHeader("Authorization", "Bearer " + token.toUtf8());
		}
		QNetworkReply* reply = m_network.sendCustomRequest(request, verb, body);
		connect(reply, &QNetworkReply::finished, this, [reply, done]() {
			FriendsApi::Reply answer;
			answer.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
			answer.body = reply->readAll();
			answer.retryAfterSeconds = reply->rawHeader("Retry-After").toInt();
			reply->deleteLater();
			done(answer);
		});
	}

	FriendsApi::Reply call(const QByteArray& verb, const QString& path, const QByteArray& body,
						   const QString& token = QString())
	{
		FriendsApi::Reply answer;
		QEventLoop loop;
		send(verb, path, body, token, [&](const FriendsApi::Reply& reply) {
			answer = reply;
			loop.quit();
		});
		loop.exec();
		return answer;
	}

	bool signIn(Player& player, const QString& name)
	{
		player.name = name;
		const auto asked = call("POST", FriendsApi::Path::challenge(), FriendsApi::challengeBody(name));
		const auto challenge = FriendsApi::parseChallenge(asked.body);
		if (!challenge) {
			qWarning() << "challenge:" << asked.status << asked.body;
			return false;
		}
		const auto login = call("POST", FriendsApi::Path::login(), FriendsApi::loginBody(challenge->challengeId));
		const auto session = FriendsApi::parseSession(login.body);
		if (!session) {
			qWarning() << "login:" << login.status << login.body;
			return false;
		}
		player.uuid = session->uuid.toLower();
		player.token = session->token;
		return true;
	}

	/* As FriendsController::setUpRealtime() does it. */
	void wire(Player& player)
	{
		player.socket = std::make_unique<FriendsSocket>();
		player.chat = std::make_unique<FriendsChat>();
		FriendsSocket* socket = player.socket.get();
		FriendsChat* chat = player.chat.get();
		connect(socket, &FriendsSocket::upChanged, chat, &FriendsChat::setLive);
		connect(socket, &FriendsSocket::frameReceived, chat, &FriendsChat::handleFrame);
		FriendsChat::Transport transport;
		transport.sendFrame = [socket](const QJsonObject& frame) { return socket->send(frame); };
		const QString token = player.token;
		transport.request = [this, token](const QByteArray& verb, const QString& path,
										  const QByteArray& body,
										  std::function<void(const FriendsApi::Reply&)> done) {
			send(verb, path, body, token, std::move(done));
		};
		QStringList* finished = &player.finished;
		transport.finished = [finished](const QString& action, bool ok, const QString&) {
			finished->append(action + (ok ? QStringLiteral(":ok") : QStringLiteral(":failed")));
		};
		chat->setTransport(std::move(transport));
		chat->setMe(player.uuid);
		socket->start(FriendsSocket::socketUrl(m_url), player.token);
	}

	QString newestText(Player& player)
	{
		auto* model = player.chat->messages();
		return model->rowCount() > 0
				   ? model->data(model->index(0), FriendsChatModel::TextRole).toString()
				   : QString();
	}
	QString newestState(Player& player)
	{
		auto* model = player.chat->messages();
		return model->rowCount() > 0
				   ? model->data(model->index(0), FriendsChatModel::StateRole).toString()
				   : QString();
	}

	int unreadFrom(Player& reader, const Player& sender)
	{
		const auto lists = FriendsApi::parseFriends(
			call("GET", FriendsApi::Path::friends(), QByteArray(), reader.token).body);
		for (const auto& one : lists ? lists->friends : QList<FriendsApi::Friend>()) {
			if (one.uuid.toLower() == sender.uuid) {
				return one.unread;
			}
		}
		return -1;
	}

  private slots:
	void initTestCase()
	{
		m_url = qEnvironmentVariable("KITE_FRIENDS_E2E_URL");
		if (m_url.isEmpty()) {
			QSKIP("KITE_FRIENDS_E2E_URL is not set");
		}
		QVERIFY(signIn(m_alex, QStringLiteral("AlexE2E")));
		QVERIFY(signIn(m_jeb, QStringLiteral("JebE2E")));
		const auto asked = call("POST", FriendsApi::Path::requests(),
								FriendsApi::requestBody(m_jeb.name), m_alex.token);
		QVERIFY2(asked.ok() || asked.status == 409, asked.body.constData());
		const auto accepted = call("POST", FriendsApi::Path::accept(m_alex.uuid), QByteArray(), m_jeb.token);
		QVERIFY2(accepted.ok() || accepted.status == 404, accepted.body.constData());
		wire(m_alex);
		wire(m_jeb);
		QTRY_VERIFY(m_alex.socket->isUp());
		QTRY_VERIFY(m_jeb.socket->isUp());
		m_alex.chat->open(m_jeb.uuid);
		m_jeb.chat->open(m_alex.uuid);
		QTRY_VERIFY(m_alex.chat->loaded());
		QTRY_VERIFY(m_jeb.chat->loaded());
	}

	void aMessageReachesTheFriendAtOnce()
	{
		QSignalSpy received(m_jeb.chat.get(), &FriendsChat::messageReceived);
		const QString text = QStringLiteral("§6gold stays plain %1").arg(QDateTime::currentMSecsSinceEpoch());
		QVERIFY(m_alex.chat->send(text + QStringLiteral("\n")));
		QCOMPARE(newestState(m_alex), QStringLiteral("sending"));
		QTRY_COMPARE(newestState(m_alex), QStringLiteral("sent"));
		QTRY_COMPARE(received.size(), 1);
		QCOMPARE(newestText(m_jeb), text);
		QCOMPARE(unreadFrom(m_jeb, m_alex), 1);
	}

	void theOpenConversationIsMarkedRead()
	{
		QSignalSpy read(m_jeb.chat.get(), &FriendsChat::markedRead);
		m_jeb.chat->setViewing(true);
		QTRY_COMPARE(read.size(), 1);
		QTRY_COMPARE(unreadFrom(m_jeb, m_alex), 0);
	}

	void withoutItsSocketAPlayerSendsOverHttp()
	{
		m_jeb.socket->stop();
		QVERIFY(m_jeb.chat->send(QStringLiteral("over http")));
		QTRY_COMPARE(newestState(m_jeb), QStringLiteral("sent"));
		QTRY_COMPARE(newestText(m_alex), QStringLiteral("over http"));
	}

	void aReconnectCatchesUpWhatWasMissed()
	{
		m_alex.socket->stop();
		QTRY_VERIFY(!m_alex.socket->isUp());
		QVERIFY(call("POST", FriendsChatApi::Path::messages(m_alex.uuid),
					 FriendsChatApi::sendBody(QStringLiteral("missed one")), m_jeb.token).ok());
		QVERIFY(call("POST", FriendsChatApi::Path::messages(m_alex.uuid),
					 FriendsChatApi::sendBody(QStringLiteral("missed two")), m_jeb.token).ok());
		m_alex.socket->start(FriendsSocket::socketUrl(m_url), m_alex.token);
		QTRY_COMPARE(newestText(m_alex), QStringLiteral("missed two"));
	}

	void aMessageCanBeReported()
	{
		auto* model = m_alex.chat->messages();
		const QString id = model->data(model->index(0), FriendsChatModel::IdRole).toString();
		m_alex.chat->report(id, QStringLiteral("spam"));
		QTRY_COMPARE(m_alex.finished, QStringList({"report:ok"}));
	}

	void cleanupTestCase()
	{
		for (Player* player : {&m_alex, &m_jeb}) {
			if (player->socket) {
				player->socket->stop();
			}
		}
	}
};

QTEST_GUILESS_MAIN(FriendsChatE2ETest)

#include "FriendsChatE2E_test.moc"
