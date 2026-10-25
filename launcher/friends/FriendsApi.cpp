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

#include "FriendsApi.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

namespace FriendsApi
{
	namespace Code
	{
		const QString Unauthorized = QStringLiteral("unauthorized");
		const QString NotJoined = QStringLiteral("not_joined");
		const QString NotFound = QStringLiteral("not_found");
		const QString BadRequest = QStringLiteral("bad_request");
		const QString AlreadyFriends = QStringLiteral("already_friends");
		const QString BlockedByYou = QStringLiteral("blocked_by_you");
		const QString LimitReached = QStringLiteral("limit_reached");
		const QString RateLimited = QStringLiteral("rate_limited");
	} // namespace Code

	namespace MojangError
	{
		const QString Forbidden =
			QStringLiteral("ForbiddenOperationException");
		const QString InsufficientPrivileges =
			QStringLiteral("InsufficientPrivilegesException");
		const QString UserBanned = QStringLiteral("UserBannedException");
	} // namespace MojangError

	namespace
	{
		/* The server's own limits (src/schemas.ts, serverAddress.ts). */
		constexpr int kMaxInstanceLength = 64;
		constexpr int kMaxVersionLength = 32;
		constexpr int kMaxHostLength = 253;
		constexpr int kMaxPort = 65535;
		/* The access token is renewed when it has less than this left: a
		 * sign-in takes seconds, not minutes. */
		constexpr qint64 kTokenMarginSeconds = 5 * 60;
		const QString kUuidPattern = QStringLiteral(
			"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
			"[0-9a-fA-F]{12}$");
		const QString kNamePattern = QStringLiteral("^[A-Za-z0-9_]{1,16}$");
		const QString kLabelPattern =
			QStringLiteral("^[a-z0-9](?:[a-z0-9-]{0,61}[a-z0-9])?$");
		const QString kIpv4Pattern = QStringLiteral(
			"^(?:(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)\\.){3}"
			"(?:25[0-5]|2[0-4]\\d|1\\d\\d|[1-9]\\d|\\d)$");
		const QString kNumericLabelPattern =
			QStringLiteral("^(?:\\d+|0x[0-9a-f]*)$");
		/* What an IPv6 address is written with, once lower-cased. A zone id
		 * (fe80::1%en0) is not among them: QHostAddress takes anything after
		 * the % for one, spaces and "${...}" included. */
		const QString kIpv6CharsPattern = QStringLiteral("^[0-9a-f:.]+$");
		/* Waits after a failed sign-in, by how often it has failed. */
		constexpr int kSignInBackoffMs[] = {60 * 1000, 2 * 60 * 1000,
											5 * 60 * 1000, 15 * 60 * 1000};
		const QString kLocalhostName = QStringLiteral("localhost");
		const QString kBadVersionChars =
			QStringLiteral("[^A-Za-z0-9._+ -]");

		bool matches(const QString& pattern, const QString& text)
		{
			return QRegularExpression(pattern).match(text).hasMatch();
		}

		QJsonObject objectOf(const QByteArray& body, bool* ok = nullptr)
		{
			QJsonParseError error;
			auto doc = QJsonDocument::fromJson(body, &error);
			const bool good =
				error.error == QJsonParseError::NoError && doc.isObject();
			if (ok) {
				*ok = good;
			}
			return good ? doc.object() : QJsonObject();
		}

		QByteArray compact(const QJsonObject& object)
		{
			return QJsonDocument(object).toJson(QJsonDocument::Compact);
		}

		QDateTime parseTime(const QJsonValue& value)
		{
			if (!value.isString()) {
				return QDateTime();
			}
			return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
		}

		QString trimmedSlash(const QString& base)
		{
			QString out = base;
			while (out.endsWith('/')) {
				out.chop(1);
			}
			return out;
		}

		World parseWorld(const QJsonValue& value)
		{
			World world;
			if (!value.isObject()) {
				return world;
			}
			const auto object = value.toObject();
			const auto type = object.value("type").toString();
			if (type == "server") {
				world.type = World::Type::Server;
				world.host = object.value("host").toString();
				world.port = object.value("port").toInt();
			} else if (type == "singleplayer") {
				world.type = World::Type::Singleplayer;
			}
			return world;
		}

