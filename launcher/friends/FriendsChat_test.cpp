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

#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTest>

#include "FriendsChat.h"
#include "FriendsChatApi.h"

namespace
{
	const QString kMe = QStringLiteral("069a79f4-44e9-4726-a5be-fca90e38aaf5");
	const QString kJeb = QStringLiteral("853c80ef-3c37-49fd-aa49-938b674adae6");
	const QString kTime = QStringLiteral("2026-11-04T10:00:00.000Z");

	QJsonObject message(const QString& id, const QString& from, const QString& text,
						const QString& at = kTime)
	{
		return {{"id", id}, {"from", from}, {"to", from == kMe ? kJeb : kMe},
				{"text", text}, {"createdAt", at}, {"deleted", false}};
	}

	QByteArray page(const QJsonArray& messages, bool more = false)
	{
		return QJsonDocument(QJsonObject{{"messages", messages}, {"more", more}})
			.toJson(QJsonDocument::Compact);
	}

	FriendsApi::Reply reply(int status, const QByteArray& body = QByteArray(), int retryAfter = 0)
	{
		FriendsApi::Reply answer;
		answer.status = status;
		answer.body = body;
		answer.retryAfterSeconds = retryAfter;
		return answer;
	}

	/* Stands in for the controller: records what the chat sends, answers when told. */
	struct FakeServer {
		struct Request {
			QByteArray verb;
			QString path;
			QByteArray body;
			std::function<void(const FriendsApi::Reply&)> done;
		};
		bool socketUp = true;
		QList<QJsonObject> frames;
		QList<Request> requests;
		QStringList finished;

		FriendsChat::Transport transport()
		{
			FriendsChat::Transport t;
			t.sendFrame = [this](const QJsonObject& frame) {
				if (socketUp) {
					frames.append(frame);
				}
				return socketUp;
			};
			t.request = [this](const QByteArray& verb, const QString& path, const QByteArray& body,
							   std::function<void(const FriendsApi::Reply&)> done) {
				requests.append({verb, path, body, std::move(done)});
			};
			t.finished = [this](const QString& action, bool ok, const QString&) {
				finished.append(action + (ok ? QStringLiteral(":ok") : QStringLiteral(":failed")));
			};
			return t;
		}
		/* Answers the oldest request still waiting. */
		void answer(const FriendsApi::Reply& reply)
		{
			auto request = requests.takeFirst();
			request.done(reply);
		}
	};

	QVariant at(FriendsChat& chat, int row, int role)
	{
		auto* model = chat.messages();
		return model->data(model->index(row), role);
	}

	/* "id:state" for every row, newest first. */
	QString rows(FriendsChat& chat)
	{
		QStringList out;
		for (int row = 0; row < chat.messages()->rowCount(); ++row) {
			out << at(chat, row, FriendsChatModel::KeyRole).toString() + QLatin1Char(':') +
					   at(chat, row, FriendsChatModel::StateRole).toString();
		}
		return out.join(QLatin1Char(','));
	}
} // namespace

class FriendsChatTest : public QObject
{
	Q_OBJECT

	FakeServer m_server;
	std::unique_ptr<FriendsChat> m_chat;

	/* The conversation with jeb_ open, its newest page answered with @p messages. */
	void openWith(const QJsonArray& messages, bool more = false)
	{
		m_chat->open(kJeb);
		QCOMPARE(m_server.requests.size(), 1);
		QCOMPARE(m_server.requests.first().path,
				 QStringLiteral("/v1/chats/%1/messages?limit=50").arg(kJeb));
		m_server.answer(reply(200, page(messages, more)));
	}

  private slots:
	void init()
	{
		m_server = FakeServer();
		m_chat = std::make_unique<FriendsChat>();
		m_chat->setTransport(m_server.transport());
		m_chat->setMe(kMe);
	}

