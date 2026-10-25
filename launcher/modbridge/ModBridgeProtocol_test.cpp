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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVariantMap>

#include "ModBridgeJson.h"
#include "ModBridgeProtocol.h"

using namespace ModBridgeProtocol;
namespace Json = ModBridgeJson;
using FriendsApi::World;

namespace
{
	constexpr quint16 kPort = 40123;
	const QString kAli = QStringLiteral("11111111-2222-3333-4444-555555555555");
	const QString kBob = QStringLiteral("66666666-7777-8888-9999-000000000000");
	const QString kCy = QStringLiteral("aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee");
	const QString kNobody = QStringLiteral("99999999-9999-9999-9999-999999999999");

	QByteArray head(const QByteArray& extra = QByteArray(),
					const QByteArray& line = "GET /v1/hello HTTP/1.1")
	{
		return line + "\r\nHost: 127.0.0.1:" + QByteArray::number(kPort) +
			   "\r\n" + extra + "\r\n";
	}

	Request parsedOk(const QByteArray& raw)
	{
		const Parsed parsed = parseRequest(raw);
		return parsed.state == Parsed::State::Complete ? parsed.request
													   : Request();
	}

	QVariantMap friendMap(const QString& uuid, const QString& name,
						  const QString& state, const QString& worldType,
						  const QString& host, bool joinable)
	{
		QVariantMap map;
		map["uuid"] = uuid;
		map["name"] = name;
		map["state"] = state;
		map["statusText"] = host.isEmpty() ? QStringLiteral("Offline") : "On " + host;
		map["version"] = QStringLiteral("1.8.9");
		map["worldType"] = worldType;
		map["host"] = host;
		map["port"] = host.isEmpty() ? 0 : 25565;
		map["joinable"] = joinable;
		map["lastSeenAt"] = QStringLiteral("2026-10-25T15:00:00.000Z");
		/* What the bridge must not pass on. */
		map["face"] = QStringLiteral("file:///secret/face.png");
		map["since"] = QStringLiteral("2026-01-01T00:00:00.000Z");
		map["joinOff"] = false;
		map["instance"] = QStringLiteral("My private instance");
		map["token"] = QStringLiteral("hunter2");
		return map;
	}

	Json::FriendsView view()
	{
		Json::FriendsView v;
		v.rev = 17;
		v.enabled = true;
		v.state = QStringLiteral("ready");
		v.meName = QStringLiteral("grxtor");
		v.meUuid = kAli;
		v.incomingCount = 1;
		v.updatedAtMs = 1761400000000;
		v.friends = {
			friendMap(kBob, "Ali", "playing", "server", "mc.hypixel.net", true),
			friendMap(kCy, "Cy", "offline", "", "", false)};
		return v;
	}
} // namespace

class ModBridgeProtocolTest : public QObject
{
	Q_OBJECT

  private slots:
	// ---- Reading a request
	void aSmallRequestIsRead()
	{
		const auto parsed = parseRequest(head("Authorization: Bearer abc\r\n"));
		QCOMPARE(parsed.state, Parsed::State::Complete);
		QCOMPARE(parsed.request.method, QByteArray("GET"));
		QCOMPARE(parsed.request.path, QByteArray("/v1/hello"));
		QCOMPARE(parsed.request.header("authorization"),
				 QByteArray("Bearer abc"));
		QCOMPARE(parsed.request.header("host"),
				 "127.0.0.1:" + QByteArray::number(kPort));
	}

	void theQueryIsNotPartOfThePath()
	{
		const auto request =
			parsedOk(head({}, "GET /v1/friends?x=1 HTTP/1.1"));
		QCOMPARE(request.path, QByteArray("/v1/friends"));
	}

	void anIncompleteHeadWaits()
	{
		QCOMPARE(parseRequest("GET /v1/hello HTTP/1.1\r\nHo").state,
				 Parsed::State::NeedMore);
		QCOMPARE(parseRequest(QByteArray()).state, Parsed::State::NeedMore);
	}

	void aBodyIsReadByItsLength()
	{
		const QByteArray body = "{\"type\":\"none\"}";
		const QByteArray raw =
			head("Content-Length: " + QByteArray::number(body.size()) + "\r\n",
				 "POST /v1/world HTTP/1.1");
		QCOMPARE(parseRequest(raw + body.left(3)).state,
				 Parsed::State::NeedMore);
		const auto parsed = parseRequest(raw + body);
		QCOMPARE(parsed.state, Parsed::State::Complete);
		QCOMPARE(parsed.request.body, body);
	}

