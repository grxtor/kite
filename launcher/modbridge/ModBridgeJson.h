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
#include <QString>
#include <QVariantList>

#include <optional>

#include "friends/FriendsApi.h"

/*
 * The messages of the Kite mod <-> launcher bridge (kite-mod/docs/BRIDGE.md),
 * as plain functions: the answers are built from FriendsController's variant
 * maps with a whitelist of the document's keys, so nothing else those maps
 * (or later versions of them) carry can reach the game, and what the game
 * sends is read as strictly as the friends server reads the launcher.
 */
namespace ModBridgeJson
{
	/// What the bridge serves of the friends data: a snapshot of
	/// FriendsController (friends in FriendsDisplay::friendMap's shape).
	struct FriendsView {
		/// Changes whenever anything served here does.
		qint64 rev = 0;
		bool enabled = false;
		/// "off" | "signedOut" | "connecting" | "ready" | "error"
		QString state = QStringLiteral("off");
		QString meName;
		QString meUuid;
		int incomingCount = 0;
		qint64 updatedAtMs = 0;
		QVariantList friends;
	};

	/// The game a session belongs to.
	struct InstanceInfo {
		QString id;
		QString name;
		QString version;
	};

	/// The answer to /v1/join.
	struct JoinAnswer {
		bool ok = false;
		/// "unknown_friend" | "offline" | "not_joinable" | "friends_off"
		QString reason;
		QString host;
		int port = 0;
		/// The friend's Minecraft version.
		QString version;
		/// It is the one this game runs.
		bool versionMatch = false;
	};

	/// FriendsController's state name ("off", "noAccount", "signingIn",
	/// "ready", "error") as the bridge says it; an error only a new sign-in
	/// fixes (@p needsSignIn) is "signedOut" too.
	QString friendsState(const QString& controllerState, bool needsSignIn);

	QByteArray helloBody(const QString& launcher, const InstanceInfo& instance,
						 bool friendsEnabled, const QString& friendsState);
	QByteArray friendsBody(const FriendsView& view);

	/// The uuid (with dashes) of {"uuid": "..."}; either spelling is read.
	std::optional<QString> parseJoinUuid(const QByteArray& body);
	/// Whether @p uuid is a friend who can be followed, from @p view; the
	/// friend's version is compared with @p gameVersion.
	JoinAnswer checkJoin(const FriendsView& view, const QString& uuid,
						 const QString& gameVersion);
	QByteArray joinBody(const JoinAnswer& answer);

	/// The world of {"type": "server" | "singleplayer" | "none", ...}: a
	/// server's host is normalised as the log parser does, and a LAN,
	/// loopback or unusable one is "none". Nothing for a body that is not one.
	std::optional<FriendsApi::World> parseWorld(const QByteArray& body);
	/// A host only the player's own network can reach: a private or
	/// link-local address, a name without a dot or ending in .local, .lan,
	/// .home.arpa or .internal.
	bool isLanHost(const QString& host);
} // namespace ModBridgeJson
