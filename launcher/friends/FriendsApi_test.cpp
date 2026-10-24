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

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

#include "FriendsApi.h"
#include "FriendsDisplay.h"
#include "FriendsSessionStore.h"

using namespace FriendsApi;

namespace
{
	/* The bodies of server/friends/README.md. */
	const QByteArray kChallenge = R"({
		"challengeId": "6f1c2c1e-6c55-4d0b-9d3b-6bd4f1a3e0a2",
		"serverId": "3f786850e387550fdab836ed7e6dc881de23001b",
		"expiresAt": "2026-10-24T18:02:00.000Z" })";
	const QByteArray kLogin = R"({
		"token": "kfs_Jx2v",
		"expiresAt": "2026-11-23T18:00:00.000Z",
		"user": { "uuid": "069a79f4-44e9-4726-a5be-fca90e38aaf5", "name": "Notch" } })";
	const QByteArray kMe = R"({
		"uuid": "069a79f4-44e9-4726-a5be-fca90e38aaf5", "name": "Notch",
		"settings": { "visibility": "everyone", "hideServer": true, "shareIpServers": false } })";
	const QByteArray kFriends = R"({
		"friends": [
			{ "uuid": "853c80ef-3c37-49fd-aa49-938b674adae6", "name": "jeb_",
			  "since": "2026-10-20T12:00:00.000Z",
			  "status": { "state": "playing", "instance": "Hypixel", "version": "1.8.9",
						  "world": { "type": "server", "host": "mc.hypixel.net", "port": 25565 } } },
			{ "uuid": "11111111-1111-4111-8111-111111111111", "name": "Solo",
			  "since": "2026-10-21T12:00:00.000Z",
			  "status": { "state": "playing", "instance": "Survival", "version": "1.21.4",
						  "world": { "type": "singleplayer" } } },
			{ "uuid": "22222222-2222-4222-8222-222222222222", "name": "Menus",
			  "since": "2026-10-21T12:00:00.000Z",
			  "status": { "state": "playing", "instance": "Survival", "version": "1.21.4",
						  "world": null } },
			{ "uuid": "33333333-3333-4333-8333-333333333333", "name": "Home",
			  "since": "2026-10-21T12:00:00.000Z",
			  "status": { "state": "playing", "instance": "Survival", "version": "1.21.4",
						  "world": { "type": "server" } } },
			{ "uuid": "44444444-4444-4444-8444-444444444444", "name": "Idle",
			  "since": "2026-10-21T12:00:00.000Z", "status": { "state": "online" } },
			{ "uuid": "55555555-5555-4555-8555-555555555555", "name": "Away",
			  "since": "2026-10-21T12:00:00.000Z",
			  "status": { "state": "offline", "lastSeenAt": "2026-10-24T16:00:00.000Z" } },
			{ "uuid": "66666666-6666-4666-8666-666666666666", "name": "Never",
			  "since": "2026-10-21T12:00:00.000Z",
			  "status": { "state": "offline", "lastSeenAt": null } }
		],
		"requests": {
			"incoming": [ { "uuid": "77777777-7777-4777-8777-777777777777", "name": "Dinnerbone",
							"createdAt": "2026-10-24T17:58:00.000Z" } ],
			"outgoing": [ { "uuid": "88888888-8888-4888-8888-888888888888", "name": "Grumm",
							"createdAt": "2026-10-24T17:50:00.000Z" } ]
		} })";

	QDateTime at(const char* iso)
	{
		return QDateTime::fromString(QString::fromLatin1(iso), Qt::ISODateWithMs);
	}

	QJsonObject jsonOf(const QByteArray& body)
	{
		return QJsonDocument::fromJson(body).object();
	}
} // namespace

class FriendsApiTest : public QObject
{
	Q_OBJECT

  private slots:
	// ---- URLs and shapes

	void urlsJoinWithOrWithoutASlash()
	{
		const QUrl expected("https://api.kiteclient.com/v1/friends");
		QCOMPARE(url("https://api.kiteclient.com/", Path::friends()), expected);
		QCOMPARE(url("https://api.kiteclient.com", Path::friends()), expected);
		QCOMPARE(url("http://localhost:8086/", Path::status()),
				 QUrl("http://localhost:8086/v1/me/status"));
		QCOMPARE(mojangJoinUrl("http://localhost:5545"),
				 QUrl("http://localhost:5545/session/minecraft/join"));
	}

