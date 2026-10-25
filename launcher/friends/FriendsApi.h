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

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>
#include <QVariantMap>

#include <optional>

/*
 * Kite's friends server (server/friends/README.md): the requests the
 * launcher sends and the answers it reads, as plain functions over plain
 * structs. No network and no Qt object lives here, so every shape in the
 * README is tested against fixture JSON.
 */
namespace FriendsApi
{
	/// Where a player is, as the game log tells it.
	struct World {
		enum class Type { None, Server, Singleplayer };
		Type type = Type::None;
		/// Server only; empty for a raw IP the server keeps to itself.
		QString host;
		/// Server only; 0 when unknown.
		int port = 0;

		bool operator==(const World& other) const
		{
			return type == other.type && host == other.host &&
				   port == other.port;
		}
	};

	/// What the launcher reports about itself.
	struct Presence {
		bool playing = false;
		QString instance;
		QString version;
		World world;

		bool operator==(const Presence& other) const
		{
			return playing == other.playing && instance == other.instance &&
				   version == other.version && world == other.world;
		}
	};

	/// What the server shows about a player (the "status objects").
	struct Status {
		enum class State { Offline, Online, Playing };
		State state = State::Offline;
		/// Offline only; invalid when never seen or invisible.
		QDateTime lastSeenAt;
		QString instance;
		QString version;
		World world;
		/// Playing only: the friend lets others join the server they are
		/// on, and the server's host is visible (the server decides; false
		/// when the field is absent).
		bool joinable = false;
		/// The server sent `joinable` at all. One that does not (an older
		/// one) says nothing about whether the friend allows joining, so
		/// "false" there is not "turned off".
		bool joinableSaid = false;
	};

	struct Challenge {
		QString challengeId;
		QString serverId;
	};

	struct Session {
		QString token;
		QDateTime expiresAt;
		QString uuid;
		QString name;
	};

	struct Settings {
		QString visibility = QStringLiteral("friends");
		bool hideServer = false;
		bool shareIpServers = false;
		/// Friends may join the server this player is on.
		bool allowJoin = true;
	};

	struct Person {
		QString uuid;
		QString name;
		QDateTime createdAt;
	};

	struct Friend {
		QString uuid;
		QString name;
		QDateTime since;
		Status status;
	};

	struct FriendsSnapshot {
		QList<Friend> friends;
		QList<Person> incoming;
		QList<Person> outgoing;
	};

	/// The answer to POST /v1/friends/requests.
	struct RequestResult {
		/// "pending" or "accepted".
		QString status;
		QString uuid;
		QString name;
	};

	/// A failure body: {"error": code, "message": text}.
	struct Error {
		QString code;
		QString message;
	};

	/// A raw answer: status 0 means the request never got one.
	struct Reply {
		int status = 0;
		QByteArray body;
		/// Retry-After in seconds, 0 when the header is absent.
		int retryAfterSeconds = 0;
		/// A failure that is already worded for the user (the sign-in
		/// behind the request failed), instead of an answer to read.
		QString failureText;

		bool networkFailed() const
		{
			return status == 0;
		}
		bool ok() const
		{
			return status >= 200 && status < 300;
		}
	};

	/// Error codes the launcher reacts to.
	namespace Code
	{
		extern const QString Unauthorized;
		extern const QString NotJoined;
		extern const QString NotFound;
		extern const QString BadRequest;
		extern const QString AlreadyFriends;
		extern const QString BlockedByYou;
		extern const QString LimitReached;
		extern const QString RateLimited;
	} // namespace Code

	/// The "error" names of Mojang's session server the launcher tells apart.
	namespace MojangError
	{
		/// The token itself was refused: signing in again is the fix.
		extern const QString Forbidden;
		/// Multiplayer is off for the Microsoft account (its privacy
		/// settings, a child account); signing in again changes nothing.
		extern const QString InsufficientPrivileges;
		extern const QString UserBanned;
	} // namespace MojangError

	// ---- URLs. @p base is the server URL with or without a trailing slash.
	QUrl url(const QString& base, const QString& path);
	/// A Mojang session server URL: @p base + /session/minecraft/join.
	QUrl mojangJoinUrl(const QString& base);
	namespace Path
	{
		QString challenge();
		QString login();
		QString logout();
		QString me();
		QString settings();
		QString status();
		QString friends();
		QString requests();
		QString blocks();
		QString accept(const QString& uuid);
		QString decline(const QString& uuid);
		QString cancel(const QString& uuid);
		QString removeFriend(const QString& uuid);
		QString block(const QString& uuid);
	} // namespace Path

