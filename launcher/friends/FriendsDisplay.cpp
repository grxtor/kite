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
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>

namespace FriendsDisplay
{
	namespace
	{
		constexpr qint64 kSecondsPerMinute = 60;
		constexpr qint64 kMinutesPerHour = 60;
		constexpr qint64 kHoursPerDay = 24;
		constexpr qint64 kDaysPerMonth = 30;

		/* The context lupdate files these strings under: it names the
		 * namespace's own tr(), whatever context is passed on here. */
		QString tr(const char* text)
		{
			return QCoreApplication::translate("FriendsDisplay", text);
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

	QString lastSeenText(const QDateTime& then, const QDateTime& now)
	{
		/* Whole sentences, not a time pasted after "Last seen": "Yesterday"
		 * and "Just now" would stand capitalised mid-sentence. */
		const qint64 minutes =
			then.secsTo(now) / kSecondsPerMinute;
		if (minutes < 1) {
			return tr("Last seen just now");
		}
		if (minutes < kMinutesPerHour) {
			return tr("Last seen %1 min ago").arg(minutes);
		}
		const qint64 hours = minutes / kMinutesPerHour;
		if (hours < kHoursPerDay) {
			return tr("Last seen %1 h ago").arg(hours);
		}
		const qint64 days = hours / kHoursPerDay;
		if (days == 1) {
			return tr("Last seen yesterday");
		}
		if (days < kDaysPerMonth) {
			return tr("Last seen %1 days ago").arg(days);
		}
		const bool sameYear =
			then.date().year() == now.toLocalTime().date().year();
		return tr("Last seen %1")
			.arg(QLocale().toString(then.toLocalTime().date(),
									sameYear ? "MMM d" : "MMM d, yyyy"));
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
					return tr("Singleplayer");
				}
				return tr("Playing");
			case State::Online:
				/* Online is the launcher being open: under an "Online"
				 * caption "Online" says nothing, "In Kite" does. */
				return tr("In Kite");
			case State::Offline:
				break;
		}
		if (status.lastSeenAt.isValid()) {
			return lastSeenText(status.lastSeenAt, now);
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
		/* Only a server with a visible host can be joined, whatever the
		 * friend allows. The host goes to the game as the server to enter,
		 * so one that is not a clean DNS name or IP address (what the
		 * server itself would have refused) is never offered, nor one that
		 * only means the joiner's own machine (localhost). */
		const bool server = playing && status.world.type ==
										   FriendsApi::World::Type::Server;
		const bool hostShown = server && !status.world.host.isEmpty();
		map["joinable"] =
			hostShown && status.joinable &&
			FriendsApi::normalizeHost(status.world.host) ==
				status.world.host &&
			!FriendsApi::isLoopbackHost(status.world.host);
		/* A visible server the server says is not joinable: the friend turned
		 * joining off. One that says nothing (an older server) is not
		 * blamed on the friend. */
		map["joinOff"] = hostShown && status.joinableSaid && !status.joinable;
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

	QVariantMap selfStatusMap(const FriendsApi::Presence& presence,
							  const QVariantMap& settings)
	{
		using Type = FriendsApi::World::Type;
		const bool server = presence.playing &&
							presence.world.type == Type::Server &&
							!presence.world.host.isEmpty();
		/* A raw IP address stays hidden unless "Show servers I join by IP
		 * address" is on (the server applies the same rule). */
		const bool rawAddress =
			server && presence.world.host.contains(QRegularExpression(
						  QStringLiteral("^[0-9.]+$|:")));
		const bool hidden = settings.value("hideServer").toBool() ||
							(rawAddress &&
							 !settings.value("shareIpServers").toBool());
		const bool shown = server && !hidden;
		return {{"playing", presence.playing},
				{"worldType", presence.playing ? worldType(presence.world)
											   : QString()},
				{"host", shown ? presence.world.host : QString()},
				{"serverHidden", server && hidden},
				{"joinable", shown && settings.value("allowJoin").toBool()}};
	}

	QVariantMap selfStatusFromView(const FriendsApi::Presence& sent,
								   const FriendsApi::Status& view)
	{
		using Type = FriendsApi::World::Type;
		const bool playing = view.state == FriendsApi::Status::State::Playing;
		const bool server = playing && view.world.type == Type::Server;
		const bool sentServer = sent.playing && sent.world.type == Type::Server &&
								!sent.world.host.isEmpty();
		const QString host = server ? view.world.host : QString();
		return {{"playing", playing},
				{"worldType", playing ? worldType(view.world) : QString()},
				{"host", host},
				/* A server was reported and the server kept its address
				 * from friends. */
				{"serverHidden", sentServer && playing && host.isEmpty()},
				{"joinable", playing && view.joinable}};
	}

	QVariantMap settingsMap(const FriendsApi::Settings& settings)
	{
		return {{"visibility", settings.visibility},
				{"hideServer", settings.hideServer},
				{"shareIpServers", settings.shareIpServers},
				{"allowJoin", settings.allowJoin}};
	}

	bool settingsSaved(const QByteArray& body, const QVariantMap& saved)
	{
		const QJsonObject asked = QJsonDocument::fromJson(body).object();
		for (auto it = asked.begin(); it != asked.end(); ++it) {
			if (saved.value(it.key()) != it.value().toVariant()) {
				return false;
			}
		}
		return true;
	}
} // namespace FriendsDisplay
