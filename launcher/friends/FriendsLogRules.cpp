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

#include "FriendsLogRules.h"

#include <QRegularExpression>

namespace FriendsLogRules
{
	namespace
	{
		constexpr int kMaxPort = 65535;
		/* The log's own prefix: "[time] [thread/INFO]", then any number of
		 * "[logger/]" brackets, then ": ". The thread is captured. */
		const QString kPrefix = QStringLiteral(
			"^\\[[^\\]]+\\] \\[(?<thread>[^\\]]+)/INFO\\](?: \\[[^\\]]*\\])*: ");
		const QString kServerThread = QStringLiteral("Server thread");

		const QRegularExpression& connectRule()
		{
			static const QRegularExpression rule(
				kPrefix + "Connecting to (?<host>.+), (?<port>\\d+)\\s*$");
			return rule;
		}

		const QRegularExpression& integratedRule()
		{
			static const QRegularExpression rule(
				kPrefix +
				"Starting (?:integrated )?minecraft server version \\S+\\s*$");
			return rule;
		}

		const QRegularExpression& stoppingRule()
		{
			static const QRegularExpression rule(kPrefix +
												 "Stopping server\\s*$");
			return rule;
		}

		Change connect(const QRegularExpressionMatch& match)
		{
			Change change;
			const QString host = FriendsApi::normalizeHost(match.captured("host"));
			if (host.isEmpty()) {
				change.kind = Change::Kind::Clear;
				return change;
			}
			bool ok = false;
			const int port = match.captured("port").toInt(&ok);
			change.kind = Change::Kind::Set;
			change.world.type = FriendsApi::World::Type::Server;
			change.world.host = host;
			change.world.port = ok && port >= 1 && port <= kMaxPort ? port : 0;
			return change;
		}
	} // namespace

	Change fromLine(const QString& line)
	{
		/* The cheap test first: this runs for every line of every game. */
		if (!line.contains("Connecting to") && !line.contains("minecraft server") &&
			!line.contains("Stopping server")) {
			return {};
		}
		auto match = connectRule().match(line);
		if (match.hasMatch()) {
			return connect(match);
		}
		match = integratedRule().match(line);
		if (match.hasMatch() && match.captured("thread") == kServerThread) {
			Change change;
			change.kind = Change::Kind::Set;
			change.world.type = FriendsApi::World::Type::Singleplayer;
			return change;
		}
		match = stoppingRule().match(line);
		if (match.hasMatch() && match.captured("thread") == kServerThread) {
			Change change;
			change.kind = Change::Kind::LeftSingleplayer;
			return change;
		}
		return {};
	}

	FriendsApi::World apply(const Change& change,
							const FriendsApi::World& current)
	{
		using Kind = Change::Kind;
		switch (change.kind) {
			case Kind::Set:
				return change.world;
			case Kind::Clear:
				return {};
			case Kind::LeftSingleplayer:
				return current.type == FriendsApi::World::Type::Singleplayer
						   ? FriendsApi::World()
						   : current;
			case Kind::None:
				break;
		}
		return current;
	}
} // namespace FriendsLogRules