	void aHeadOverEightKilobytesIsTooLarge()
	{
		const QByteArray padding(kMaxHeadBytes, 'a');
		auto parsed = parseRequest(head("X-Pad: " + padding + "\r\n"));
		QCOMPARE(parsed.state, Parsed::State::Rejected);
		QCOMPARE(parsed.status, Http::kPayloadTooLarge);
		/* Even before the head has ended. */
		parsed = parseRequest("GET /" + padding + padding);
		QCOMPARE(parsed.status, Http::kPayloadTooLarge);
	}

	void aBodyOverFourKilobytesIsTooLarge()
	{
		const auto parsed = parseRequest(
			head("Content-Length: " + QByteArray::number(kMaxBodyBytes + 1) +
					 "\r\n",
				 "POST /v1/world HTTP/1.1"));
		QCOMPARE(parsed.state, Parsed::State::Rejected);
		QCOMPARE(parsed.status, Http::kPayloadTooLarge);
		/* Exactly the limit is fine. */
		QCOMPARE(parseRequest(head("Content-Length: " +
									   QByteArray::number(kMaxBodyBytes) +
									   "\r\n",
								   "POST /v1/world HTTP/1.1"))
					 .state,
				 Parsed::State::NeedMore);
	}

	void malformedRequestsAreRefused_data()
	{
		QTest::addColumn<QByteArray>("raw");
		QTest::newRow("transfer-encoding")
			<< head("Transfer-Encoding: chunked\r\n");
		QTest::newRow("transfer-encoding any case")
			<< head("tRaNsFeR-eNcOdInG: identity\r\n");
		QTest::newRow("duplicate host")
			<< head("Host: 127.0.0.1:1\r\n");
		QTest::newRow("duplicate content-length")
			<< head("Content-Length: 0\r\nContent-Length: 0\r\n");
		QTest::newRow("folded header")
			<< head("X-A: 1\r\n continued\r\n");
		QTest::newRow("space before the colon") << head("X-A : 1\r\n");
		QTest::newRow("no colon") << head("garbage\r\n");
		QTest::newRow("content-length not a number")
			<< head("Content-Length: 12a\r\n");
		QTest::newRow("negative content-length")
			<< head("Content-Length: -1\r\n");
		QTest::newRow("not http") << head({}, "GET /v1/hello SPDY/3");
		QTest::newRow("no target") << head({}, "GET HTTP/1.1");
		QTest::newRow("absolute target")
			<< head({}, "GET http://example.com/ HTTP/1.1");
		QTest::newRow("bare newline")
			<< QByteArray("GET /v1/hello HTTP/1.1\nHost: x\r\n\r\n");
	}

	void malformedRequestsAreRefused()
	{
		QFETCH(QByteArray, raw);
		const auto parsed = parseRequest(raw);
		QCOMPARE(parsed.state, Parsed::State::Rejected);
		QCOMPARE(parsed.status, Http::kBadRequest);
	}

	// ---- The rules of the document
	void theRightHostPasses()
	{
		QCOMPARE(checkRequest(parsedOk(head()), kPort), 0);
	}

	void anotherHostIsMisdirected_data()
	{
		QTest::addColumn<QByteArray>("host");
		QTest::newRow("localhost") << QByteArray("localhost:40123");
		QTest::newRow("other port") << QByteArray("127.0.0.1:40124");
		QTest::newRow("no port") << QByteArray("127.0.0.1");
		QTest::newRow("a name") << QByteArray("evil.example:40123");
		QTest::newRow("rebinding") << QByteArray("127.0.0.1:40123.evil.example");
		QTest::newRow("empty") << QByteArray();
	}

	void anotherHostIsMisdirected()
	{
		QFETCH(QByteArray, host);
		const auto request = parsedOk("GET /v1/hello HTTP/1.1\r\nHost: " +
									  host + "\r\n\r\n");
		QCOMPARE(checkRequest(request, kPort), Http::kMisdirected);
	}