	void textIsCleanedLikeTheServerDoes()
	{
		using namespace FriendsChatApi;
		QCOMPARE(cleanText(QStringLiteral("  gg\n wp‮ §6gold  ")), QStringLiteral("gg wp §6gold"));
		QVERIFY(!sendable(QStringLiteral(" \n ")));
		QVERIFY(sendable(QString(kMaxTextLength, QLatin1Char('a'))));
		QVERIFY(!sendable(QString(kMaxTextLength + 1, QLatin1Char('a'))));
		QVERIFY(idLess(QStringLiteral("9"), QStringLiteral("10")));
		QVERIFY(idLess(QString(), QStringLiteral("1")));
		QVERIFY(!idLess(QStringLiteral("10"), QStringLiteral("10")));
		QVERIFY(isRef(QStringLiteral("s1")));
		QVERIFY(!isRef(QStringLiteral("bad ref")));
		QCOMPARE(Path::page(kJeb, QStringLiteral("42"), QString(), kPageLimit),
				 QStringLiteral("/v1/chats/%1/messages?before=42&limit=50").arg(kJeb));
	}

	void aDeletedMessageHasNoText()
	{
		const auto parsed = FriendsChatApi::parsePage(page(
			{QJsonObject{{"id", "3"}, {"from", kJeb.toUpper()}, {"to", kMe}, {"text", QJsonValue()},
						 {"createdAt", kTime}, {"deleted", true}}}));
		QVERIFY(parsed);
		QCOMPARE(parsed->messages.size(), 1);
		QVERIFY(parsed->messages.first().deleted);
		QVERIFY(parsed->messages.first().text.isEmpty());
		QCOMPARE(parsed->messages.first().from, kJeb);
		QVERIFY(!FriendsChatApi::parseMessage(QJsonObject{{"from", kJeb}}));
	}

	void openingReadsTheNewestPageNewestFirst()
	{
		openWith({message("1", kJeb, "hi"), message("2", kMe, "hey")}, true);
		QVERIFY(m_chat->loaded());
		QVERIFY(m_chat->hasOlder());
		QCOMPARE(rows(*m_chat), QStringLiteral("2:sent,1:sent"));
		QVERIFY(at(*m_chat, 0, FriendsChatModel::MineRole).toBool());

		m_chat->loadOlder();
		QCOMPARE(m_server.requests.first().path,
				 QStringLiteral("/v1/chats/%1/messages?before=1&limit=50").arg(kJeb));
		m_server.answer(reply(200, page({message("0", kJeb, "first")})));
		QCOMPARE(rows(*m_chat), QStringLiteral("2:sent,1:sent,0:sent"));
		QVERIFY(!m_chat->hasOlder());
	}

	void aSendShowsAtOnceAndTheServersCopyReplacesIt()
	{
		openWith({message("1", kJeb, "hi")});
		QVERIFY(m_chat->send(QStringLiteral(" gg\n")));
		QCOMPARE(rows(*m_chat), QStringLiteral("local-1:sending,1:sent"));
		QCOMPARE(at(*m_chat, 0, FriendsChatModel::TextRole).toString(), QStringLiteral("gg"));
		QCOMPARE(m_server.frames.size(), 1);
		const QJsonObject frame = m_server.frames.first();
		QCOMPARE(frame.value("type").toString(), QStringLiteral("chat.send"));
		QCOMPARE(frame.value("to").toString(), kJeb);
		QCOMPARE(frame.value("text").toString(), QStringLiteral("gg"));

		m_chat->handleFrame({{"type", "chat.sent"}, {"ref", frame.value("ref")},
							 {"message", message("5", kMe, "gg")}});
		QCOMPARE(rows(*m_chat), QStringLiteral("5:sent,1:sent"));
		QVERIFY(!m_chat->send(QStringLiteral("   ")));
	}

	void aRefusedSendFailsAndCanBeSentAgain()
	{
		openWith({});
		m_chat->send(QStringLiteral("hello"));
		const QJsonValue ref = m_server.frames.first().value("ref");
		m_chat->handleFrame({{"type", "error"}, {"ref", ref}, {"error", "not_found"},
							 {"message", "Not friends with this player"}});
		QCOMPARE(rows(*m_chat), QStringLiteral("local-1:failed"));
		QVERIFY(!at(*m_chat, 0, FriendsChatModel::ErrorRole).toString().isEmpty());

		m_chat->retry(QStringLiteral("local-1"));
		QCOMPARE(rows(*m_chat), QStringLiteral("local-2:sending"));
		QCOMPARE(m_server.frames.size(), 2);
		QVERIFY(m_server.frames.last().value("ref") != ref);

		m_chat->handleFrame({{"type", "error"}, {"ref", m_server.frames.last().value("ref")},
							 {"error", "bad_request"}, {"message", "too long"}});
		m_chat->discard(QStringLiteral("local-2"));
		QCOMPARE(m_chat->messages()->rowCount(), 0);
	}

