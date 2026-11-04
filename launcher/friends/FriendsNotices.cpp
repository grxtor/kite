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

#include "FriendsNotices.h"

namespace
{
	FriendsNotices::Notice inviteNotice(const FriendsApi::Invite& invite)
	{
		FriendsNotices::Notice notice;
		notice.kind = FriendsNotices::Notice::Kind::Invite;
		notice.uuid = invite.uuid;
		notice.name = invite.name;
		notice.host = invite.host;
		notice.port = invite.port;
		return notice;
	}
} // namespace

FriendsNotices::Diff FriendsNotices::diff(
	const Memory& previous, const FriendsApi::FriendsSnapshot& snapshot,
	const QDateTime& now)
{
	using State = FriendsApi::Status::State;
	Diff out;
	out.memory.primed = true;

	for (const FriendsApi::Friend& one : snapshot.friends) {
		const bool online = one.status.state != State::Offline;
		const QDateTime lastOnline = previous.lastOnline.value(one.uuid);
		out.memory.online.insert(one.uuid, online);
		if (online) {
			out.memory.lastOnline.insert(one.uuid, now);
		} else if (lastOnline.isValid()) {
			out.memory.lastOnline.insert(one.uuid, lastOnline);
		}

		/* Only a friend the last snapshot had as offline: a request just
		 * accepted is not someone coming online. */
		const bool cameOnline = previous.primed && online &&
								previous.online.contains(one.uuid) &&
								!previous.online.value(one.uuid);
		const bool quiet = !lastOnline.isValid() ||
						   lastOnline.secsTo(now) >= kQuietSeconds;
		if (cameOnline && quiet) {
			out.notices.append(
				{Notice::Kind::Online, one.uuid, one.name, one.status});
		}
	}

	out.memory.requests = previous.requests;
	for (const FriendsApi::Person& person : snapshot.incoming) {
		if (previous.primed && !previous.requests.contains(person.uuid)) {
			out.notices.append(
				{Notice::Kind::Request, person.uuid, person.name, {}});
		}
		out.memory.requests.insert(person.uuid);
	}

	/* Invites run out within minutes; only the live ones are remembered. */
	for (const FriendsApi::Invite& one : snapshot.invites.incoming) {
		const QString key = inviteKey(one);
		if (previous.primed && !previous.invites.contains(key)) {
			out.notices.append(inviteNotice(one));
		}
		out.memory.invites.insert(key);
	}
	return out;
}

QString FriendsNotices::inviteKey(const FriendsApi::Invite& invite)
{
	return invite.uuid + QLatin1Char('|') +
		   invite.createdAt.toUTC().toString(Qt::ISODateWithMs);
}

std::optional<FriendsNotices::Notice> FriendsNotices::invite(
	Memory& memory, const FriendsApi::Invite& one)
{
	const QString key = inviteKey(one);
	const bool seen = memory.invites.contains(key);
	memory.invites.insert(key);
	if (!memory.primed || seen) {
		return std::nullopt;
	}
	return inviteNotice(one);
}
