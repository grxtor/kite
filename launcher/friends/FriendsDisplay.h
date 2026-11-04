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

#include <QDateTime>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

#include "FriendsApi.h"

/*
 * What the QML side is given: the structs of FriendsApi as variant maps,
 * with the status line already worded in the selected language.
 */
namespace FriendsDisplay
{
	/// "Last seen just now", "Last seen 5 min ago", "Last seen 3 h ago",
	/// "Last seen yesterday", "Last seen 12 days ago", or "Last seen" and a
	/// date from a month on.
	QString lastSeenText(const QDateTime& then, const QDateTime& now);
	/// "On mc.hypixel.net", "On a server", "Singleplayer", "Playing",
	/// "In Kite" (the launcher is open), lastSeenText() or "Offline".
	QString statusText(const FriendsApi::Status& status, const QDateTime& now);

	/// uuid, name, since, state ("offline" | "online" | "playing"),
	/// lastSeenAt (ISO or ""), instance, version, worldType ("server" |
	/// "singleplayer" | ""), host, port, joinable, joinOff (a visible
	/// server whose player does not let friends join: the server said so,
	/// silence is not that), statusText, unread (chat messages not read yet).
	QVariantMap friendMap(const FriendsApi::Friend& one, const QDateTime& now);
	QVariantList friendList(const QList<FriendsApi::Friend>& friends,
							const QDateTime& now);
	/// uuid, name, createdAt (ISO).
	QVariantMap personMap(const FriendsApi::Person& person);
	QVariantList personList(const QList<FriendsApi::Person>& people);
	/// What friends can see of you right now, from what the launcher reports
	/// (@p presence) and your settings (settingsMap()): playing, worldType
	/// ("server" | "singleplayer" | ""), host (empty when hidden or not on a
	/// server), serverHidden (on a server that is kept from them) and
	/// joinable (they get a Join button).
	QVariantMap selfStatusMap(const FriendsApi::Presence& presence,
							  const QVariantMap& settings);
	/// From the server's answer @p view to the status report @p sent: the truth
	/// about what friends see (it hides more than selfStatusMap() can tell, and
	/// decides joinable).
	QVariantMap selfStatusFromView(const FriendsApi::Presence& sent,
								   const FriendsApi::Status& view);
	QVariantMap settingsMap(const FriendsApi::Settings& settings);
	/// Whether every setting in a request body is as asked in the settings the
	/// server answered with; a server that does not know a setting drops it and
	/// answers with its default.
	bool settingsSaved(const QByteArray& body, const QVariantMap& saved);
} // namespace FriendsDisplay
