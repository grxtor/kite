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

#include "FriendsEvents.h"

#include <algorithm>

namespace FriendsEvents
{
	namespace
	{
		bool same(const QString& a, const QString& b)
		{
			return a.compare(b, Qt::CaseInsensitive) == 0;
		}

		template <typename List>
		bool removeUuid(List& list, const QString& uuid)
		{
			const auto before = list.size();
			list.removeIf([&uuid](const auto& one) { return same(one.uuid, uuid); });
			return list.size() != before;
		}

		FriendsApi::Person personOf(const QJsonObject& frame)
		{
			const auto user = frame.value("user").toObject();
			FriendsApi::Person person;
			person.uuid = user.value("uuid").toString();
			person.name = user.value("name").toString();
			person.createdAt = QDateTime::fromString(frame.value("createdAt").toString(),
													 Qt::ISODateWithMs);
			return person;
		}

		FriendsApi::Friend* friendOf(FriendsApi::FriendsSnapshot& snapshot, const QString& uuid)
		{
			for (auto& one : snapshot.friends) {
				if (same(one.uuid, uuid)) {
					return &one;
				}
			}
			return nullptr;
		}

		Outcome status(FriendsApi::FriendsSnapshot& snapshot, const QJsonObject& frame)
		{
			auto* one = friendOf(snapshot, frame.value("uuid").toString());
			if (!one) {
				return {false, true};
			}
			one->status = FriendsApi::parseStatus(frame.value("status").toObject());
			return {true, false};
		}

		Outcome request(FriendsApi::FriendsSnapshot& snapshot, const QJsonObject& frame)
		{
			const auto person = personOf(frame);
			if (person.uuid.isEmpty() || !FriendsApi::isMinecraftName(person.name)) {
				return {false, true};
			}
			const bool known = std::any_of(snapshot.incoming.cbegin(), snapshot.incoming.cend(),
										   [&person](const auto& one) { return same(one.uuid, person.uuid); });
			if (known) {
				return {};
			}
			snapshot.incoming.append(person);
			return {true, false};
		}
	} // namespace

	Kind kindOf(const QJsonObject& frame)
	{
		const QString type = frame.value("type").toString();
		if (type.startsWith(QLatin1String("friend."))) {
			return Kind::Friend;
		}
		if (type.startsWith(QLatin1String("chat.")) || type == QLatin1String("error")) {
			return Kind::Chat;
		}
		if (type == QLatin1String("invite") || type == QLatin1String("invite.cancelled")) {
			return Kind::Invite;
		}
		return Kind::Other;
	}

	bool forInvites(const QJsonObject& frame)
	{
		return kindOf(frame) == Kind::Invite ||
			   frame.value("type").toString() == QLatin1String("friend.removed");
	}

	Outcome apply(FriendsApi::FriendsSnapshot& snapshot, const QJsonObject& frame)
	{
		const QString type = frame.value("type").toString();
		if (type == QLatin1String("friend.status")) {
			return status(snapshot, frame);
		}
		if (type == QLatin1String("friend.request")) {
			return request(snapshot, frame);
		}
		if (type == QLatin1String("friend.accepted")) {
			/* They are a friend now; their status comes with the next read. */
			const bool changed = removeUuid(snapshot.outgoing, personOf(frame).uuid);
			return {changed, true};
		}
		if (type == QLatin1String("friend.removed")) {
			const QString uuid = frame.value("uuid").toString();
			bool changed = removeUuid(snapshot.friends, uuid);
			changed = removeUuid(snapshot.incoming, uuid) || changed;
			changed = removeUuid(snapshot.outgoing, uuid) || changed;
			/* Their invites go too, which the next read says. */
			return {changed, true};
		}
		return {};
	}

	bool setUnread(FriendsApi::FriendsSnapshot& snapshot, const QString& uuid, int count)
	{
		auto* one = friendOf(snapshot, uuid);
		if (!one || one->unread == count) {
			return false;
		}
		one->unread = count;
		return true;
	}

	bool addUnread(FriendsApi::FriendsSnapshot& snapshot, const QString& uuid)
	{
		auto* one = friendOf(snapshot, uuid);
		if (!one) {
			return false;
		}
		++one->unread;
		return true;
	}
} // namespace FriendsEvents
