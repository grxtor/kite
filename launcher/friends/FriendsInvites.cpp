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

#include "FriendsInvites.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>

#include <algorithm>

namespace FriendsInvites
{
	namespace
	{
		const QString kInviteEvent = QStringLiteral("invite");
		const QString kCancelledEvent = QStringLiteral("invite.cancelled");
		const QString kFriendRemovedEvent = QStringLiteral("friend.removed");

		/* The server's lists (src/serverAddress.ts): reserved and home-network
		 * names, and wildcard DNS services whose names are an IP address. */
		const QStringList kLocalSuffixes = {
			"localhost", "localdomain", "local",	  "lan",		 "home",
			"internal",	 "intranet",	"corp",		  "private",	 "home.arpa",
			"test",		 "invalid",		"example",	  "fritz.box",	 "mshome.net",
			"localtest.me", "lvh.me"};
		const QStringList kIpEncodingSuffixes = {"nip.io", "sslip.io", "xip.io",
												 "traefik.me"};
		const QList<QPair<QString, int>> kLocalRanges = {
			{"0.0.0.0", 8},		 {"10.0.0.0", 8},	  {"100.64.0.0", 10},
			{"127.0.0.0", 8},	 {"169.254.0.0", 16}, {"172.16.0.0", 12},
			{"192.168.0.0", 16}, {"::", 128},		  {"::1", 128},
			{"fc00::", 7},		 {"fe80::", 10}};
		/* Four numbers in a row that could be an IPv4 address's parts. */
		constexpr int kIpv4Parts = 4;
		constexpr int kMaxOctet = 255;
		const QRegularExpression kOctet(
			QRegularExpression::anchoredPattern(QStringLiteral("\\d{1,3}")));

		QDateTime parseTime(const QJsonValue& value)
		{
			return QDateTime::fromString(value.toString(), Qt::ISODateWithMs);
		}

		QString iso(const QDateTime& time)
		{
			return time.isValid() ? time.toUTC().toString(Qt::ISODateWithMs)
								  : QString();
		}

		QList<FriendsApi::Invite> parseList(const QJsonValue& value, bool* ok)
		{
			QList<FriendsApi::Invite> list;
			if (!value.isArray()) {
				*ok = false;
				return list;
			}
			for (const QJsonValue& entry : value.toArray()) {
				if (const auto invite = parse(entry.toObject())) {
					list.append(*invite);
				}
			}
			return list;
		}

		bool isLocalAddress(const QHostAddress& address)
		{
			for (const auto& range : kLocalRanges) {
				if (address.isInSubnet(QHostAddress(range.first), range.second)) {
					return true;
				}
			}
			return false;
		}

		bool under(const QString& host, const QString& suffix)
		{
			return host == suffix || host.endsWith(QLatin1Char('.') + suffix);
		}

		bool embedsIpv4(const QString& host)
		{
			static const QRegularExpression separators(QStringLiteral("[.-]"));
			int run = 0;
			for (const QString& part : host.split(separators)) {
				const bool octet =
					kOctet.match(part).hasMatch() && part.toInt() <= kMaxOctet;
				run = octet ? run + 1 : 0;
				if (run == kIpv4Parts) {
					return true;
				}
			}
			return false;
		}
	} // namespace

	std::optional<FriendsApi::Invite> parse(const QJsonObject& object)
	{
		FriendsApi::Invite invite;
		invite.uuid = object.value("uuid").toString();
		invite.name = object.value("name").toString();
		invite.host = object.value("host").toString();
		invite.port = object.value("port").toInt(kDefaultPort);
		invite.createdAt = parseTime(object.value("createdAt"));
		invite.expiresAt = parseTime(object.value("expiresAt"));
		if (!FriendsApi::isUuid(invite.uuid) || invite.name.isEmpty() ||
			FriendsApi::normalizeHost(invite.host).isEmpty() ||
			!invite.expiresAt.isValid()) {
			return std::nullopt;
		}
		return invite;
	}

	std::optional<FriendsApi::InviteLists> parseLists(const QJsonObject& object)
	{
		bool ok = true;
		FriendsApi::InviteLists lists;
		lists.incoming = parseList(object.value("incoming"), &ok);
		lists.outgoing = parseList(object.value("outgoing"), &ok);
		if (!ok) {
			return std::nullopt;
		}
		return lists;
	}