	void aMissingHostIsMisdirected()
	{
		QCOMPARE(checkRequest(parsedOk("GET /v1/hello HTTP/1.1\r\n\r\n"), kPort),
				 Http::kMisdirected);
	}

	void aBrowserIsForbidden_data()
	{
		QTest::addColumn<QByteArray>("header");
		QTest::newRow("origin") << QByteArray("Origin: http://evil.example\r\n");
		QTest::newRow("null origin") << QByteArray("Origin: null\r\n");
		QTest::newRow("sec-fetch-site") << QByteArray("Sec-Fetch-Site: cross-site\r\n");
		QTest::newRow("sec-fetch-mode") << QByteArray("sec-fetch-mode: cors\r\n");
		QTest::newRow("sec-fetch-anything") << QByteArray("Sec-Fetch-Foo: 1\r\n");
	}

	void aBrowserIsForbidden()
	{
		QFETCH(QByteArray, header);
		QCOMPARE(checkRequest(parsedOk(head(header)), kPort), Http::kForbidden);
	}

	void onlyGetAndPostAreAllowed_data()
	{
		QTest::addColumn<QByteArray>("method");
		QTest::addColumn<int>("status");
		QTest::newRow("GET") << QByteArray("GET") << 0;
		QTest::newRow("POST") << QByteArray("POST") << 0;
		QTest::newRow("OPTIONS") << QByteArray("OPTIONS") << Http::kMethodNotAllowed;
		QTest::newRow("PUT") << QByteArray("PUT") << Http::kMethodNotAllowed;
		QTest::newRow("DELETE") << QByteArray("DELETE") << Http::kMethodNotAllowed;
		QTest::newRow("HEAD") << QByteArray("HEAD") << Http::kMethodNotAllowed;
		QTest::newRow("lower case") << QByteArray("get") << Http::kMethodNotAllowed;
	}

	void onlyGetAndPostAreAllowed()
	{
		QFETCH(QByteArray, method);
		QFETCH(int, status);
		const auto request = parsedOk(head({}, method + " /v1/hello HTTP/1.1"));
		QCOMPARE(checkRequest(request, kPort), status);
	}

	void theHostIsCheckedBeforeTheBrowserHeaders()
	{
		const auto request = parsedOk(
			"GET / HTTP/1.1\r\nHost: evil.example\r\nOrigin: x\r\n\r\n");
		QCOMPARE(checkRequest(request, kPort), Http::kMisdirected);
	}

	void aPostNeedsJson()
	{
		const auto post = [](const QByteArray& type) {
			return parsedOk(head(type, "POST /v1/world HTTP/1.1"));
		};
		QCOMPARE(checkContentType(post("Content-Type: application/json\r\n")), 0);
		QCOMPARE(checkContentType(
					 post("Content-Type: Application/JSON; charset=utf-8\r\n")),
				 0);
		QCOMPARE(checkContentType(post("Content-Type: text/plain\r\n")),
				 Http::kUnsupportedMediaType);
		QCOMPARE(checkContentType(post("Content-Type: application/jsonx\r\n")),
				 Http::kUnsupportedMediaType);
		QCOMPARE(checkContentType(post(QByteArray())),
				 Http::kUnsupportedMediaType);
		/* A GET has nothing to declare. */
		QCOMPARE(checkContentType(parsedOk(head())), 0);
	}

	// ---- The token
	void theBearerTokenIsRead()
	{
		const auto token = [](const QByteArray& value) {
			return bearerToken(parsedOk(head("Authorization: " + value + "\r\n")));
		};
		QCOMPARE(token("Bearer abc").value_or(QByteArray()), QByteArray("abc"));
		QCOMPARE(token("bearer   abc  ").value_or(QByteArray()),
				 QByteArray("abc"));
		QVERIFY(!token("Basic abc"));
		QVERIFY(!token("Bearer"));
		QVERIFY(!token("Bearer "));
		QVERIFY(!token("abc"));
		QVERIFY(!bearerToken(parsedOk(head())));
	}

	void tokensAreComparedWhole()
	{
		QVERIFY(tokenMatches("abcdef", "abcdef"));
		QVERIFY(!tokenMatches("abcdef", "abcdeg"));
		QVERIFY(!tokenMatches("abcdef", "abcde"));
		QVERIFY(!tokenMatches("abcdef", "abcdefg"));
		QVERIFY(!tokenMatches("abcdef", ""));
		QVERIFY(tokenMatches("", ""));
		/* A shorter one never matches a longer one that starts with it,
		 * and zero bytes are not padding. */
		QVERIFY(!tokenMatches(QByteArray("abc\0", 4), "abc"));
	}

