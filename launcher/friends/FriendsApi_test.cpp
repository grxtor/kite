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
		"settings": { "visibility": "everyone", "hideServer": true, "shareIpServers": false,
					  "allowJoin": false } })";
	const QByteArray kFriends = R"({
		"friends": [
			{ "uuid": "853c80ef-3c37-49fd-aa49-938b674adae6", "name": "jeb_",
			  "since": "2026-10-20T12:00:00.000Z",
			  "status": { "state": "playing", "instance": "Hypixel", "version": "1.8.9",
						  "world": { "type": "server", "host": "mc.hypixel.net", "port": 25565 },
						  "joinable": true } },
			{ "uuid": "11111111-1111-4111-8111-111111111111", "name": "Solo",
			  "since": "2026-10-21T12:00:00.000Z",
			  "status": { "state": "playing", "instance": "Survival", "version": "1.21.4",
						  "world": { "type": "singleplayer" }, "joinable": true } },
			{ "uuid": "22222222-2222-4222-8222-222222222222", "name": "Menus",
			  "since": "2026-10-21T12:00:00.000Z",
			  "status": { "state": "playing", "instance": "Survival", "version": "1.21.4",
						  "world": null } },
			{ "uuid": "33333333-3333-4333-8333-333333333333", "name": "Home",
			  "since": "2026-10-21T12:00:00.000Z",
			  "status": { "state": "playing", "instance": "Survival", "version": "1.21.4",
						  "world": { "type": "server" }, "joinable": true } },
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

	void anIpv6ZoneIdIsNotACleanHost()
	{
		// QHostAddress takes anything after the % for a zone id; the host
		// goes to the game as an argument, so none of it may pass -- "${...}"
		// would be filled in with the access token, a space would start
		// another option.
		for (const char* bad :
			 {"fe80::1%en0", "fe80::1%${auth_access_token}",
			  "fe80::1%a ${auth_access_token}", "[fe80::1%en0]",
			  "fe80::1%x --server evil.example", "::1 --demo", "2001:db8::g"}) {
			QVERIFY2(normalizeHost(bad).isEmpty(), bad);
		}
		QCOMPARE(normalizeHost("2001:DB8::1"), QString("2001:db8::1"));
		QCOMPARE(normalizeHost("::ffff:1.2.3.4"), QString("::ffff:1.2.3.4"));
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

	void settingsBodyCarriesAllowJoin()
	{
		const auto off = jsonOf(settingsBody({{"allowJoin", false}}));
		QCOMPARE(off.keys().size(), 1);
		QVERIFY(off.contains("allowJoin"));
		QCOMPARE(off.value("allowJoin").toBool(), false);
		QCOMPARE(jsonOf(settingsBody({{"allowJoin", true}}))
					 .value("allowJoin")
					 .toBool(),
				 true);
		// Not a bool: left out, like every other key.
		QCOMPARE(settingsBody({{"allowJoin", "no"}}), QByteArray("{}"));
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
		QVERIFY(!me->allowJoin);
		const auto settings = parseSettings(
			R"({"visibility":"invisible","hideServer":false,"shareIpServers":true})");
		QVERIFY(settings);
		QCOMPARE(settings->visibility, QString("invisible"));
		QVERIFY(settings->shareIpServers);
		// A server that does not send allowJoin has joining on.
		QVERIFY(settings->allowJoin);
		QVERIFY(!parseSettings("{}"));
	}

	void parsesJoinable()
	{
		const auto yes = parseStatus(jsonOf(R"({"state": "playing", "version": "1.21.4",
			"world": {"type": "server", "host": "mc.hypixel.net"}, "joinable": true})"));
		QVERIFY(yes.joinable);
		QVERIFY(yes.joinableSaid);
		// Absent means false, and that the server did not say.
		const auto absent = parseStatus(jsonOf(R"({"state": "playing", "version": "1.21.4",
			"world": {"type": "server", "host": "mc.hypixel.net"}})"));
		QVERIFY(!absent.joinable);
		QVERIFY(!absent.joinableSaid);
		// Said no is said.
		const auto no = parseStatus(jsonOf(R"({"state": "playing", "version": "1.21.4",
			"world": {"type": "server", "host": "mc.hypixel.net"}, "joinable": false})"));
		QVERIFY(!no.joinable);
		QVERIFY(no.joinableSaid);
		// Only someone playing can be joined.
		QVERIFY(!parseStatus(jsonOf(R"({"state": "online", "joinable": true})"))
					 .joinable);
		QVERIFY(!parseStatus(jsonOf(R"({"state": "offline", "lastSeenAt": null,
			"joinable": true})")).joinable);
	}

	void aStatusReplyCarriesJoinable()
	{
		// What the server answers to your own report is what friends see of
		// you, so it carries joinable like a friend's status does.
		const auto reply = parseStatusReply(R"({
			"expiresAt": "2026-10-24T18:03:00.000Z",
			"status": { "state": "playing", "version": "1.21.4",
						"world": { "type": "server", "host": "mc.hypixel.net" },
						"joinable": true } })");
		QVERIFY(reply);
		QVERIFY(reply->joinable);
		const auto closed = parseStatusReply(R"({
			"expiresAt": "x",
			"status": { "state": "playing", "version": "1.21.4",
						"world": { "type": "server", "host": "mc.hypixel.net" } } })");
		QVERIFY(closed);
		QVERIFY(!closed->joinable);
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
			"On mc.hypixel.net", "Singleplayer", "Playing", "On a server",
			"In Kite",			 "Last seen 2 h ago", "Offline"};
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
		QCOMPARE(playing.value("joinable").toBool(), true);
		QCOMPARE(playing.value("statusText").toString(),
				 QString("On mc.hypixel.net"));

		// Joinable needs a server with a visible host, whatever the server
		// said: singleplayer and a hidden host are never joined.
		const auto solo = FriendsDisplay::friendMap(snapshot->friends.at(1), now);
		QCOMPARE(solo.value("joinable").toBool(), false);
		const auto hidden = FriendsDisplay::friendMap(snapshot->friends.at(3), now);
		QCOMPARE(hidden.value("worldType").toString(), QString("server"));
		QCOMPARE(hidden.value("joinable").toBool(), false);
		const auto idle = FriendsDisplay::friendMap(snapshot->friends.at(4), now);
		QCOMPARE(idle.value("joinable").toBool(), false);

		const auto away = FriendsDisplay::friendMap(snapshot->friends.at(5), now);
		QCOMPARE(away.value("state").toString(), QString("offline"));
		QCOMPARE(away.value("lastSeenAt").toString(),
				 QString("2026-10-24T16:00:00.000Z"));
		QCOMPARE(away.value("worldType").toString(), QString());
		QCOMPARE(away.value("instance").toString(), QString());
	}

	void joinNeedsACleanHost()
	{
		const auto now = at("2026-10-24T18:00:00.000Z");
		Friend one;
		one.uuid = "853c80ef-3c37-49fd-aa49-938b674adae6";
		one.name = "jeb_";
		one.status.state = Status::State::Playing;
		one.status.world.type = World::Type::Server;
		one.status.joinable = true;
		one.status.joinableSaid = true;

		one.status.world.host = "mc.hypixel.net";
		QCOMPARE(FriendsDisplay::friendMap(one, now).value("joinable").toBool(),
				 true);
		// Not something the server would have let through.
		for (const char* host :
			 {"evil.example --demo", "MC.Hypixel.NET", "mc.hypixel.net.",
			  "2130706433", ""}) {
			one.status.world.host = host;
			QVERIFY2(!FriendsDisplay::friendMap(one, now)
						  .value("joinable")
						  .toBool(),
					 host);
		}
		// And not joinable at all when the friend says so -- which a row
		// says too, instead of just having no Join.
		one.status.world.host = "mc.hypixel.net";
		one.status.joinable = false;
		const auto off = FriendsDisplay::friendMap(one, now);
		QCOMPARE(off.value("joinable").toBool(), false);
		QCOMPARE(off.value("joinOff").toBool(), true);
		one.status.joinable = true;
		QCOMPARE(FriendsDisplay::friendMap(one, now).value("joinOff").toBool(),
				 false);
	}

	void aServerThatSaysNothingIsNotBlamedOnTheFriend()
	{
		const auto now = at("2026-10-24T18:00:00.000Z");
		// What an older server sends: a visible host, no joinable.
		const auto status = parseStatus(jsonOf(R"({"state": "playing",
			"version": "1.21.4",
			"world": {"type": "server", "host": "mc.hypixel.net"}})"));
		Friend one;
		one.uuid = "853c80ef-3c37-49fd-aa49-938b674adae6";
		one.name = "jeb_";
		one.status = status;
		const auto map = FriendsDisplay::friendMap(one, now);
		// No Join, which is the safe side, but not "doesn't let friends
		// join" either: they never said so.
		QCOMPARE(map.value("joinable").toBool(), false);
		QCOMPARE(map.value("joinOff").toBool(), false);
	}

	void joinNeverLeadsToYourOwnMachine()
	{
		const auto now = at("2026-10-24T18:00:00.000Z");
		Friend one;
		one.uuid = "853c80ef-3c37-49fd-aa49-938b674adae6";
		one.name = "jeb_";
		one.status.state = Status::State::Playing;
		one.status.world.type = World::Type::Server;
		one.status.joinable = true;
		auto joinable = [&](const char* host) {
			one.status.world.host = host;
			return FriendsDisplay::friendMap(one, now).value("joinable").toBool();
		};
		// A friend's localhost is not yours.
		for (const char* host : {"localhost", "mc.localhost", "127.0.0.1",
								 "127.8.8.8", "::1", "0.0.0.0", "::"}) {
			QVERIFY2(!joinable(host), host);
		}
		// A server on a home network or an IPv6 address can be a friend's
		// (the same house, a VPN) and stays offered.
		for (const char* host : {"192.168.1.10", "10.0.0.5", "2001:db8::1",
								 "mc.hypixel.net", "notlocalhost.com"}) {
			QVERIFY2(joinable(host), host);
		}
		QVERIFY(isLoopbackHost("localhost"));
		QVERIFY(!isLoopbackHost("localhost.example.com"));
		QVERIFY(!isLoopbackHost("8.8.8.8"));
	}

	void whatYouShowIsWhatYouAreDoing()
	{
		Presence idle;
		Presence onServer;
		onServer.playing = true;
		onServer.world.type = World::Type::Server;
		onServer.world.host = "mc.hypixel.net";
		auto status = [](const Presence& presence, const Settings& settings) {
			return FriendsDisplay::selfStatusMap(
				presence, FriendsDisplay::settingsMap(settings));
		};

		QCOMPARE(status(idle, Settings()).value("playing").toBool(), false);
		const auto shown = status(onServer, Settings());
		QCOMPARE(shown.value("host").toString(), QString("mc.hypixel.net"));
		QCOMPARE(shown.value("joinable").toBool(), true);
		QCOMPARE(shown.value("serverHidden").toBool(), false);

		Settings noJoin;
		noJoin.allowJoin = false;
		QCOMPARE(status(onServer, noJoin).value("joinable").toBool(), false);

		Settings hide;
		hide.hideServer = true;
		const auto hidden = status(onServer, hide);
		QCOMPARE(hidden.value("host").toString(), QString());
		QCOMPARE(hidden.value("serverHidden").toBool(), true);
		QCOMPARE(hidden.value("joinable").toBool(), false);
		QCOMPARE(hidden.value("playing").toBool(), true);

		// A raw IP stays with the server unless it is allowed out.
		onServer.world.host = "203.0.113.7";
		QCOMPARE(status(onServer, Settings()).value("serverHidden").toBool(),
				 true);
		Settings share;
		share.shareIpServers = true;
		QCOMPARE(status(onServer, share).value("host").toString(),
				 QString("203.0.113.7"));

		Presence solo;
		solo.playing = true;
		solo.world.type = World::Type::Singleplayer;
		const auto alone = status(solo, Settings());
		QCOMPARE(alone.value("worldType").toString(), QString("singleplayer"));
		QCOMPARE(alone.value("joinable").toBool(), false);
	}

	void whatTheServerShowsIsWhatYouSee()
	{
		Presence local;
		local.playing = true;
		local.world.type = World::Type::Server;
		local.world.host = "mc.lan";
		// The server keeps a name that only works on a home network from
		// friends, which the launcher's own reading of the settings cannot
		// tell: the answer is the truth.
		const auto kept = parseStatus(jsonOf(R"({"state": "playing",
			"version": "1.21.4", "world": {"type": "server"}})"));
		const auto hidden = FriendsDisplay::selfStatusFromView(local, kept);
		QCOMPARE(hidden.value("playing").toBool(), true);
		QCOMPARE(hidden.value("host").toString(), QString());
		QCOMPARE(hidden.value("serverHidden").toBool(), true);
		QCOMPARE(hidden.value("joinable").toBool(), false);

		Presence onServer;
		onServer.playing = true;
		onServer.world.type = World::Type::Server;
		onServer.world.host = "mc.hypixel.net";
		const auto shown = parseStatus(jsonOf(R"({"state": "playing",
			"version": "1.21.4", "joinable": true,
			"world": {"type": "server", "host": "mc.hypixel.net"}})"));
		const auto open = FriendsDisplay::selfStatusFromView(onServer, shown);
		QCOMPARE(open.value("host").toString(), QString("mc.hypixel.net"));
		QCOMPARE(open.value("serverHidden").toBool(), false);
		QCOMPARE(open.value("joinable").toBool(), true);
		// The server did not say joinable (an older one, or joining off).
		const auto noJoin = parseStatus(jsonOf(R"({"state": "playing",
			"version": "1.21.4",
			"world": {"type": "server", "host": "mc.hypixel.net"}})"));
		QCOMPARE(FriendsDisplay::selfStatusFromView(onServer, noJoin)
					 .value("joinable").toBool(),
				 false);

		// Invisible: the server shows you as offline, whatever you play.
		const auto offline = parseStatus(jsonOf(
			R"({"state": "offline", "lastSeenAt": null})"));
		const auto invisible =
			FriendsDisplay::selfStatusFromView(onServer, offline);
		QCOMPARE(invisible.value("playing").toBool(), false);
		QCOMPARE(invisible.value("joinable").toBool(), false);
		QCOMPARE(invisible.value("serverHidden").toBool(), false);

		Presence solo;
		solo.playing = true;
		solo.world.type = World::Type::Singleplayer;
		const auto alone = parseStatus(jsonOf(R"({"state": "playing",
			"version": "1.21.4", "world": {"type": "singleplayer"}})"));
		const auto soloView = FriendsDisplay::selfStatusFromView(solo, alone);
		QCOMPARE(soloView.value("worldType").toString(), QString("singleplayer"));
		QCOMPARE(soloView.value("serverHidden").toBool(), false);
	}

	void aTokenAboutToExpireIsRenewedOnceAndAwaited()
	{
		const auto now = at("2026-10-24T18:00:00.000Z");
		const auto stale = at("2026-10-24T18:02:00.000Z");
		const auto fresh = at("2026-10-24T19:00:00.000Z");
		auto step = [&](const QDateTime& expiry, bool account, bool queued,
						bool asked, bool timedOut) {
			return renewalStep(expiry, now, account, queued, asked, timedOut);
		};

		// A fresh token, or one with no known end, needs nothing.
		QCOMPARE(step(fresh, true, true, false, false), RenewalStep::Proceed);
		QCOMPARE(step(QDateTime(), false, false, false, false),
				 RenewalStep::Proceed);
		// About to expire: the first time it is asked for, then awaited.
		QCOMPARE(step(stale, false, false, false, false),
				 RenewalStep::AskForRenewal);
		// Its own refresh is under way.
		QCOMPARE(step(stale, true, true, true, false), RenewalStep::Wait);
		// Asked, and still in the queue (behind another account's refresh, or
		// in the pause the list makes after it, when nothing runs): wait for
		// its turn instead of signing in with the old token.
		QCOMPARE(step(stale, false, true, true, false), RenewalStep::Wait);
		// Asked, and out of the queue without running: the refresh ran (or
		// could not be started), and this is what it left.
		QCOMPARE(step(stale, false, false, true, false), RenewalStep::Proceed);
		// Waited long enough.
		QCOMPARE(step(stale, true, true, true, true), RenewalStep::Proceed);
		QCOMPARE(step(stale, false, true, true, true), RenewalStep::Proceed);
	}

	void onlyANewAccountIsAReasonToSignInAgainAtOnce()
	{
		// Nothing failed yet: any change is a reason (signed out, no account).
		QVERIFY(accountChangeRetriesSignIn(0, "", "a|p|n|t", "", "p"));
		// A game starting or stopping, another account's refresh: the default
		// account is what it was, so the schedule stands.
		QVERIFY(!accountChangeRetriesSignIn(2, "a|p|n|t", "a|p|n|t", "", "p"));
		// Another account, a renewed token, a new name: worth a try.
		QVERIFY(accountChangeRetriesSignIn(2, "a|p|n|t", "b|q|m|t", "", "q"));
		QVERIFY(accountChangeRetriesSignIn(2, "a|p|n|t", "a|p|n|u", "", "p"));
		// Refused for good: a renewed token does not change that, another
		// profile does.
		QVERIFY(!accountChangeRetriesSignIn(4, "a|p|n|t", "a|p|n|u", "p", "p"));
		QVERIFY(accountChangeRetriesSignIn(4, "a|p|n|t", "b|q|m|t", "p", "q"));
	}

	void aFailedSignInWaitsLongerEachTime()
	{
		QCOMPARE(signInBackoffMs(1), 60 * 1000);
		QCOMPARE(signInBackoffMs(2), 2 * 60 * 1000);
		QCOMPARE(signInBackoffMs(3), 5 * 60 * 1000);
		QCOMPARE(signInBackoffMs(4), 15 * 60 * 1000);
		// And stays there; no failures yet is the shortest wait, not a crash.
		QCOMPARE(signInBackoffMs(50), 15 * 60 * 1000);
		QCOMPARE(signInBackoffMs(0), 60 * 1000);
	}

	void mojangRefusalsAreNamed()
	{
		QCOMPARE(parseMojangError(
					 R"({"error":"InsufficientPrivilegesException",)"
					 R"("errorMessage":"Multiplayer is disabled"})"),
				 MojangError::InsufficientPrivileges);
		QCOMPARE(parseMojangError(R"({"error":"UserBannedException"})"),
				 MojangError::UserBanned);
		QCOMPARE(parseMojangError(R"({"error":"ForbiddenOperationException"})"),
				 MojangError::Forbidden);
		// An outage's page or an empty answer names nothing.
		QCOMPARE(parseMojangError("<html>Bad gateway</html>"), QString());
		QCOMPARE(parseMojangError(QByteArray()), QString());
	}

	void lastSeenTexts()
	{
		const auto now = at("2026-10-24T18:00:00.000Z");
		auto seen = [&](qint64 seconds) {
			return FriendsDisplay::lastSeenText(now.addSecs(-seconds), now);
		};
		// A whole sentence each, so nothing stands capitalised mid-line.
		QCOMPARE(seen(10), QString("Last seen just now"));
		QCOMPARE(seen(5 * 60), QString("Last seen 5 min ago"));
		QCOMPARE(seen(3 * 3600), QString("Last seen 3 h ago"));
		QCOMPARE(seen(30 * 3600), QString("Last seen yesterday"));
		QCOMPARE(seen(12 * 86400), QString("Last seen 12 days ago"));
		QVERIFY(seen(90 * 86400).startsWith("Last seen "));
		QVERIFY(!seen(90 * 86400).contains("ago"));
	}

	void settingsComeBackAsAsked()
	{
		const auto saved = FriendsDisplay::settingsMap(Settings{});
		// Joining is on by default, so an older server that drops the key
		// still "saves" a request for on: it is on either way.
		QVERIFY(FriendsDisplay::settingsSaved(
			settingsBody({{"allowJoin", true}}), saved));
		// An older server drops "allowJoin": false and answers without it,
		// which reads as the default, on.
		QVERIFY(!FriendsDisplay::settingsSaved(
			settingsBody({{"allowJoin", false}}), saved));
		Settings changed;
		changed.hideServer = true;
		changed.visibility = "invisible";
		QVERIFY(FriendsDisplay::settingsSaved(
			settingsBody({{"hideServer", true}, {"visibility", "invisible"}}),
			FriendsDisplay::settingsMap(changed)));
		QVERIFY(!FriendsDisplay::settingsSaved(
			settingsBody({{"hideServer", true}, {"visibility", "everyone"}}),
			FriendsDisplay::settingsMap(changed)));
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
		QCOMPARE(settings.value("allowJoin").toBool(), true);
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
