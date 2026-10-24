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

#include "FriendsDisplay.h"

#include <QCoreApplication>
#include <QLocale>

namespace FriendsDisplay
{
	namespace
	{
		constexpr qint64 kSecondsPerMinute = 60;
		constexpr qint64 kMinutesPerHour = 60;
		constexpr qint64 kHoursPerDay = 24;
		constexpr qint64 kDaysPerMonth = 30;

		/* The same context as the controller's own texts, so one lupdate
		 * entry per string. */
		QString tr(const char* text)
		{
			return QCoreApplication::translate("FriendsController", text);
		}

		QString iso(const QDateTime& time)
		{
			return time.isValid() ? time.toUTC().toString(Qt::ISODateWithMs)
								  : QString();
		}

		QString worldType(const FriendsApi::World& world)
		{
			switch (world.type) {
				case FriendsApi::World::Type::Server:
					return QStringLiteral("server");
				case FriendsApi::World::Type::Singleplayer:
					return QStringLiteral("singleplayer");
				case FriendsApi::World::Type::None:
					break;
			}
			return QString();
		}

		QString stateName(FriendsApi::Status::State state)
		{
			switch (state) {
				case FriendsApi::Status::State::Online:
					return QStringLiteral("online");
				case FriendsApi::Status::State::Playing:
					return QStringLiteral("playing");
				case FriendsApi::Status::State::Offline:
					break;
			}
			return QStringLiteral("offline");
		}
	} // namespace

	QString relativeTime(const QDateTime& then, const QDateTime& now)
	{
		const qint64 minutes =
			then.secsTo(now) / kSecondsPerMinute;
		if (minutes < 1) {
			return tr("Just now");
		}
		if (minutes < kMinutesPerHour) {
			return tr("%1 min ago").arg(minutes);
		}
		const qint64 hours = minutes / kMinutesPerHour;
		if (hours < kHoursPerDay) {
			return tr("%1 h ago").arg(hours);
		}
		const qint64 days = hours / kHoursPerDay;
		if (days == 1) {
			return tr("Yesterday");
		}
		if (days < kDaysPerMonth) {
			return tr("%1 days ago").arg(days);
		}
		const bool sameYear =
			then.date().year() == now.toLocalTime().date().year();
		return QLocale().toString(then.toLocalTime().date(),
								  sameYear ? "MMM d" : "MMM d, yyyy");
	}

	QString statusText(const FriendsApi::Status& status, const QDateTime& now)
	{
		using State = FriendsApi::Status::State;
		using Type = FriendsApi::World::Type;
		switch (status.state) {
			case State::Playing:
				if (status.world.type == Type::Server) {
					return status.world.host.isEmpty()
							   ? tr("On a server")
							   : tr("On %1").arg(status.world.host);
				}
				if (status.world.type == Type::Singleplayer) {
					return tr("Single-player");
				}
				return tr("Playing");
			case State::Online:
				return tr("Online");
			case State::Offline:
				break;
		}
		if (status.lastSeenAt.isValid()) {
			return tr("Last seen %1").arg(relativeTime(status.lastSeenAt, now));
		}
		return tr("Offline");
	}

	QVariantMap friendMap(const FriendsApi::Friend& one, const QDateTime& now)
	{
		const auto& status = one.status;
		const bool playing = status.state == FriendsApi::Status::State::Playing;
		QVariantMap map;
		map["uuid"] = one.uuid;
		map["name"] = one.name;
		map["since"] = iso(one.since);
		map["state"] = stateName(status.state);
		map["lastSeenAt"] = iso(status.lastSeenAt);
		map["instance"] = playing ? status.instance : QString();
		map["version"] = playing ? status.version : QString();
		map["worldType"] = playing ? worldType(status.world) : QString();
		map["host"] = playing ? status.world.host : QString();
		map["port"] = playing ? status.world.port : 0;
		map["statusText"] = statusText(status, now);
		return map;
	}

	QVariantList friendList(const QList<FriendsApi::Friend>& friends,
							const QDateTime& now)
	{
		QVariantList list;
		for (const auto& one : friends) {
			list.append(friendMap(one, now));
		}
		return list;
	}

	QVariantMap personMap(const FriendsApi::Person& person)
	{
		return {{"uuid", person.uuid},
				{"name", person.name},
				{"createdAt", iso(person.createdAt)}};
	}

	QVariantList personList(const QList<FriendsApi::Person>& people)
	{
		QVariantList list;
		for (const auto& person : people) {
			list.append(personMap(person));
		}
		return list;
	}

	QVariantMap settingsMap(const FriendsApi::Settings& settings)
	{
		return {{"visibility", settings.visibility},
				{"hideServer", settings.hideServer},
				{"shareIpServers", settings.shareIpServers}};
	}
} // namespace FriendsDisplay