	/// Whether @p uuid is a UUID with dashes, the only form that goes into a
	/// path.
	bool isUuid(const QString& uuid);
	/// A Minecraft name: 1 to 16 of A-Z a-z 0-9 _.
	bool isMinecraftName(const QString& name);
	/// "069a79f444e94726a5befca90e38aaf5" for either spelling of a UUID.
	QString undashed(const QString& uuid);
	/// The canonical form of a host from the game log (lower case, no
	/// brackets or trailing dot), or an empty string when it is not a DNS
	/// name or an IP address. The server refuses a whole status report for a
	/// bad host, and reads a name whose last label is a number
	/// (2130706433, 0x7f.0x1) as an IP address, so those are refused here
	/// too. An IPv6 address with a zone id (fe80::1%en0) is refused as well:
	/// it only means something on the machine that wrote it.
	QString normalizeHost(const QString& raw);
	/// Whether @p host (as normalizeHost() gives it) can only mean the
	/// joiner's own machine: localhost, 127.x.x.x, ::1 or the unspecified
	/// address. Such a server is never one a friend can be followed to.
	bool isLoopbackHost(const QString& host);

	/// How long a sign-in that has failed @p failures times in a row waits
	/// before the next try: the first retry is the next poll tick, then less
	/// and less often.
	int signInBackoffMs(int failures);

	/// What a sign-in does about the account's access token (see
	/// renewalStep()).
	enum class RenewalStep {
		/// Sign in with the token as it is.
		Proceed,
		/// The token is about to expire and nothing renews it yet: ask the
		/// account list to, then decide again.
		AskForRenewal,
		/// Park the sign-in until the renewal has run.
		Wait
	};
	/// Whether a sign-in has to wait for the launcher to renew the access
	/// token. @p expiry is when it ends (invalid: not known to be stale);
	/// @p accountActive is the default account's own refresh under way,
	/// @p queued that it waits in the account list's queue (behind another
	/// account's refresh, and then for the pause between two refreshes),
	/// @p asked that this sign-in already asked for the renewal and
	/// @p timedOut that it has waited long enough for it.
	RenewalStep renewalStep(const QDateTime& expiry, const QDateTime& now,
							bool accountActive, bool queued, bool asked,
							bool timedOut);

	/// Whether the account list changing is a reason to sign in again at
	/// once after @p failures failed sign-ins: only when it changed what
	/// failed. @p failedIdentity and @p identity describe the default account
	/// (id, profile, token) then and now; a game starting or stopping, or
	/// another account's refresh, changes neither. An account Mojang refused
	/// for good (@p refusedProfile, empty if none) stays refused until its
	/// profile (@p profile) is another one.
	bool accountChangeRetriesSignIn(int failures, const QString& failedIdentity,
									const QString& identity,
									const QString& refusedProfile,
									const QString& profile);

	// ---- Request bodies.
	QByteArray challengeBody(const QString& name);
	QByteArray loginBody(const QString& challengeId);
	/// The body Mojang's session server wants for the join.
	QByteArray mojangJoinBody(const QString& accessToken,
							  const QString& profileId,
							  const QString& serverId);
	QByteArray requestBody(const QString& name);
	/// PUT /v1/me/status: a host the server would refuse, a version outside
	/// its pattern or an over-long instance name is repaired or left out
	/// here, because the server rejects the whole report for it.
	QByteArray statusBody(const Presence& presence);
	/// PATCH /v1/me/settings with the known keys of @p partial.
	QByteArray settingsBody(const QVariantMap& partial);

	// ---- Answers. Each is empty (or false) for a body that is not one.
	std::optional<Challenge> parseChallenge(const QByteArray& body);
	std::optional<Session> parseSession(const QByteArray& body);
	std::optional<Settings> parseMe(const QByteArray& body);
	std::optional<Settings> parseSettings(const QByteArray& body);
	std::optional<Status> parseStatusReply(const QByteArray& body);
	std::optional<FriendsSnapshot> parseFriends(const QByteArray& body);
	std::optional<QList<Person>> parseBlocked(const QByteArray& body);
	std::optional<RequestResult> parseRequestResult(const QByteArray& body);
	/// The {"user": {"uuid", "name"}} of an accept.
	std::optional<Person> parseUser(const QByteArray& body);
	/// Never empty: a body without the two fields comes back as an
	/// unknown error.
	Error parseError(const QByteArray& body);
	/// The "error" name in an answer of Mojang's session server (see
	/// MojangError), or an empty string for a body without one.
	QString parseMojangError(const QByteArray& body);
	/// The status object of one friend or of PUT /v1/me/status.
	Status parseStatus(const QJsonObject& object);
} // namespace FriendsApi