	void pathsFollowTheReadme()
	{
		const QString id = "853c80ef-3c37-49fd-aa49-938b674adae6";
		QCOMPARE(Path::accept(id), "/v1/friends/requests/" + id + "/accept");
		QCOMPARE(Path::decline(id), "/v1/friends/requests/" + id + "/decline");
		QCOMPARE(Path::cancel(id), "/v1/friends/requests/" + id);
		QCOMPARE(Path::removeFriend(id), "/v1/friends/" + id);
		QCOMPARE(Path::block(id), "/v1/blocks/" + id);
		QCOMPARE(Path::settings(), QString("/v1/me/settings"));
	}

	void uuidsAndNamesAreChecked()
	{
		QVERIFY(isUuid("069a79f4-44e9-4726-a5be-fca90e38aaf5"));
		QVERIFY(!isUuid("069a79f444e94726a5befca90e38aaf5"));
		QVERIFY(!isUuid("../me"));
		QVERIFY(!isUuid("069a79f4-44e9-4726-a5be-fca90e38aaf5/accept"));
		QVERIFY(isMinecraftName("jeb_"));
		QVERIFY(isMinecraftName("A"));
		QVERIFY(!isMinecraftName(""));
		QVERIFY(!isMinecraftName("seventeen_chars_xx"));
		QVERIFY(!isMinecraftName("has space"));
		QCOMPARE(undashed("069A79F4-44e9-4726-a5be-fca90e38aaf5"),
				 QString("069a79f444e94726a5befca90e38aaf5"));
	}

	void hostsAreNormalizedLikeTheServerDoes()
	{
		QCOMPARE(normalizeHost("MC.Hypixel.NET"), QString("mc.hypixel.net"));
		QCOMPARE(normalizeHost("mc.hypixel.net."), QString("mc.hypixel.net"));
		QCOMPARE(normalizeHost("1.2.3.4"), QString("1.2.3.4"));
		QCOMPARE(normalizeHost("[2001:db8::1]"), QString("2001:db8::1"));
		QCOMPARE(normalizeHost("localhost"), QString("localhost"));
		for (const char* bad :
			 {"", ".", "a..b", "-a.com", "a_b.com", "has space.com",
			  "2130706433", "0x7f.0x1", "1.2.3", "01.2.3.4", "a.com/path",
			  "[::1", "example.123"}) {
			QVERIFY2(normalizeHost(bad).isEmpty(), bad);
		}
		QVERIFY(normalizeHost(QString(300, 'a') + ".com").isEmpty());
	}

	// ---- Bodies

	void signInBodies()
	{
		QCOMPARE(jsonOf(challengeBody("Notch")).value("name").toString(),
				 QString("Notch"));
		QCOMPARE(jsonOf(loginBody("abc")).value("challengeId").toString(),
				 QString("abc"));
		const auto join = jsonOf(mojangJoinBody(
			"token", "069a79f4-44e9-4726-a5be-fca90e38aaf5", "server"));
		QCOMPARE(join.value("accessToken").toString(), QString("token"));
		QCOMPARE(join.value("selectedProfile").toString(),
				 QString("069a79f444e94726a5befca90e38aaf5"));
		QCOMPARE(join.value("serverId").toString(), QString("server"));
	}

	void anIdleStatusIsOnline()
	{
		QCOMPARE(statusBody(Presence()), QByteArray("{\"state\":\"online\"}"));
	}

	void aServerStatus()
	{
		Presence p;
		p.playing = true;
		p.instance = "Survival 1.21";
		p.version = "1.21.4";
		p.world = {World::Type::Server, "mc.hypixel.net", 25565};
		const auto body = jsonOf(statusBody(p));
		QCOMPARE(body.value("state").toString(), QString("playing"));
		QCOMPARE(body.value("instance").toString(), QString("Survival 1.21"));
		QCOMPARE(body.value("version").toString(), QString("1.21.4"));
		const auto world = body.value("world").toObject();
		QCOMPARE(world.value("type").toString(), QString("server"));
		QCOMPARE(world.value("host").toString(), QString("mc.hypixel.net"));
		QCOMPARE(world.value("port").toInt(), 25565);
	}

	void aSingleplayerStatus()
	{
		Presence p;
		p.playing = true;
		p.instance = "Survival";
		p.world.type = World::Type::Singleplayer;
		const auto world = jsonOf(statusBody(p)).value("world").toObject();
		QCOMPARE(world.value("type").toString(), QString("singleplayer"));
		QVERIFY(!world.contains("host"));
	}