	std::optional<FriendsApi::InviteLists> parseListsBody(const QByteArray& body)
	{
		const QJsonDocument doc = QJsonDocument::fromJson(body);
		return doc.isObject() ? parseLists(doc.object()) : std::nullopt;
	}

	std::optional<FriendsApi::Invite> parseBody(const QByteArray& body)
	{
		const QJsonDocument doc = QJsonDocument::fromJson(body);
		return doc.isObject() ? parse(doc.object()) : std::nullopt;
	}

	QList<FriendsApi::Invite> live(const QList<FriendsApi::Invite>& invites,
								   const QDateTime& now)
	{
		QList<FriendsApi::Invite> out;
		for (const auto& invite : invites) {
			if (invite.expiresAt > now) {
				out.append(invite);
			}
		}
		std::stable_sort(out.begin(), out.end(),
						 [](const auto& a, const auto& b) {
							 return a.createdAt > b.createdAt;
						 });
		return out;
	}

	QDateTime nextExpiry(const QList<FriendsApi::Invite>& invites)
	{
		QDateTime first;
		for (const auto& invite : invites) {
			if (!first.isValid() || invite.expiresAt < first) {
				first = invite.expiresAt;
			}
		}
		return first;
	}

	EventEffect applyEvent(QList<FriendsApi::Invite>& incoming,
						   const QJsonObject& event, const QDateTime& now)
	{
		const QString type = event.value("type").toString();
		if (type == kInviteEvent) {
			const auto invite = parse(event.value("invite").toObject());
			if (!invite || invite->expiresAt <= now) {
				return EventEffect::Ignored;
			}
			/* One invite per friend: a new one replaces the old. */
			incoming.removeIf([&](const auto& one) {
				return one.uuid == invite->uuid;
			});
			incoming.prepend(*invite);
			return EventEffect::Added;
		}
		if (type != kCancelledEvent && type != kFriendRemovedEvent) {
			return EventEffect::Ignored;
		}
		const QString uuid = event.value("uuid").toString();
		const auto removed = incoming.removeIf(
			[&](const auto& one) { return one.uuid == uuid; });
		return removed > 0 ? EventEffect::Removed : EventEffect::Ignored;
	}

	AddressKind addressKind(const QString& host)
	{
		const QHostAddress address(host);
		if (!address.isNull()) {
			bool mapped = false;
			const quint32 v4 = address.toIPv4Address(&mapped);
			const QHostAddress plain = mapped ? QHostAddress(v4) : address;
			return isLocalAddress(plain) ? AddressKind::Local : AddressKind::Ip;
		}
		if (!host.contains(QLatin1Char('.'))) {
			return AddressKind::Local;
		}
		for (const QString& suffix : kLocalSuffixes) {
			if (under(host, suffix)) {
				return AddressKind::Local;
			}
		}
		for (const QString& suffix : kIpEncodingSuffixes) {
			if (under(host, suffix)) {
				return AddressKind::Ip;
			}
		}
		return embedsIpv4(host) ? AddressKind::Ip : AddressKind::Domain;
	}

	QString addressKindName(AddressKind kind)
	{
		switch (kind) {
			case AddressKind::Ip:
				return QStringLiteral("ip");
			case AddressKind::Local:
				return QStringLiteral("local");
			case AddressKind::Domain:
				break;
		}
		return QStringLiteral("domain");
	}

	QVariantMap toMap(const FriendsApi::Invite& invite)
	{
		return {{"uuid", invite.uuid},
				{"name", invite.name},
				{"host", invite.host},
				{"port", invite.port},
				{"createdAt", iso(invite.createdAt)},
				{"expiresAt", iso(invite.expiresAt)},
				{"expiresAtMs", invite.expiresAt.toMSecsSinceEpoch()},
				{"addressKind",
				 addressKindName(addressKind(invite.host))}};
	}

	QVariantList toList(const QList<FriendsApi::Invite>& invites)
	{
		QVariantList list;
		for (const auto& invite : invites) {
			list.append(toMap(invite));
		}
		return list;
	}
} // namespace FriendsInvites
