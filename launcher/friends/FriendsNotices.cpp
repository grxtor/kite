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
	return out;
}