	void inTheMenusTheWorldIsNull()
	{
		Presence p;
		p.playing = true;
		p.instance = "Survival";
		const auto body = jsonOf(statusBody(p));
		QVERIFY(body.contains("world"));
		QVERIFY(body.value("world").isNull());
	}

	void aStatusTheServerWouldRefuseIsRepaired()
	{
		Presence p;
		p.playing = true;
		p.instance = QString(100, 'x');
		p.version = "1.21.4 (Fabric)é";
		p.world = {World::Type::Server, "2130706433", 25565};
		auto body = jsonOf(statusBody(p));
		QCOMPARE(body.value("instance").toString().size(), 64);
		QCOMPARE(body.value("version").toString(), QString("1.21.4 Fabric"));
		QVERIFY(body.value("world").isNull());

		p.version = QString(50, '1');
		p.instance = "   ";
		p.world = {World::Type::Server, "a.example.com", 0};
		body = jsonOf(statusBody(p));
		QCOMPARE(body.value("version").toString().size(), 32);
		QVERIFY(!body.contains("instance"));
		QVERIFY(!body.value("world").toObject().contains("port"));
	}

	void settingsBodyKeepsOnlyKnownKeys()
	{
		const auto body = jsonOf(settingsBody(
			{{"visibility", "invisible"}, {"hideServer", true}, {"other", 1}}));
		QCOMPARE(body.keys().size(), 2);
		QCOMPARE(body.value("visibility").toString(), QString("invisible"));
		QCOMPARE(body.value("hideServer").toBool(), true);
		QCOMPARE(settingsBody({{"visibility", "nobody"}, {"hideServer", "yes"}}),
				 QByteArray("{}"));
	}

	// ---- Answers

	void parsesAChallenge()
	{
		const auto c = parseChallenge(kChallenge);
		QVERIFY(c);
		QCOMPARE(c->challengeId, QString("6f1c2c1e-6c55-4d0b-9d3b-6bd4f1a3e0a2"));
		QCOMPARE(c->serverId, QString("3f786850e387550fdab836ed7e6dc881de23001b"));
		QVERIFY(!parseChallenge("{}"));
		QVERIFY(!parseChallenge("not json"));
	}

	void parsesASession()
	{
		const auto s = parseSession(kLogin);
		QVERIFY(s);
		QCOMPARE(s->token, QString("kfs_Jx2v"));
		QCOMPARE(s->expiresAt, at("2026-11-23T18:00:00.000Z"));
		QCOMPARE(s->uuid, QString("069a79f4-44e9-4726-a5be-fca90e38aaf5"));
		QCOMPARE(s->name, QString("Notch"));
		QVERIFY(!parseSession(R"({"token": "x"})"));
	}

	void parsesMeAndSettings()
	{
		const auto me = parseMe(kMe);
		QVERIFY(me);
		QCOMPARE(me->visibility, QString("everyone"));
		QVERIFY(me->hideServer);
		QVERIFY(!me->shareIpServers);
		const auto settings = parseSettings(
			R"({"visibility":"invisible","hideServer":false,"shareIpServers":true})");
		QVERIFY(settings);
		QCOMPARE(settings->visibility, QString("invisible"));
		QVERIFY(settings->shareIpServers);
		QVERIFY(!parseSettings("{}"));
	}

