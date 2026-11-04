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

/* The requests the launcher sends to the friends server (server/friends/README.md)
 * and the answers it reads, as plain functions over plain structs; no network
 * or Qt objects. */
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
		/// Playing only: the friend allows joining and the host is visible; false when absent.
		bool joinable = false;
		/// False for an older server that omits `joinable`: its false is not "turned off".
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
		/// Friends may invite this player to a server.
		bool allowInvites = true;
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
		/// Chat messages from them after our read marker.
		int unread = 0;
	};

	/// An invite to a server (README "Invites"); uuid and name are the other player.
	struct Invite {
		QString uuid;
		QString name;
		/// Normalized by the server: lower case, no trailing dot.
		QString host;
		/// 25565 when the server leaves it out.
		int port = 0;
		QDateTime createdAt;
		QDateTime expiresAt;
	};

	struct InviteLists {
		QList<Invite> incoming;
		QList<Invite> outgoing;
	};

	struct FriendsSnapshot {
		QList<Friend> friends;
		QList<Person> incoming;
		QList<Person> outgoing;
		/// Empty from a server without invites.
		InviteLists invites;
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
		/// A failure already worded for the user (the sign-in behind the request failed).
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
		/// An invite to a raw IP or local address while shareIpServers is off.
		extern const QString AddressNotShared;
		/// An invite to a friend whose allowInvites is off.
		extern const QString InvitesOff;
	} // namespace Code

	/// The "error" names of Mojang's session server the launcher tells apart.
	namespace MojangError
	{
		/// The token itself was refused: signing in again is the fix.
		extern const QString Forbidden;
		/// Multiplayer is off for the Microsoft account; signing in again changes nothing.
		extern const QString InsufficientPrivileges;
		extern const QString UserBanned;
	} // namespace MojangError

	// @p base is the server URL with or without a trailing slash.
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
		QString invites();
		/// PUT to send, DELETE to cancel your invite to @p uuid.
		QString invite(const QString& uuid);
		QString dismissInvite(const QString& uuid);
	} // namespace Path

	/// Dashed UUIDs are the only form that goes into a path.
	bool isUuid(const QString& uuid);
	/// A Minecraft name: 1 to 16 of A-Z a-z 0-9 _.
	bool isMinecraftName(const QString& name);
	/// "069a79f444e94726a5befca90e38aaf5" for either spelling of a UUID.
	QString undashed(const QString& uuid);
	/// Canonical host from the game log (lower case, no brackets or trailing dot),
	/// or empty if it is not a DNS name or IP. Also refused: names whose last label
	/// is a number (the server reads 2130706433 as an IP) and IPv6 with a zone id
	/// (fe80::1%en0). The server rejects a whole status report for a bad host.
	QString normalizeHost(const QString& raw);
	/// Whether @p host (as normalizeHost() gives it) can only mean the joiner's own
	/// machine; never one a friend can be followed to.
	bool isLoopbackHost(const QString& host);

	/// Wait before the next try after @p failures failed sign-ins in a row: the
	/// first retry is the next poll tick, then less often.
	int signInBackoffMs(int failures);

	enum class RenewalStep {
		Proceed,
		/// About to expire, nothing renews it yet: ask the account list to, then decide again.
		AskForRenewal,
		Wait
	};
	/// Whether a sign-in must wait for the access token to be renewed. @p expiry:
	/// when it ends (invalid: not known stale); @p accountActive: the default
	/// account's refresh is under way; @p queued: it waits in the account list's
	/// queue; @p asked: this sign-in already asked for a renewal; @p timedOut: it
	/// has waited long enough.
	RenewalStep renewalStep(const QDateTime& expiry, const QDateTime& now,
							bool accountActive, bool queued, bool asked,
							bool timedOut);

	/// Whether an account-list change warrants signing in again at once after
	/// @p failures failures: only when it changed what failed (@p failedIdentity
	/// vs @p identity; a game starting or another account's refresh changes
	/// neither). An account Mojang refused for good (@p refusedProfile) stays
	/// refused until @p profile is another one.
	bool accountChangeRetriesSignIn(int failures, const QString& failedIdentity,
									const QString& identity,
									const QString& refusedProfile,
									const QString& profile);

	QByteArray challengeBody(const QString& name);
	QByteArray loginBody(const QString& challengeId);
	QByteArray mojangJoinBody(const QString& accessToken,
							  const QString& profileId,
							  const QString& serverId);
	QByteArray requestBody(const QString& name);
	/// PUT /v1/me/status. A bad host, version or over-long instance name is
	/// repaired or left out: the server rejects the whole report for it.
	QByteArray statusBody(const Presence& presence);
	/// PATCH /v1/me/settings with the known keys of @p partial.
	QByteArray settingsBody(const QVariantMap& partial);
	/// PUT /v1/invites/{uuid}: the host as normalizeHost() gives it (empty if
	/// it is not one), the port only when it is a valid one.
	QByteArray inviteBody(const QString& host, int port);

	// Answers: empty (or false) for a body that is not one.
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
	Status parseStatus(const QJsonObject& object);
} // namespace FriendsApi