	void aNewTokenIsBase64UrlOf32Bytes()
	{
		const QString a = newToken();
		const QString b = newToken();
		QCOMPARE(a.size(), 43);
		QVERIFY(QRegularExpression("^[A-Za-z0-9_-]{43}$").match(a).hasMatch());
		QCOMPARE(QByteArray::fromBase64(a.toLatin1(),
										QByteArray::Base64UrlEncoding)
					 .size(),
				 kTokenBytes);
		QVERIFY(a != b);
	}

	void tooManyWrongTokensLockEverything()
	{
		Lockout lockout;
		qint64 now = 1000;
		for (int i = 0; i < kMaxBadTokens - 1; ++i) {
			QVERIFY(!lockout.locked(now));
			lockout.recordFailure(now);
		}
		QVERIFY(!lockout.locked(now));
		lockout.recordFailure(now);
		QVERIFY(lockout.locked(now));
		QVERIFY(lockout.locked(now + kLockoutMs - 1));
		QVERIFY(!lockout.locked(now + kLockoutMs));
		/* After it, the count starts over. */
		now += kLockoutMs;
		for (int i = 0; i < kMaxBadTokens - 1; ++i) {
			lockout.recordFailure(now);
		}
		QVERIFY(!lockout.locked(now));
	}

	// ---- The answer's bytes
	void answersCarryNoCorsAndAreNotCached()
	{
		const QByteArray out = response(Http::kOk, "{}");
		QVERIFY(out.startsWith("HTTP/1.1 200 OK\r\n"));
		QVERIFY(out.contains("Cache-Control: no-store\r\n"));
		QVERIFY(out.contains("Connection: close\r\n"));
		QVERIFY(out.contains("Content-Length: 2\r\n"));
		QVERIFY(!out.toLower().contains("access-control"));
		QVERIFY(out.endsWith("\r\n\r\n{}"));
	}

	void a204HasNoBody()
	{
		const QByteArray out = response(Http::kNoContent);
		QVERIFY(out.startsWith("HTTP/1.1 204 No Content\r\n"));
		QVERIFY(!out.contains("Content-Length"));
		QVERIFY(out.endsWith("\r\n\r\n"));
	}

	void anErrorIsJson()
	{
		const QByteArray out = errorResponse(Http::kUnauthorized, "unauthorized");
		QVERIFY(out.startsWith("HTTP/1.1 401 Unauthorized\r\n"));
		QVERIFY(out.endsWith("{\"error\":\"unauthorized\"}"));
	}

	void aFacePathNeedsAUuid()
	{
		QCOMPARE(faceUuid("/v1/faces/" + kAli.toLatin1() + ".png")
					 .value_or(QString()),
				 kAli);
		QVERIFY(!faceUuid("/v1/faces/../../etc/passwd.png"));
		QVERIFY(!faceUuid("/v1/faces/" + kAli.toLatin1()));
		QVERIFY(!faceUuid("/v1/faces/.png"));
		QVERIFY(!faceUuid("/v1/faces/" + kAli.toLatin1() + ".png/x"));
		QVERIFY(!faceUuid("/v1/friends"));
		QVERIFY(!faceUuid("/v1/faces/%2e%2e.png"));
	}

	// ---- The messages
	void theStatesAreMapped()
	{
		QCOMPARE(Json::friendsState("off", false), QString("off"));
		QCOMPARE(Json::friendsState("noAccount", false), QString("signedOut"));
		QCOMPARE(Json::friendsState("signingIn", false), QString("connecting"));
		QCOMPARE(Json::friendsState("ready", false), QString("ready"));
		QCOMPARE(Json::friendsState("error", false), QString("error"));
		QCOMPARE(Json::friendsState("error", true), QString("signedOut"));
		QCOMPARE(Json::friendsState("something else", false), QString("off"));
	}