	void parsesAStatusReply()
	{
		const auto reply = parseStatusReply(R"({
			"expiresAt": "2026-10-24T18:03:00.000Z",
			"status": { "state": "playing", "instance": "Survival 1.21", "version": "1.21.4",
						"world": { "type": "server", "host": "mc.hypixel.net", "port": 25565 } } })");
		QVERIFY(reply);
		QCOMPARE(reply->state, Status::State::Playing);
		QCOMPARE(reply->world.host, QString("mc.hypixel.net"));
		const auto invisible = parseStatusReply(
			R"({"expiresAt": "x", "status": { "state": "offline", "lastSeenAt": null }})");
		QVERIFY(invisible);
		QCOMPARE(invisible->state, Status::State::Offline);
		QVERIFY(!invisible->lastSeenAt.isValid());
	}

	void parsesTheFriendsAndRequests()
	{
		const auto snapshot = parseFriends(kFriends);
		QVERIFY(snapshot);
		QCOMPARE(snapshot->friends.size(), 7);
		const auto& jeb = snapshot->friends.first();
		QCOMPARE(jeb.name, QString("jeb_"));
		QCOMPARE(jeb.since, at("2026-10-20T12:00:00.000Z"));
		QCOMPARE(jeb.status.state, Status::State::Playing);
		QCOMPARE(jeb.status.instance, QString("Hypixel"));
		QCOMPARE(jeb.status.version, QString("1.8.9"));
		QCOMPARE(jeb.status.world.port, 25565);
		QCOMPARE(snapshot->friends.at(1).status.world.type,
				 World::Type::Singleplayer);
		QCOMPARE(snapshot->friends.at(2).status.world.type, World::Type::None);
		QCOMPARE(snapshot->friends.at(3).status.world.type, World::Type::Server);
		QVERIFY(snapshot->friends.at(3).status.world.host.isEmpty());
		QCOMPARE(snapshot->incoming.size(), 1);
		QCOMPARE(snapshot->incoming.first().name, QString("Dinnerbone"));
		QCOMPARE(snapshot->outgoing.first().name, QString("Grumm"));
		QVERIFY(!parseFriends("{}"));
		QVERIFY(!parseFriends(R"({"friends": [], "requests": {}})"));
	}

	void parsesBlocksAndRequestAnswers()
	{
		const auto blocked = parseBlocked(R"({ "blocked": [ { "uuid": "9", "name": "Griefer",
			"createdAt": "2026-10-24T17:00:00.000Z" } ] })");
		QVERIFY(blocked);
		QCOMPARE(blocked->first().name, QString("Griefer"));
		QVERIFY(!parseBlocked("{}"));

		const auto sent = parseRequestResult(
			R"({ "status": "pending", "user": { "uuid": "u", "name": "jeb_" } })");
		QVERIFY(sent);
		QCOMPARE(sent->status, QString("pending"));
		QCOMPARE(sent->name, QString("jeb_"));
		const auto user = parseUser(R"({ "user": { "uuid": "u", "name": "jeb_" } })");
		QVERIFY(user);
		QCOMPARE(user->name, QString("jeb_"));
		QVERIFY(!parseUser("{}"));
	}

	void parsesErrors()
	{
		const auto error = parseError(
			R"({ "error": "not_found", "message": "No such player" })");
		QCOMPARE(error.code, Code::NotFound);
		QCOMPARE(error.message, QString("No such player"));
		QCOMPARE(parseError("<html>").code, QString("unknown"));
		QCOMPARE(parseError("").code, QString("unknown"));
		QCOMPARE(parseError(R"({"error":"rate_limited"})").code,
				 Code::RateLimited);
	}

	// ---- What the QML side is shown

	void everyStatusIsWorded()
	{
		const auto now = at("2026-10-24T18:00:00.000Z");
		const auto snapshot = parseFriends(kFriends);
		QVERIFY(snapshot);
		const QStringList expected = {
			"On mc.hypixel.net", "Single-player", "Playing", "On a server",
			"Online",			 "Last seen 2 h ago", "Offline"};
		for (int i = 0; i < expected.size(); ++i) {
			QCOMPARE(FriendsDisplay::statusText(snapshot->friends.at(i).status, now),
					 expected.at(i));
		}
	}

	void aFriendMapHasEveryField()
	{
		const auto now = at("2026-10-24T18:00:00.000Z");
		const auto snapshot = parseFriends(kFriends);
		QVERIFY(snapshot);
		const auto playing = FriendsDisplay::friendMap(snapshot->friends.at(0), now);
		QCOMPARE(playing.value("uuid").toString(),
				 QString("853c80ef-3c37-49fd-aa49-938b674adae6"));
		QCOMPARE(playing.value("name").toString(), QString("jeb_"));
		QCOMPARE(playing.value("since").toString(),
				 QString("2026-10-20T12:00:00.000Z"));
		QCOMPARE(playing.value("state").toString(), QString("playing"));
		QCOMPARE(playing.value("lastSeenAt").toString(), QString());
		QCOMPARE(playing.value("instance").toString(), QString("Hypixel"));
		QCOMPARE(playing.value("version").toString(), QString("1.8.9"));
		QCOMPARE(playing.value("worldType").toString(), QString("server"));
		QCOMPARE(playing.value("host").toString(), QString("mc.hypixel.net"));
		QCOMPARE(playing.value("port").toInt(), 25565);
		QCOMPARE(playing.value("statusText").toString(),
				 QString("On mc.hypixel.net"));

		const auto away = FriendsDisplay::friendMap(snapshot->friends.at(5), now);
		QCOMPARE(away.value("state").toString(), QString("offline"));
		QCOMPARE(away.value("lastSeenAt").toString(),
				 QString("2026-10-24T16:00:00.000Z"));
		QCOMPARE(away.value("worldType").toString(), QString());
		QCOMPARE(away.value("instance").toString(), QString());
	}

	void relativeTimes()
	{
		const auto now = at("2026-10-24T18:00:00.000Z");
		auto ago = [&](qint64 seconds) {
			return FriendsDisplay::relativeTime(now.addSecs(-seconds), now);
		};
		QCOMPARE(ago(10), QString("Just now"));
		QCOMPARE(ago(5 * 60), QString("5 min ago"));
		QCOMPARE(ago(3 * 3600), QString("3 h ago"));
		QCOMPARE(ago(30 * 3600), QString("Yesterday"));
		QCOMPARE(ago(12 * 86400), QString("12 days ago"));
		QVERIFY(!ago(90 * 86400).contains("ago"));
	}

	void peopleAndSettingsMaps()
	{
		const auto snapshot = parseFriends(kFriends);
		QVERIFY(snapshot);
		const auto people = FriendsDisplay::personList(snapshot->incoming);
		QCOMPARE(people.size(), 1);
		QCOMPARE(people.first().toMap().value("name").toString(),
				 QString("Dinnerbone"));
		QCOMPARE(people.first().toMap().value("createdAt").toString(),
				 QString("2026-10-24T17:58:00.000Z"));
		const auto settings = FriendsDisplay::settingsMap(Settings());
		QCOMPARE(settings.value("visibility").toString(), QString("friends"));
		QCOMPARE(settings.value("hideServer").toBool(), false);
		QCOMPARE(settings.value("shareIpServers").toBool(), false);
	}

	// ---- The stored session

	void aSessionSurvivesAFile()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath("friends.json");
		QVERIFY(FriendsSessionStore::load(path).token.isEmpty());
		const auto session = *parseSession(kLogin);
		QVERIFY(FriendsSessionStore::save(path, session));
		const auto loaded = FriendsSessionStore::load(path);
		QCOMPARE(loaded.token, session.token);
		QCOMPARE(loaded.expiresAt, session.expiresAt);
		QCOMPARE(loaded.uuid, session.uuid);
		QCOMPARE(loaded.name, session.name);
		FriendsSessionStore::forget(path);
		QVERIFY(!QFile::exists(path));
	}

