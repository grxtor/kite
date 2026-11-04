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

#include <QJsonObject>
#include <QString>

#include "FriendsApi.h"

/* What the socket's friend.* events do to the lists (server/friends/README.md
 * "Events"), as a plain function over FriendsApi's snapshot. */
namespace FriendsEvents
{
	enum class Kind { Friend, Chat, Invite, Other };

	struct Outcome {
		/// The snapshot changed and is to be shown.
		bool changed = false;
		/// Only a full read tells the rest (a new friend's status, a friend we did not know).
		bool poll = false;
	};

	/// Which part of the launcher a frame's `type` is for.
	Kind kindOf(const QJsonObject& frame);

	/// friend.status, friend.request, friend.accepted and friend.removed.
	Outcome apply(FriendsApi::FriendsSnapshot& snapshot, const QJsonObject& frame);

	/// @p uuid's unread count set to @p count; false if not a friend or unchanged.
	bool setUnread(FriendsApi::FriendsSnapshot& snapshot, const QString& uuid, int count);
	/// One more unread message from @p uuid; false if not a friend.
	bool addUnread(FriendsApi::FriendsSnapshot& snapshot, const QString& uuid);
} // namespace FriendsEvents