	void helloSaysWhoAndWhatFriendsDoes()
	{
		const auto body = QJsonDocument::fromJson(Json::helloBody(
			"Kite 11.0.0", {"abc", "Minecraft 26.3", "26.3"}, true, "ready"));
		const auto object = body.object();
		QCOMPARE(object.value("bridge").toInt(), 1);
		QCOMPARE(object.value("launcher").toString(), QString("Kite 11.0.0"));
		QCOMPARE(object.value("instance").toObject().value("id").toString(),
				 QString("abc"));
		QCOMPARE(object.value("instance").toObject().value("name").toString(),
				 QString("Minecraft 26.3"));
		QCOMPARE(object.value("instance").toObject().value("version").toString(),
				 QString("26.3"));
		QCOMPARE(object.value("friends").toObject().value("enabled").toBool(),
				 true);
		QCOMPARE(object.value("friends").toObject().value("state").toString(),
				 QString("ready"));
		QCOMPARE(object.keys().size(), 4);
	}

	void friendsAreBuiltFromTheWhitelistOnly()
	{
		const QByteArray raw = Json::friendsBody(view());
		QVERIFY(!raw.contains("secret"));
		QVERIFY(!raw.contains("hunter2"));
		QVERIFY(!raw.contains("private"));
		const auto object = QJsonDocument::fromJson(raw).object();
		QCOMPARE(object.keys(),
				 QStringList({"enabled", "friends", "incomingCount", "me", "rev",
							  "state", "updatedAtMs"}));
		QCOMPARE(object.value("rev").toInt(), 17);
		QCOMPARE(object.value("me").toObject().value("name").toString(),
				 QString("grxtor"));
		QCOMPARE(object.value("me").toObject().keys(),
				 QStringList({"name", "uuid"}));
		QCOMPARE(object.value("incomingCount").toInt(), 1);
		QCOMPARE(object.value("updatedAtMs").toVariant().toLongLong(),
				 qint64(1761400000000));
		const auto friends = object.value("friends").toArray();
		QCOMPARE(friends.size(), 2);
		const auto ali = friends[0].toObject();
		QCOMPARE(ali.keys(),
				 QStringList({"joinable", "lastSeenAt", "name", "state",
							  "statusText", "uuid", "version", "world"}));
		QCOMPARE(ali.value("name").toString(), QString("Ali"));
		QCOMPARE(ali.value("state").toString(), QString("playing"));
		QCOMPARE(ali.value("joinable").toBool(), true);
		QCOMPARE(ali.value("world").toObject().value("type").toString(),
				 QString("server"));
		QCOMPARE(ali.value("world").toObject().value("host").toString(),
				 QString("mc.hypixel.net"));
		QCOMPARE(ali.value("world").toObject().value("port").toInt(), 25565);
		const auto cy = friends[1].toObject();
		QCOMPARE(cy.value("state").toString(), QString("offline"));
		QCOMPARE(cy.value("world").toObject().value("type").toString(),
				 QString("none"));
		QVERIFY(!cy.value("world").toObject().contains("host"));
	}

	void aHiddenServerHasNoHost()
	{
		Json::FriendsView v = view();
		QVariantMap hidden = friendMap(kBob, "Ali", "playing", "server", "", false);
		v.friends = {hidden};
		const auto world = QJsonDocument::fromJson(Json::friendsBody(v))
							   .object()
							   .value("friends")
							   .toArray()[0]
							   .toObject()
							   .value("world")
							   .toObject();
		QCOMPARE(world.keys(), QStringList({"type"}));
		QCOMPARE(world.value("type").toString(), QString("server"));
	}

	void anUnknownStateIsOffline()
	{
		Json::FriendsView v = view();
		QVariantMap odd = friendMap(kBob, "Ali", "invisible", "", "", false);
		v.friends = {odd};
		const auto one = QJsonDocument::fromJson(Json::friendsBody(v))
							 .object()
							 .value("friends")
							 .toArray()[0]
							 .toObject();
		QCOMPARE(one.value("state").toString(), QString("offline"));
	}

	void theRevIsTheViewsAndTheDigestIgnoresIt()
	{
		Json::FriendsView a = view();
		Json::FriendsView b = view();
		b.rev = 99;
		b.updatedAtMs = 5;
		const auto drop = [](Json::FriendsView v) {
			v.rev = 0;
			v.updatedAtMs = 0;
			return Json::friendsBody(v);
		};
		QCOMPARE(drop(a), drop(b));
		b.friends.removeLast();
		QVERIFY(drop(a) != drop(b));
	}