		Person parsePerson(const QJsonObject& object)
		{
			Person person;
			person.uuid = object.value("uuid").toString();
			person.name = object.value("name").toString();
			person.createdAt = parseTime(object.value("createdAt"));
			return person;
		}

		bool hasIdentity(const Person& person)
		{
			return !person.uuid.isEmpty() && !person.name.isEmpty();
		}

		QList<Person> parsePeople(const QJsonValue& value, bool* ok)
		{
			QList<Person> people;
			if (!value.isArray()) {
				*ok = false;
				return people;
			}
			for (const auto& entry : value.toArray()) {
				auto person = parsePerson(entry.toObject());
				if (hasIdentity(person)) {
					people.append(person);
				}
			}
			return people;
		}

		Settings settingsOf(const QJsonObject& object)
		{
			Settings settings;
			settings.visibility = object.value("visibility").toString();
			settings.hideServer = object.value("hideServer").toBool();
			settings.shareIpServers = object.value("shareIpServers").toBool();
			/* An older server does not know the field; joining is on by
			 * default, as it is there. */
			settings.allowJoin = object.value("allowJoin").toBool(true);
			return settings;
		}

		QJsonValue worldJson(const World& world)
		{
			QJsonObject object;
			if (world.type == World::Type::Singleplayer) {
				object.insert("type", "singleplayer");
				return object;
			}
			if (world.type != World::Type::Server) {
				return QJsonValue(QJsonValue::Null);
			}
			const QString host = normalizeHost(world.host);
			if (host.isEmpty()) {
				return QJsonValue(QJsonValue::Null);
			}
			object.insert("type", "server");
			object.insert("host", host);
			if (world.port >= 1 && world.port <= kMaxPort) {
				object.insert("port", world.port);
			}
			return object;
		}
	} // namespace

	QUrl url(const QString& base, const QString& path)
	{
		return QUrl(trimmedSlash(base) + path);
	}

	QUrl mojangJoinUrl(const QString& base)
	{
		return url(base, QStringLiteral("/session/minecraft/join"));
	}

	namespace Path
	{
		QString challenge()
		{
			return QStringLiteral("/v1/auth/challenge");
		}
		QString login()
		{
			return QStringLiteral("/v1/auth/login");
		}
		QString logout()
		{
			return QStringLiteral("/v1/auth/logout");
		}
		QString me()
		{
			return QStringLiteral("/v1/me");
		}
		QString settings()
		{
			return QStringLiteral("/v1/me/settings");
		}
		QString status()
		{
			return QStringLiteral("/v1/me/status");
		}
		QString friends()
		{
			return QStringLiteral("/v1/friends");
		}
		QString requests()
		{
			return QStringLiteral("/v1/friends/requests");
		}
		QString blocks()
		{
			return QStringLiteral("/v1/blocks");
		}
		QString accept(const QString& uuid)
		{
			return requests() + "/" + uuid + "/accept";
		}
		QString decline(const QString& uuid)
		{
			return requests() + "/" + uuid + "/decline";
		}
		QString cancel(const QString& uuid)
		{
			return requests() + "/" + uuid;
		}
		QString removeFriend(const QString& uuid)
		{
			return friends() + "/" + uuid;
		}
		QString block(const QString& uuid)
		{
			return blocks() + "/" + uuid;
		}
	} // namespace Path

	bool isUuid(const QString& uuid)
	{
		return matches(kUuidPattern, uuid);
	}

	bool isMinecraftName(const QString& name)
	{
		return matches(kNamePattern, name);
	}

	QString undashed(const QString& uuid)
	{
		return uuid.toLower().remove('-');
	}

	QString normalizeHost(const QString& raw)
	{
		/* A Forge handshake marker follows the host after a NUL. */
		QString host = raw.section(QChar(0), 0, 0).trimmed().toLower();
		if (host.startsWith('[') && host.endsWith(']')) {
			host = host.mid(1, host.size() - 2);
		}
		if (matches(kIpv4Pattern, host)) {
			return host;
		}
		if (host.contains(':')) {
			QHostAddress address;
			return matches(kIpv6CharsPattern, host) &&
						   address.setAddress(host) &&
						   address.protocol() ==
							   QAbstractSocket::IPv6Protocol
					   ? host
					   : QString();
		}
		if (host.endsWith('.')) {
			host.chop(1);
		}
		if (host.isEmpty() || host.size() > kMaxHostLength) {
			return QString();
		}
		const QStringList labels = host.split('.');
		for (const QString& label : labels) {
			if (!matches(kLabelPattern, label)) {
				return QString();
			}
		}
		/* A numeric last label is an IPv4 address in another notation, or
		 * malformed. */
		if (matches(kNumericLabelPattern, labels.last())) {
			return QString();
		}
		return host;
	}

