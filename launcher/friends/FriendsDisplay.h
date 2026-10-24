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
	/// "Just now", "5 min ago", "3 h ago", "Yesterday", "12 days ago", or a
	/// date from a month on.
	QString relativeTime(const QDateTime& then, const QDateTime& now);
	/// "On mc.hypixel.net", "On a server", "Single-player", "Playing",
	/// "Online", "Last seen 3 h ago" or "Offline".
	QString statusText(const FriendsApi::Status& status, const QDateTime& now);

	/// uuid, name, since, state ("offline" | "online" | "playing"),
	/// lastSeenAt (ISO or ""), instance, version, worldType ("server" |
	/// "singleplayer" | ""), host, port, statusText.
	QVariantMap friendMap(const FriendsApi::Friend& one, const QDateTime& now);
	QVariantList friendList(const QList<FriendsApi::Friend>& friends,
							const QDateTime& now);
	/// uuid, name, createdAt (ISO).
	QVariantMap personMap(const FriendsApi::Person& person);
	QVariantList personList(const QList<FriendsApi::Person>& people);
	/// visibility, hideServer, shareIpServers.
	QVariantMap settingsMap(const FriendsApi::Settings& settings);
} // namespace FriendsDisplay