	// ---- Joining
	void aPlayingFriendCanBeJoined()
	{
		const auto answer = Json::checkJoin(view(), kBob, "1.8.9");
		QVERIFY(answer.ok);
		QCOMPARE(answer.host, QString("mc.hypixel.net"));
		QCOMPARE(answer.port, 25565);
		QCOMPARE(answer.version, QString("1.8.9"));
		QVERIFY(answer.versionMatch);
		const auto body = QJsonDocument::fromJson(Json::joinBody(answer)).object();
		QCOMPARE(body.value("ok").toBool(), true);
		QCOMPARE(body.value("host").toString(), QString("mc.hypixel.net"));
		QCOMPARE(body.value("port").toInt(), 25565);
		QCOMPARE(body.value("version").toString(), QString("1.8.9"));
		QCOMPARE(body.value("versionMatch").toBool(), true);
	}

	void anotherVersionIsSaidNotRefused()
	{
		const auto answer = Json::checkJoin(view(), kBob, "26.3");
		QVERIFY(answer.ok);
		QVERIFY(!answer.versionMatch);
		QVERIFY(!Json::checkJoin(view(), kBob, QString()).versionMatch);
	}

	void theUuidMayComeWithoutDashes()
	{
		const auto uuid = Json::parseJoinUuid(
			"{\"uuid\":\"66666666777788889999000000000000\"}");
		QCOMPARE(uuid.value_or(QString()), kBob);
		QVERIFY(Json::checkJoin(view(), uuid.value_or(QString()), "1.8.9").ok);
		QCOMPARE(Json::parseJoinUuid(("{\"uuid\":\"" + kBob.toUpper() + "\"}")
										 .toLatin1())
					 .value_or(QString()),
				 kBob);
	}

	void aBadJoinBodyIsNotOne_data()
	{
		QTest::addColumn<QByteArray>("body");
		QTest::newRow("empty") << QByteArray();
		QTest::newRow("not json") << QByteArray("uuid=1");
		QTest::newRow("array") << QByteArray("[]");
		QTest::newRow("no uuid") << QByteArray("{}");
		QTest::newRow("number") << QByteArray("{\"uuid\":5}");
		QTest::newRow("path") << QByteArray("{\"uuid\":\"../../x\"}");
		QTest::newRow("short") << QByteArray("{\"uuid\":\"1234\"}");
	}

	void aBadJoinBodyIsNotOne()
	{
		QFETCH(QByteArray, body);
		QVERIFY(!Json::parseJoinUuid(body));
	}

	void joinRefusalsHaveReasons()
	{
		QCOMPARE(Json::checkJoin(view(), kNobody, "1.8.9").reason,
				 QString("unknown_friend"));
		QCOMPARE(Json::checkJoin(view(), kCy, "1.8.9").reason, QString("offline"));
		Json::FriendsView off = view();
		off.enabled = false;
		off.state = "off";
		QCOMPARE(Json::checkJoin(off, kBob, "1.8.9").reason,
				 QString("friends_off"));
		Json::FriendsView notJoinable = view();
		notJoinable.friends = {friendMap(kBob, "Ali", "playing", "server",
										 "mc.hypixel.net", false)};
		QCOMPARE(Json::checkJoin(notJoinable, kBob, "1.8.9").reason,
				 QString("not_joinable"));
		Json::FriendsView single = view();
		single.friends = {friendMap(kBob, "Ali", "playing", "singleplayer", "", false)};
		QCOMPARE(Json::checkJoin(single, kBob, "1.8.9").reason,
				 QString("not_joinable"));
		Json::FriendsView online = view();
		online.friends = {friendMap(kBob, "Ali", "online", "", "", false)};
		QCOMPARE(Json::checkJoin(online, kBob, "1.8.9").reason,
				 QString("not_joinable"));
		const auto body = QJsonDocument::fromJson(
							  Json::joinBody(Json::checkJoin(view(), kCy, "")))
							  .object();
		QCOMPARE(body.value("ok").toBool(), false);
		QCOMPARE(body.value("reason").toString(), QString("offline"));
		QCOMPARE(body.keys(), QStringList({"ok", "reason"}));
	}