	bool isLoopbackHost(const QString& host)
	{
		if (host == kLocalhostName ||
			host.endsWith(QLatin1Char('.') + kLocalhostName)) {
			return true;
		}
		QHostAddress address;
		if (!address.setAddress(host)) {
			return false;
		}
		return address.isLoopback() || address == QHostAddress::AnyIPv4 ||
			   address == QHostAddress::AnyIPv6;
	}

	int signInBackoffMs(int failures)
	{
		constexpr int last =
			int(sizeof(kSignInBackoffMs) / sizeof(kSignInBackoffMs[0])) - 1;
		return kSignInBackoffMs[qBound(0, failures - 1, last)];
	}

	RenewalStep renewalStep(const QDateTime& expiry, const QDateTime& now,
							bool accountActive, bool queued, bool asked,
							bool timedOut)
	{
		if (!expiry.isValid() ||
			expiry > now.addSecs(kTokenMarginSeconds) || timedOut) {
			return RenewalStep::Proceed;
		}
		if (accountActive) {
			return RenewalStep::Wait;
		}
		if (!asked) {
			return RenewalStep::AskForRenewal;
		}
		/* Asked, and the account is not refreshing: it still waits its turn
		 * (the list is idle between two refreshes, so "nothing runs" does not
		 * say it ran), or it ran and this is what it left. */
		return queued ? RenewalStep::Wait : RenewalStep::Proceed;
	}

	bool accountChangeRetriesSignIn(int failures, const QString& failedIdentity,
									const QString& identity,
									const QString& refusedProfile,
									const QString& profile)
	{
		if (failures == 0) {
			return true;
		}
		if (!refusedProfile.isEmpty() && refusedProfile == profile) {
			return false;
		}
		return failedIdentity != identity;
	}

	QByteArray challengeBody(const QString& name)
	{
		return compact({{"name", name}});
	}

	QByteArray loginBody(const QString& challengeId)
	{
		return compact({{"challengeId", challengeId}});
	}

	QByteArray mojangJoinBody(const QString& accessToken,
							  const QString& profileId,
							  const QString& serverId)
	{
		return compact({{"accessToken", accessToken},
						{"selectedProfile", undashed(profileId)},
						{"serverId", serverId}});
	}

	QByteArray requestBody(const QString& name)
	{
		return compact({{"name", name}});
	}

	QByteArray statusBody(const Presence& presence)
	{
		QJsonObject body;
		if (!presence.playing) {
			body.insert("state", "online");
			return compact(body);
		}
		body.insert("state", "playing");
		const QString instance = presence.instance.trimmed();
		if (!instance.isEmpty()) {
			body.insert("instance", instance.left(kMaxInstanceLength));
		}
		QString version = presence.version;
		version.remove(QRegularExpression(kBadVersionChars));
		version = version.trimmed().left(kMaxVersionLength);
		if (!version.isEmpty()) {
			body.insert("version", version);
		}
		body.insert("world", worldJson(presence.world));
		return compact(body);
	}

	QByteArray settingsBody(const QVariantMap& partial)
	{
		static const QStringList kVisibilities = {"friends", "everyone",
												  "invisible"};
		QJsonObject body;
		const QString visibility = partial.value("visibility").toString();
		if (kVisibilities.contains(visibility)) {
			body.insert("visibility", visibility);
		}
		for (const char* key : {"hideServer", "shareIpServers", "allowJoin"}) {
			if (partial.value(key).typeId() == QMetaType::Bool) {
				body.insert(key, partial.value(key).toBool());
			}
		}
		return compact(body);
	}

