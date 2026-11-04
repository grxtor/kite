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
#include <QVariantList>
#include <QVariantMap>

#include <optional>

#include "FriendsApi.h"

/*
 * Invites to a server (server/friends/README.md, "Invites"): reading them from
 * the friends poll, a PUT's answer or a socket event, keeping only the live
 * ones, and telling a public server name from a raw IP or a home-network
 * address the way the server does (src/serverAddress.ts). Plain functions; no
 * network or Qt objects.
 */
namespace FriendsInvites
{
	/// The port the server assumes when an invite names none.
	constexpr int kDefaultPort = 25565;

	/// An entry of `invites.incoming` / `outgoing`, the `invite` of an event or
	/// the answer to a PUT. Empty without uuid, name, host or expiry.
	std::optional<FriendsApi::Invite> parse(const QJsonObject& object);
	/// {"incoming": [...], "outgoing": [...]}; empty when either is not a list.
	std::optional<FriendsApi::InviteLists> parseLists(const QJsonObject& object);
	/// The body of GET /v1/invites.
	std::optional<FriendsApi::InviteLists> parseListsBody(const QByteArray& body);
	/// The body of PUT /v1/invites/{uuid}.
	std::optional<FriendsApi::Invite> parseBody(const QByteArray& body);

	/// Those that have not run out at @p now, newest first.
	QList<FriendsApi::Invite> live(const QList<FriendsApi::Invite>& invites,
								   const QDateTime& now);
	/// When the first of them runs out; invalid for none.
	QDateTime nextExpiry(const QList<FriendsApi::Invite>& invites);

	enum class EventEffect { Ignored, Added, Removed };
	/// What a socket event does to the invites you received: `invite` adds its
	/// invite (replacing an older one from the same friend), `invite.cancelled`
	/// and `friend.removed` drop the invite of that `uuid`. Anything else, an
	/// invite that already ran out at @p now, or an event missing its fields
	/// changes nothing.
	EventEffect applyEvent(QList<FriendsApi::Invite>& incoming,
						   const QJsonObject& event, const QDateTime& now);

	enum class AddressKind {
		/// A public DNS name.
		Domain,
		/// A raw IP address, or a name that only spells one (1.2.3.4.nip.io).
		Ip,
		/// Only reachable on a home network (192.168.1.20, nas.local, localhost).
		Local
	};
	/// For a host as FriendsApi::normalizeHost() gives it.
	AddressKind addressKind(const QString& host);
	/// "domain", "ip" or "local".
	QString addressKindName(AddressKind kind);

	/// For QML: uuid, name, host, port, createdAt, expiresAt (ISO strings),
	/// expiresAtMs and addressKind (addressKindName()). face is added by the
	/// controller.
	QVariantMap toMap(const FriendsApi::Invite& invite);
	QVariantList toList(const QList<FriendsApi::Invite>& invites);
} // namespace FriendsInvites
