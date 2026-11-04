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
#include <QTemporaryDir>
#include <QTest>

#include "FriendsApi.h"
#include "FriendsController.h"
#include "FriendsDisplay.h"
#include "FriendsInvites.h"
#include "FriendsNotices.h"

using namespace FriendsInvites;

namespace
{
	const QString kJeb = QStringLiteral("853c80ef-3c37-49fd-aa49-938b674adae6");
	const QString kAnn = QStringLiteral("11111111-1111-4111-8111-111111111111");
	const QDateTime kNow =
		QDateTime::fromString("2026-10-24T18:00:00.000Z", Qt::ISODateWithMs);

	QJsonObject json(const QByteArray& text)
	{
		return QJsonDocument::fromJson(text).object();
	}

	/* An invite as the server writes it, made @p ageMinutes before kNow. */
	QJsonObject inviteJson(const QString& uuid, const QString& name,
						   int ageMinutes, const QString& host = "mc.hypixel.net")
	{
		constexpr int kLifeMinutes = 10;
		constexpr int kSecondsPerMinute = 60;
		const QDateTime created = kNow.addSecs(-ageMinutes * kSecondsPerMinute);
		return {{"uuid", uuid},
				{"name", name},
				{"host", host},
				{"port", 25565},
				{"createdAt", created.toString(Qt::ISODateWithMs)},
				{"expiresAt", created.addSecs(kLifeMinutes * kSecondsPerMinute)
								  .toString(Qt::ISODateWithMs)}};
	}
} // namespace

class FriendsInvitesTest : public QObject
{
	Q_OBJECT

