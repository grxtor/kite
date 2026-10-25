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

#include "ModBridgeJson.h"

#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QVariantMap>

#include "ModBridgeProtocol.h"
#include "minecraft/launch/MinecraftServerTarget.h"

namespace ModBridgeJson
{
	namespace
	{
		constexpr int kMaxPort = 65535;
		constexpr int kUndashedUuidLength = 32;
		const QStringList kLanSuffixes = {
			QStringLiteral(".local"), QStringLiteral(".lan"),
			QStringLiteral(".home.arpa"), QStringLiteral(".internal")};

		QByteArray compact(const QJsonObject& object)
		{
			return QJsonDocument(object).toJson(QJsonDocument::Compact);
		}

		int portOrDefault(int port)
		{
			return port >= 1 && port <= kMaxPort
					   ? port
					   : ModBridgeProtocol::kDefaultServerPort;
		}

		QString friendState(const QString& state)
		{
			return state == QLatin1String("playing") ||
						   state == QLatin1String("online")
					   ? state
					   : QStringLiteral("offline");
		}

		QJsonObject worldOf(const QVariantMap& one)
		{
			const QString type = one.value("worldType").toString();
			const QString host = one.value("host").toString();
			QJsonObject world;
			if (type == QLatin1String("server")) {
				world["type"] = QStringLiteral("server");
				if (!host.isEmpty()) {
					world["host"] = host;
					world["port"] = portOrDefault(one.value("port").toInt());
				}
			} else if (type == QLatin1String("singleplayer")) {
				world["type"] = QStringLiteral("singleplayer");
			} else {
				world["type"] = QStringLiteral("none");
			}
			return world;
		}

		QJsonObject friendOf(const QVariantMap& one)
		{
			QJsonObject out;
			out["uuid"] = one.value("uuid").toString();
			out["name"] = one.value("name").toString();
			out["state"] = friendState(one.value("state").toString());
			out["statusText"] = one.value("statusText").toString();
			out["version"] = one.value("version").toString();
			out["world"] = worldOf(one);
			out["joinable"] = one.value("joinable").toBool();
			const QString lastSeen = one.value("lastSeenAt").toString();
			if (!lastSeen.isEmpty()) {
				out["lastSeenAt"] = lastSeen;
			}
			return out;
		}

		/* "6903...": 32 hex digits are the same uuid without dashes. */
		std::optional<QString> dashed(const QString& text)
		{
			const QString lower = text.trimmed().toLower();
			if (FriendsApi::isUuid(lower)) {
				return lower;
			}
			static const QRegularExpression hex(
				QStringLiteral("^[0-9a-f]{%1}$").arg(kUndashedUuidLength));
			if (!hex.match(lower).hasMatch()) {
				return std::nullopt;
			}
			return lower.left(8) + '-' + lower.mid(8, 4) + '-' +
				   lower.mid(12, 4) + '-' + lower.mid(16, 4) + '-' +
				   lower.mid(20);
		}
	} // namespace

	QString friendsState(const QString& controllerState, bool needsSignIn)
	{
		if (controllerState == QLatin1String("noAccount")) {
			return QStringLiteral("signedOut");
		}
		if (controllerState == QLatin1String("signingIn")) {
			return QStringLiteral("connecting");
		}
		if (controllerState == QLatin1String("ready")) {
			return QStringLiteral("ready");
		}
		if (controllerState == QLatin1String("error")) {
			return needsSignIn ? QStringLiteral("signedOut")
							   : QStringLiteral("error");
		}
		return QStringLiteral("off");
	}

	QByteArray helloBody(const QString& launcher, const InstanceInfo& instance,
						 bool friendsEnabled, const QString& friendsState)
	{
		QJsonObject game;
		game["id"] = instance.id;
		game["name"] = instance.name;
		game["version"] = instance.version;
		QJsonObject friends;
		friends["enabled"] = friendsEnabled;
		friends["state"] = friendsState;
		QJsonObject out;
		out["bridge"] = ModBridgeProtocol::kVersion;
		out["launcher"] = launcher;
		out["instance"] = game;
		out["friends"] = friends;
		return compact(out);
	}

