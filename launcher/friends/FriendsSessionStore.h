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
 * The friends server's session token, kept in friends.json in the data
 * folder next to accounts.json, and readable by the user alone. It is a
 * Kite token, not a Minecraft one: it opens the friends list and nothing
 * else.
 */
namespace FriendsSessionStore
{
	/// An empty session when there is no file or it is not one.
	FriendsApi::Session load(const QString& path);
	/// Whether the file was written, with owner-only permissions.
	bool save(const QString& path, const FriendsApi::Session& session);
	/// Deletes the file.
	void forget(const QString& path);

	/// Whether @p session can be used at @p now for the account
	/// @p profileId / @p profileName: it has a token that is not about to
	/// expire, and it was issued to that very account and name (a renamed
	/// account signs in again, because the server only learns names then).
	bool usable(const FriendsApi::Session& session, const QString& profileId,
				const QString& profileName, const QDateTime& now);
} // namespace FriendsSessionStore