  private slots:
	/* GET /v1/friends carries them; a server without invites leaves them out. */
	void friendsSnapshotCarriesInvites()
	{
		const QByteArray body = R"({ "friends": [], "requests": { "incoming": [], "outgoing": [] },
			"invites": {
				"incoming": [ { "uuid": "853c80ef-3c37-49fd-aa49-938b674adae6", "name": "jeb_",
								"host": "mc.hypixel.net", "port": 25565,
								"createdAt": "2026-10-24T17:59:00.000Z",
								"expiresAt": "2026-10-24T18:09:00.000Z" } ],
				"outgoing": [] } })";
		const auto snapshot = FriendsApi::parseFriends(body);
		QVERIFY(snapshot);
		QCOMPARE(snapshot->invites.incoming.size(), 1);
		QCOMPARE(snapshot->invites.incoming.first().name, QStringLiteral("jeb_"));
		QCOMPARE(snapshot->invites.incoming.first().port, 25565);
		QVERIFY(snapshot->invites.outgoing.isEmpty());

		const auto older = FriendsApi::parseFriends(
			R"({ "friends": [], "requests": { "incoming": [], "outgoing": [] } })");
		QVERIFY(older);
		QVERIFY(older->invites.incoming.isEmpty());
	}

	void parseNeedsTheFieldsAndDefaultsThePort()
	{
		QJsonObject noPort = inviteJson(kJeb, "jeb_", 1);
		noPort.remove("port");
		QCOMPARE(parse(noPort)->port, kDefaultPort);
		for (const char* field : {"uuid", "name", "host", "expiresAt"}) {
			QJsonObject missing = inviteJson(kJeb, "jeb_", 1);
			missing.remove(field);
			QVERIFY2(!parse(missing), field);
		}
		QVERIFY(!parse(inviteJson("not-a-uuid", "jeb_", 1)));
		QVERIFY(!parse(inviteJson(kJeb, "jeb_", 1, "bad host!")));
		QVERIFY(!parseLists(json(R"({ "incoming": {} , "outgoing": [] })")));
		QVERIFY(parseBody(QJsonDocument(inviteJson(kAnn, "Ann", 0)).toJson()));
	}

	void liveDropsTheExpiredAndPutsTheNewestFirst()
	{
		const QList<FriendsApi::Invite> all = {*parse(inviteJson(kJeb, "jeb_", 9)),
											   *parse(inviteJson(kAnn, "Ann", 2)),
											   *parse(inviteJson(kAnn, "Old", 11))};
		const auto live = FriendsInvites::live(all, kNow);
		QCOMPARE(live.size(), 2);
		QCOMPARE(live.at(0).name, QStringLiteral("Ann"));
		QCOMPARE(live.at(1).name, QStringLiteral("jeb_"));
		QCOMPARE(nextExpiry(live), live.at(1).expiresAt);
		QVERIFY(!nextExpiry({}).isValid());
	}

	/* The seam the socket uses: invite, invite.cancelled, friend.removed. */
	void eventsChangeTheIncomingInvites()
	{
		QList<FriendsApi::Invite> incoming = {*parse(inviteJson(kJeb, "jeb_", 5))};
		const QJsonObject again{{"type", "invite"},
								{"invite", inviteJson(kJeb, "jeb_", 0, "play.example.net")}};
		QCOMPARE(applyEvent(incoming, again, kNow), EventEffect::Added);
		QCOMPARE(incoming.size(), 1);
		QCOMPARE(incoming.first().host, QStringLiteral("play.example.net"));

		const QJsonObject ann{{"type", "invite"}, {"invite", inviteJson(kAnn, "Ann", 0)}};
		QCOMPARE(applyEvent(incoming, ann, kNow), EventEffect::Added);
		QCOMPARE(incoming.first().name, QStringLiteral("Ann"));

		const QJsonObject stale{{"type", "invite"}, {"invite", inviteJson(kAnn, "Ann", 12)}};
		QCOMPARE(applyEvent(incoming, stale, kNow), EventEffect::Ignored);
		QCOMPARE(applyEvent(incoming, json(R"({ "type": "invite" })"), kNow),
				 EventEffect::Ignored);
		QCOMPARE(applyEvent(incoming, json(R"({ "type": "chat.message" })"), kNow),
				 EventEffect::Ignored);

		const QJsonObject cancelled{{"type", "invite.cancelled"}, {"uuid", kAnn}};
		QCOMPARE(applyEvent(incoming, cancelled, kNow), EventEffect::Removed);
		QCOMPARE(applyEvent(incoming, cancelled, kNow), EventEffect::Ignored);
		const QJsonObject removed{{"type", "friend.removed"}, {"uuid", kJeb}};
		QCOMPARE(applyEvent(incoming, removed, kNow), EventEffect::Removed);
		QVERIFY(incoming.isEmpty());
	}

	void addressKindFollowsTheServer_data()
	{
		QTest::addColumn<QString>("host");
		QTest::addColumn<QString>("kind");
		QTest::newRow("name") << "mc.hypixel.net" << "domain";
		QTest::newRow("public ip") << "203.0.113.7" << "ip";
		QTest::newRow("home ip") << "192.168.1.20" << "local";
		QTest::newRow("cgnat") << "100.64.0.1" << "local";
		QTest::newRow("loopback") << "127.0.0.1" << "local";
		QTest::newRow("ipv6") << "2001:db8::1" << "ip";
		QTest::newRow("ipv6 ula") << "fd00::1" << "local";
		QTest::newRow("mapped ipv4") << "::ffff:192.168.0.5" << "local";
		QTest::newRow("single label") << "nas" << "local";
		QTest::newRow(".local") << "myserver.local" << "local";
		QTest::newRow("fritz.box") << "nas.fritz.box" << "local";
		QTest::newRow("nip.io") << "1.2.3.4.nip.io" << "ip";
		QTest::newRow("embedded") << "ip-10-0-0-1.example.com" << "ip";
		QTest::newRow("numbers but not ip") << "play.1.21.net" << "domain";
	}
	void addressKindFollowsTheServer()
	{
		QFETCH(QString, host);
		QFETCH(QString, kind);
		QCOMPARE(addressKindName(addressKind(host)), kind);
	}

	void inviteBodyAndSettings()
	{
		QCOMPARE(FriendsApi::inviteBody("MC.Hypixel.NET.", 25565),
				 QByteArray(R"({"host":"mc.hypixel.net","port":25565})"));
		QCOMPARE(FriendsApi::inviteBody("mc.hypixel.net", 0),
				 QByteArray(R"({"host":"mc.hypixel.net"})"));
		QCOMPARE(FriendsApi::settingsBody({{"allowInvites", false}}),
				 QByteArray(R"({"allowInvites":false})"));
		const auto me = FriendsApi::parseMe(R"({ "settings": { "visibility": "friends" } })");
		QVERIFY(me && me->allowInvites);
		QCOMPARE(FriendsDisplay::settingsMap(*me).value("allowInvites"), QVariant(true));
		QCOMPARE(FriendsApi::Path::dismissInvite(kJeb),
				 QStringLiteral("/v1/invites/") + kJeb + "/dismiss");
	}

	/* A new invite is announced once, whether the poll or the socket brought it. */
	void invitesAreAnnouncedOnce()
	{
		FriendsApi::FriendsSnapshot snapshot;
		auto first = FriendsNotices::diff({}, snapshot, kNow);
		snapshot.invites.incoming = {*parse(inviteJson(kJeb, "jeb_", 1))};
		auto second = FriendsNotices::diff(first.memory, snapshot, kNow);
		QCOMPARE(second.notices.size(), 1);
		QCOMPARE(second.notices.first().kind, FriendsNotices::Notice::Kind::Invite);
		QCOMPARE(second.notices.first().host, QStringLiteral("mc.hypixel.net"));
		QVERIFY(FriendsNotices::diff(second.memory, snapshot, kNow).notices.isEmpty());

		auto memory = second.memory;
		const auto pushed = *parse(inviteJson(kAnn, "Ann", 0));
		QVERIFY(FriendsNotices::invite(memory, pushed));
		QVERIFY(!FriendsNotices::invite(memory, pushed));
		snapshot.invites.incoming.append(pushed);
		QVERIFY(FriendsNotices::diff(memory, snapshot, kNow).notices.isEmpty());
	}

	/* FriendsController::applyInviteEvent() with a fake socket frame. */
	void controllerTakesPushedInvites()
	{
		QTemporaryDir dir;
		FriendsController controller(dir.path());
		QSignalSpy changed(&controller, &FriendsController::changed);
		const QDateTime now = QDateTime::currentDateTimeUtc();
		QJsonObject invite = inviteJson(kJeb, "jeb_", 0, "192.168.1.20");
		invite["createdAt"] = now.toString(Qt::ISODateWithMs);
		constexpr int kTenMinutes = 10 * 60;
		invite["expiresAt"] = now.addSecs(kTenMinutes).toString(Qt::ISODateWithMs);
		const QJsonObject event{{"type", "invite"}, {"invite", invite}};

		controller.applyInviteEvent(event);
		QVERIFY2(controller.invites().isEmpty(), "Friends is off: nothing is taken");

		controller.m_enabled = true;
		controller.applyInviteEvent(event);
		QCOMPARE(changed.size(), 1);
		QCOMPARE(controller.invites().size(), 1);
		const QVariantMap shown = controller.invites().first().toMap();
		QCOMPARE(shown.value("name").toString(), QStringLiteral("jeb_"));
		QCOMPARE(shown.value("addressKind").toString(), QStringLiteral("local"));
		QVERIFY(shown.contains("face"));

		controller.applyInviteEvent(json(R"({ "type": "friend.status" })"));
		QCOMPARE(changed.size(), 1);
		controller.applyInviteEvent({{"type", "invite.cancelled"}, {"uuid", kJeb}});
		QCOMPARE(changed.size(), 2);
		QVERIFY(controller.invites().isEmpty());
		controller.m_enabled = false;
	}
};

QTEST_GUILESS_MAIN(FriendsInvitesTest)

#include "FriendsInvites_test.moc"
