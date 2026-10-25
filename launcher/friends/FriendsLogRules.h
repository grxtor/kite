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

#include <QString>

#include "FriendsApi.h"

/*
 * Where the player is, read off the game's log. The launcher already
 * receives every line the game prints; three of them say what a friend
 * wants to know:
 *
 *   [12:00:00] [Render thread/INFO]: Connecting to mc.hypixel.net, 25565
 *   [12:00:00] [Server thread/INFO]: Starting integrated minecraft server version 1.8.9
 *   [12:00:00] [Server thread/INFO]: Stopping server
 *
 * (Forge and NeoForge put a date in the first bracket and a logger name in
 * another one after the level; both are accepted.) A line is only believed
 * if it has the log's own prefix and the right thread, so a chat message or
 * a mod's output that merely contains the words changes nothing.
 */
namespace FriendsLogRules
{
	struct Change {
		enum class Kind {
			/// Not a line that says anything about the world.
			None,
			/// The player is now in @p world.
			Set,
			/// The player is nowhere known (a connect to something that is
			/// not a host).
			Clear,
			/// The integrated server stopped: the player left the
			/// single-player world, which is only news while they are in it.
			LeftSingleplayer
		};
		Kind kind = Kind::None;
		FriendsApi::World world;
	};

	Change fromLine(const QString& line);

	/// The world after @p change: @p current, replaced or cleared.
	FriendsApi::World apply(const Change& change,
							const FriendsApi::World& current);
} // namespace FriendsLogRules