	// ---- Worlds the game reports
	void aServerIsNormalisedLikeTheLogParser()
	{
		const auto world = Json::parseWorld(
			"{\"type\":\"server\",\"host\":\"MC.Hypixel.NET.\",\"port\":25565}");
		QVERIFY(world);
		QCOMPARE(world->type, World::Type::Server);
		QCOMPARE(world->host, QString("mc.hypixel.net"));
		QCOMPARE(world->port, 25565);
	}

	void aPortInTheAddressIsSplitOff()
	{
		const auto world = Json::parseWorld(
			"{\"type\":\"server\",\"host\":\"play.example.com:25599\"}");
		QVERIFY(world);
		QCOMPARE(world->host, QString("play.example.com"));
		QCOMPARE(world->port, 25599);
		const auto v6 = Json::parseWorld(
			"{\"type\":\"server\",\"host\":\"[2001:db8::1]:25566\"}");
		QVERIFY(v6);
		QCOMPARE(v6->host, QString("2001:db8::1"));
		QCOMPARE(v6->port, 25566);
		const auto none = Json::parseWorld("{\"type\":\"server\",\"host\":\"x.org\"}");
		QCOMPARE(none->port, 25565);
	}

	void singleplayerAndNoneAreWorlds()
	{
		QCOMPARE(Json::parseWorld("{\"type\":\"singleplayer\"}")->type,
				 World::Type::Singleplayer);
		QCOMPARE(Json::parseWorld("{\"type\":\"none\"}")->type, World::Type::None);
	}

	void lanAndLoopbackWorldsAreNone_data()
	{
		QTest::addColumn<QString>("host");
		QTest::newRow("localhost") << "localhost";
		QTest::newRow("loopback") << "127.0.0.1";
		QTest::newRow("loopback v6") << "::1";
		QTest::newRow("any") << "0.0.0.0";
		QTest::newRow("10.x") << "10.0.0.5";
		QTest::newRow("172.16") << "172.16.4.1";
		QTest::newRow("192.168") << "192.168.1.20";
		QTest::newRow("link local") << "169.254.1.1";
		QTest::newRow("unique local v6") << "fd00::1";
		QTest::newRow("mdns") << "mypc.local";
		QTest::newRow("home") << "nas.lan";
		QTest::newRow("bare name") << "myserver";
		QTest::newRow("not a host") << "not a host!";
		QTest::newRow("numeric label") << "2130706433";
		QTest::newRow("empty") << "";
	}

	void lanAndLoopbackWorldsAreNone()
	{
		QFETCH(QString, host);
		QJsonObject object{{"type", "server"}, {"host", host}, {"port", 25565}};
		const auto world =
			Json::parseWorld(QJsonDocument(object).toJson(QJsonDocument::Compact));
		QVERIFY(world);
		QCOMPARE(world->type, World::Type::None);
		QVERIFY(world->host.isEmpty());
	}

	void aPublicAddressIsAServer()
	{
		const auto world = Json::parseWorld(
			"{\"type\":\"server\",\"host\":\"8.8.8.8\",\"port\":25565}");
		QCOMPARE(world->type, World::Type::Server);
		QCOMPARE(world->host, QString("8.8.8.8"));
	}

	void anOutOfRangePortIsUnknown()
	{
		const auto world = Json::parseWorld(
			"{\"type\":\"server\",\"host\":\"x.org\",\"port\":70000}");
		QCOMPARE(world->host, QString("x.org"));
		QCOMPARE(world->port, 0);
	}

	void aBadWorldBodyIsNotOne_data()
	{
		QTest::addColumn<QByteArray>("body");
		QTest::newRow("empty") << QByteArray();
		QTest::newRow("not json") << QByteArray("server");
		QTest::newRow("array") << QByteArray("[]");
		QTest::newRow("no type") << QByteArray("{}");
		QTest::newRow("unknown type") << QByteArray("{\"type\":\"realm\"}");
		QTest::newRow("server without host") << QByteArray("{\"type\":\"server\"}");
		QTest::newRow("host not a string")
			<< QByteArray("{\"type\":\"server\",\"host\":5}");
	}

	void aBadWorldBodyIsNotOne()
	{
		QFETCH(QByteArray, body);
		QVERIFY(!Json::parseWorld(body));
	}
};

QTEST_GUILESS_MAIN(ModBridgeProtocolTest)

#include "ModBridgeProtocol_test.moc"
