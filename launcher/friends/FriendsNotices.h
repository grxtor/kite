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
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>

#include <optional>

#include "FriendsApi.h"

/*
 * Which desktop notifications one friends snapshot calls for, compared with
 * the one before: a friend who was offline is online or playing now, a
 * friend request or an invite to a server nobody had seen yet came in (an
 * invite pushed by the socket counts as seen, see invite()). The first snapshot after
 * signing in only sets the baseline, or everyone already online would be
 * announced at once. A friend who drops out and comes back within
 * kQuietSeconds of last being seen online is not announced again.
 */
namespace FriendsNotices
{
	constexpr qint64 kQuietSeconds = 10 * 60;

	struct Notice {
		enum class Kind { Online, Request, Invite };
		Kind kind = Kind::Online;
		QString uuid;
		QString name;
		/// Online only: what the friend is doing now.
		FriendsApi::Status status;
		/// Invite only: where to.
		QString host;
		int port = 0;
	};

	/// What one snapshot leaves for the next; default-constructed before the
	/// first since signing in.
	struct Memory {
		bool primed = false;
		/// Every friend in the last snapshot: whether online or playing.
		QHash<QString, bool> online;
		/// When each friend was last seen online or playing.
		QHash<QString, QDateTime> lastOnline;
		/// Every incoming request seen since signing in: one notice each.
		QSet<QString> requests;
		/// Every invite seen since signing in, by inviteKey(): a friend's new
		/// invite replaces the old one and is news again.
		QSet<QString> invites;
	};

	struct Diff {
		QList<Notice> notices;
		Memory memory;
	};

	/// The notices @p snapshot, read at @p now, calls for after @p previous,
	/// and what to compare the next snapshot with.
	Diff diff(const Memory& previous,
			  const FriendsApi::FriendsSnapshot& snapshot,
			  const QDateTime& now);

	/// Who sent it and when: what tells two invites apart.
	QString inviteKey(const FriendsApi::Invite& invite);
	/// The notice for one invite pushed by the socket, if @p memory has not seen
	/// it, and @p memory marks it seen either way. Before the first snapshot
	/// (not primed) there is no notice, as for the snapshot's own.
	std::optional<Notice> invite(Memory& memory,
								 const FriendsApi::Invite& invite);
} // namespace FriendsNotices