#ifndef Q_OS_WIN
	void theSessionFileIsPrivate()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath("friends.json");
		QVERIFY(FriendsSessionStore::save(path, *parseSession(kLogin)));
		const auto permissions = QFile::permissions(path);
		QVERIFY(permissions & QFile::ReadOwner);
		QVERIFY(permissions & QFile::WriteOwner);
		QVERIFY(!(permissions & (QFile::ReadGroup | QFile::WriteGroup |
								 QFile::ReadOther | QFile::WriteOther)));
	}
#endif

	void aBrokenFileIsNoSession()
	{
		QTemporaryDir dir;
		const QString path = dir.filePath("friends.json");
		QFile file(path);
		QVERIFY(file.open(QIODevice::WriteOnly));
		file.write("{ not json");
		file.close();
		QVERIFY(FriendsSessionStore::load(path).token.isEmpty());
	}

	void aSessionIsUsableForItsOwnAccount()
	{
		const auto session = *parseSession(kLogin);
		const auto soon = at("2026-10-24T18:00:00.000Z");
		const QString id = "069a79f444e94726a5befca90e38aaf5";
		QVERIFY(FriendsSessionStore::usable(session, id, "Notch", soon));
		/* Another account, or a renamed one, signs in again. */
		QVERIFY(!FriendsSessionStore::usable(
			session, "853c80ef3c3749fdaa49938b674adae6", "Notch", soon));
		QVERIFY(!FriendsSessionStore::usable(session, id, "NotchRenamed", soon));
		/* Expired, or about to be. */
		QVERIFY(!FriendsSessionStore::usable(session, id, "Notch",
											 at("2026-11-23T18:30:00.000Z")));
		QVERIFY(!FriendsSessionStore::usable(session, id, "Notch",
											 at("2026-11-23T17:30:00.000Z")));
		QVERIFY(!FriendsSessionStore::usable(Session(), id, "Notch", soon));
	}
};

QTEST_GUILESS_MAIN(FriendsApiTest)

#include "FriendsApi_test.moc"