	std::optional<Challenge> parseChallenge(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		Challenge challenge;
		challenge.challengeId = object.value("challengeId").toString();
		challenge.serverId = object.value("serverId").toString();
		if (!ok || challenge.challengeId.isEmpty() ||
			challenge.serverId.isEmpty()) {
			return std::nullopt;
		}
		return challenge;
	}

	std::optional<Session> parseSession(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		const auto user = object.value("user").toObject();
		Session session;
		session.token = object.value("token").toString();
		session.expiresAt = parseTime(object.value("expiresAt"));
		session.uuid = user.value("uuid").toString();
		session.name = user.value("name").toString();
		if (!ok || session.token.isEmpty() || !session.expiresAt.isValid() ||
			session.uuid.isEmpty() || session.name.isEmpty()) {
			return std::nullopt;
		}
		return session;
	}

	std::optional<Settings> parseMe(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		if (!ok || !object.value("settings").isObject()) {
			return std::nullopt;
		}
		return settingsOf(object.value("settings").toObject());
	}

	std::optional<Settings> parseSettings(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		if (!ok || !object.contains("visibility")) {
			return std::nullopt;
		}
		return settingsOf(object);
	}

	Status parseStatus(const QJsonObject& object)
	{
		Status status;
		const auto state = object.value("state").toString();
		if (state == "online") {
			status.state = Status::State::Online;
		} else if (state == "playing") {
			status.state = Status::State::Playing;
			status.instance = object.value("instance").toString();
			status.version = object.value("version").toString();
			status.world = parseWorld(object.value("world"));
			/* An older server does not send it: not the same as "no". */
			status.joinableSaid = object.value("joinable").isBool();
			status.joinable = object.value("joinable").toBool();
		} else {
			status.lastSeenAt = parseTime(object.value("lastSeenAt"));
		}
		return status;
	}

	std::optional<Status> parseStatusReply(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		if (!ok || !object.value("status").isObject()) {
			return std::nullopt;
		}
		return parseStatus(object.value("status").toObject());
	}

	std::optional<FriendsSnapshot> parseFriends(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		if (!ok || !object.value("friends").isArray() ||
			!object.value("requests").isObject()) {
			return std::nullopt;
		}
		FriendsSnapshot snapshot;
		for (const auto& entry : object.value("friends").toArray()) {
			const auto friendObject = entry.toObject();
			Friend one;
			one.uuid = friendObject.value("uuid").toString();
			one.name = friendObject.value("name").toString();
			one.since = parseTime(friendObject.value("since"));
			one.status =
				parseStatus(friendObject.value("status").toObject());
			if (!one.uuid.isEmpty() && !one.name.isEmpty()) {
				snapshot.friends.append(one);
			}
		}
		bool peopleOk = true;
		const auto requests = object.value("requests").toObject();
		snapshot.incoming = parsePeople(requests.value("incoming"), &peopleOk);
		snapshot.outgoing = parsePeople(requests.value("outgoing"), &peopleOk);
		if (!peopleOk) {
			return std::nullopt;
		}
		return snapshot;
	}

	std::optional<QList<Person>> parseBlocked(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		bool peopleOk = true;
		auto people = parsePeople(object.value("blocked"), &peopleOk);
		if (!ok || !peopleOk) {
			return std::nullopt;
		}
		return people;
	}

	std::optional<RequestResult> parseRequestResult(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		const auto user = object.value("user").toObject();
		RequestResult result;
		result.status = object.value("status").toString();
		result.uuid = user.value("uuid").toString();
		result.name = user.value("name").toString();
		if (!ok || result.status.isEmpty() || result.name.isEmpty()) {
			return std::nullopt;
		}
		return result;
	}

	std::optional<Person> parseUser(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		auto person = parsePerson(object.value("user").toObject());
		if (!ok || !hasIdentity(person)) {
			return std::nullopt;
		}
		return person;
	}

	Error parseError(const QByteArray& body)
	{
		const auto object = objectOf(body);
		Error error;
		error.code = object.value("error").toString();
		error.message = object.value("message").toString();
		if (error.code.isEmpty()) {
			error.code = QStringLiteral("unknown");
		}
		return error;
	}

	QString parseMojangError(const QByteArray& body)
	{
		return objectOf(body).value("error").toString();
	}
} // namespace FriendsApi