	void aRateLimitHoldsBackTheNextMessage()
	{
		openWith({});
		m_chat->send(QStringLiteral("spam"));
		m_chat->handleFrame({{"type", "error"}, {"ref", m_server.frames.first().value("ref")},
							 {"error", "rate_limited"}, {"message", "Too many"}, {"retryAfter", 20}});
		QCOMPARE(rows(*m_chat), QStringLiteral("local-1:failed"));
		QVERIFY(m_chat->rateLimitedUntilMs() > QDateTime::currentMSecsSinceEpoch());
		QVERIFY(!m_chat->send(QStringLiteral("more")));
		m_chat->retry(QStringLiteral("local-1"));
		QCOMPARE(m_server.frames.size(), 1);
	}

	void withoutTheSocketItSendsOverHttp()
	{
		openWith({});
		m_server.socketUp = false;
		m_chat->send(QStringLiteral("hi"));
		QCOMPARE(m_server.requests.size(), 1);
		QCOMPARE(m_server.requests.first().verb, QByteArray("POST"));
		QCOMPARE(m_server.requests.first().path, QStringLiteral("/v1/chats/%1/messages").arg(kJeb));
		QCOMPARE(m_server.requests.first().body, QByteArray(R"({"text":"hi"})"));
		m_server.answer(reply(201, QJsonDocument(message("8", kMe, "hi")).toJson()));
		QCOMPARE(rows(*m_chat), QStringLiteral("8:sent"));

		m_chat->send(QStringLiteral("again"));
		m_server.answer(reply(429, R"({"error":"rate_limited","message":"slow"})", 30));
		QCOMPARE(rows(*m_chat), QStringLiteral("local-2:failed,8:sent"));
		QVERIFY(m_chat->rateLimitedUntilMs() > 0);
	}

	void liveMessagesLandInOrderOnce()
	{
		openWith({message("5", kJeb, "a")});
		QSignalSpy received(m_chat.get(), &FriendsChat::messageReceived);
		m_chat->handleFrame({{"type", "chat.message"}, {"message", message("7", kJeb, "c")}});
		m_chat->handleFrame({{"type", "chat.message"}, {"message", message("6", kJeb, "b")}});
		m_chat->handleFrame({{"type", "chat.message"}, {"message", message("7", kJeb, "c")}});
		QCOMPARE(rows(*m_chat), QStringLiteral("7:sent,6:sent,5:sent"));
		QCOMPARE(received.size(), 2);
	}

	void ourMessageFromElsewhereTakesThePendingOnesPlace()
	{
		openWith({});
		m_server.socketUp = false;
		m_chat->send(QStringLiteral("gg"));
		/* The HTTP answer is lost, but the message reached the server. */
		m_chat->handleFrame({{"type", "chat.message"}, {"message", message("9", kMe, "gg")}});
		QCOMPARE(rows(*m_chat), QStringLiteral("9:sent"));
		m_server.answer(reply(201, QJsonDocument(message("9", kMe, "gg")).toJson()));
		QCOMPARE(rows(*m_chat), QStringLiteral("9:sent"));
	}