	QByteArray friendsBody(const FriendsView& view)
	{
		QJsonArray list;
		for (const QVariant& one : view.friends) {
			list.append(friendOf(one.toMap()));
		}
		QJsonObject me;
		me["name"] = view.meName;
		me["uuid"] = view.meUuid;
		QJsonObject out;
		out["rev"] = view.rev;
		out["enabled"] = view.enabled;
		out["state"] = view.state;
		out["me"] = me;
		out["incomingCount"] = view.incomingCount;
		out["updatedAtMs"] = view.updatedAtMs;
		out["friends"] = list;
		return compact(out);
	}

	std::optional<QString> parseJoinUuid(const QByteArray& body)
	{
		const QJsonDocument doc = QJsonDocument::fromJson(body);
		if (!doc.isObject() || !doc.object().value("uuid").isString()) {
			return std::nullopt;
		}
		return dashed(doc.object().value("uuid").toString());
	}

	JoinAnswer checkJoin(const FriendsView& view, const QString& uuid,
						 const QString& gameVersion)
	{
		JoinAnswer answer;
		if (!view.enabled || view.state == QLatin1String("off")) {
			answer.reason = QStringLiteral("friends_off");
			return answer;
		}
		const QString wanted = FriendsApi::undashed(uuid);
		for (const QVariant& entry : view.friends) {
			const QVariantMap one = entry.toMap();
			if (FriendsApi::undashed(one.value("uuid").toString()) != wanted) {
				continue;
			}
			if (friendState(one.value("state").toString()) ==
				QLatin1String("offline")) {
				answer.reason = QStringLiteral("offline");
			} else if (!one.value("joinable").toBool() ||
					   one.value("host").toString().isEmpty()) {
				answer.reason = QStringLiteral("not_joinable");
			} else {
				answer.ok = true;
				answer.host = one.value("host").toString();
				answer.port = portOrDefault(one.value("port").toInt());
				answer.version = one.value("version").toString();
				answer.versionMatch =
					!answer.version.isEmpty() && answer.version == gameVersion;
			}
			return answer;
		}
		answer.reason = QStringLiteral("unknown_friend");
		return answer;
	}

	QByteArray joinBody(const JoinAnswer& answer)
	{
		QJsonObject out;
		out["ok"] = answer.ok;
		if (answer.ok) {
			out["host"] = answer.host;
			out["port"] = answer.port;
			out["version"] = answer.version;
			out["versionMatch"] = answer.versionMatch;
		} else {
			out["reason"] = answer.reason;
		}
		return compact(out);
	}

	bool isLanHost(const QString& host)
	{
		QHostAddress address;
		if (address.setAddress(host)) {
			return address.isPrivateUse() || address.isLinkLocal() ||
				   address.isLoopback();
		}
		if (!host.contains('.')) {
			return true;
		}
		for (const QString& suffix : kLanSuffixes) {
			if (host.endsWith(suffix)) {
				return true;
			}
		}
		return false;
	}

	std::optional<FriendsApi::World> parseWorld(const QByteArray& body)
	{
		using Type = FriendsApi::World::Type;
		const QJsonDocument doc = QJsonDocument::fromJson(body);
		if (!doc.isObject()) {
			return std::nullopt;
		}
		const QJsonObject object = doc.object();
		const QString type = object.value("type").toString();
		FriendsApi::World world;
		if (type == QLatin1String("none")) {
			return world;
		}
		if (type == QLatin1String("singleplayer")) {
			world.type = Type::Singleplayer;
			return world;
		}
		if (type != QLatin1String("server") ||
			!object.value("host").isString()) {
			return std::nullopt;
		}
		/* The port is its own field; one left in the address ("host:25566")
		 * is read the way the game reads it. */
		QString address = object.value("host").toString();
		int port = object.value("port").toInt();
		if (!object.contains("port")) {
			const auto target = MinecraftServerTarget::parse(address, false);
			address = target.address;
			port = target.port;
		}
		const QString host = FriendsApi::normalizeHost(address);
		if (host.isEmpty() || FriendsApi::isLoopbackHost(host) ||
			isLanHost(host)) {
			return FriendsApi::World();
		}
		world.type = Type::Server;
		world.host = host;
		world.port = port >= 1 && port <= kMaxPort ? port : 0;
		return world;
	}
} // namespace ModBridgeJson