	void theConversationOnScreenIsMarkedRead()
	{
		openWith({message("3", kJeb, "hi"), message("4", kMe, "yo")});
		QSignalSpy read(m_chat.get(), &FriendsChat::markedRead);
		QTest::qWait(FriendsChat::kReadDelayMs * 2);
		QVERIFY(m_server.frames.isEmpty());

		m_chat->setViewing(true);
		QTRY_COMPARE(m_server.frames.size(), 1);
		QCOMPARE(m_server.frames.first().value("type").toString(), QStringLiteral("chat.read"));
		QCOMPARE(m_server.frames.first().value("upTo").toString(), QStringLiteral("4"));
		QCOMPARE(read.size(), 1);

		m_chat->handleFrame({{"type", "chat.message"}, {"message", message("5", kMe, "me")}});
		QTest::qWait(FriendsChat::kReadDelayMs * 2);
		QCOMPARE(m_server.frames.size(), 1);
		m_chat->handleFrame({{"type", "chat.message"}, {"message", message("6", kJeb, "you")}});
		QTRY_COMPARE(m_server.frames.size(), 2);
		QCOMPARE(m_server.frames.last().value("upTo").toString(), QStringLiteral("6"));
		QVERIFY(m_chat->isViewing(kJeb.toUpper()));
	}

	void aDeletedAccountLeavesPlaceholders()
	{
		openWith({message("1", kJeb, "hi"), message("2", kMe, "hey")});
		m_chat->handleFrame({{"type", "chat.deleted"}, {"uuid", kJeb}});
		QVERIFY(at(*m_chat, 1, FriendsChatModel::DeletedRole).toBool());
		QVERIFY(at(*m_chat, 1, FriendsChatModel::TextRole).toString().isEmpty());
		QVERIFY(!at(*m_chat, 0, FriendsChatModel::DeletedRole).toBool());
	}

	void aReconnectCatchesUpTheOpenConversation()
	{
		m_chat->setLive(true);
		openWith({message("5", kJeb, "a")});
		m_chat->setLive(false);
		m_chat->setLive(true);
		QCOMPARE(m_server.requests.size(), 1);
		QCOMPARE(m_server.requests.first().path,
				 QStringLiteral("/v1/chats/%1/messages?after=5&limit=100").arg(kJeb));
		m_server.answer(reply(200, page({message("6", kJeb, "b"), message("7", kMe, "c")})));
		QCOMPARE(rows(*m_chat), QStringLiteral("7:sent,6:sent,5:sent"));
	}

	void aReportNamesTheReason()
	{
		openWith({message("5", kJeb, "spam")});
		m_chat->report(QStringLiteral("5"), QStringLiteral("spam"));
		QCOMPARE(m_server.requests.first().path,
				 QStringLiteral("/v1/chats/%1/messages/5/report").arg(kJeb));
		QCOMPARE(m_server.requests.first().body, QByteArray(R"({"reason":"spam"})"));
		m_server.answer(reply(204));
		m_chat->report(QStringLiteral("5"), QStringLiteral("rude"));
		QTRY_COMPARE(m_server.finished, QStringList({"report:ok", "report:failed"}));
	}

	void aBlockForgetsTheHistory()
	{
		openWith({message("1", kJeb, "hi")});
		m_chat->forget(kJeb);
		QVERIFY(m_chat->openUuid().isEmpty());
		QCOMPARE(m_chat->messages()->rowCount(), 0);
	}

	void runsBreakOnTheSideTheGapAndTheDay()
	{
		openWith({message("1", kJeb, "a", "2026-11-03T10:00:00.000Z"),
				  message("2", kJeb, "b", "2026-11-04T10:00:00.000Z"),
				  message("3", kJeb, "c", "2026-11-04T10:01:00.000Z"),
				  message("4", kMe, "d", "2026-11-04T10:02:00.000Z"),
				  message("5", kMe, "e", "2026-11-04T10:30:00.000Z")});
		QStringList marks;
		for (int row = m_chat->messages()->rowCount() - 1; row >= 0; --row) {
			marks << QStringLiteral("%1%2")
						 .arg(at(*m_chat, row, FriendsChatModel::DayStartRole).toBool() ? "D" : "-",
							  at(*m_chat, row, FriendsChatModel::GroupStartRole).toBool() ? "G" : "-");
		}
		QCOMPARE(marks.join(QLatin1Char(' ')), QStringLiteral("DG DG -- -G -G"));
	}
};

QTEST_GUILESS_MAIN(FriendsChatTest)

#include "FriendsChat_test.moc"
