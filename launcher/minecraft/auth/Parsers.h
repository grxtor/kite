/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
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

#include "AccountData.h"

namespace Parsers
{
	bool getDateTime(QJsonValue value, QDateTime& out);
	bool getString(QJsonValue value, QString& out);
	bool getNumber(QJsonValue value, double& out);
	bool getNumber(QJsonValue value, int64_t& out);
	bool getBool(QJsonValue value, bool& out);

	bool parseXTokenResponse(QByteArray& data, Katabasis::Token& output,
							 QString name);
	bool parseMojangResponse(QByteArray& data, Katabasis::Token& output);

	bool parseMinecraftProfile(QByteArray& data, MinecraftProfile& output);

	/* Parse a *public* profile from
	 * sessionserver.mojang.com/session/minecraft/profile/<id>.
	 *
	 * Unlike parseMinecraftProfile(), which reads the signed-in account's own
	 * profile, this describes an arbitrary player: only id, name and texture
	 * URLs come back, and the textures arrive as a base64 blob. Used to copy
	 * another player's skin into the local library.
	 *
	 * Fills output.skin (with the default Steve/Alex texture when the player
	 * never set one) and, if the player has a cape, a single cape entry under
	 * a placeholder id -- the endpoint publishes no cape ids. */
	bool parseMojangSessionProfile(QByteArray& data, MinecraftProfile& output);

	bool parseMinecraftEntitlements(QByteArray& data,
									MinecraftEntitlement& output);
	bool parseRolloutResponse(QByteArray& data, bool& result);

	/* @p data, a sign-in service's reply, made fit for the log: the value
	 * of every token field (Token, access_token, refresh_token, id_token,
	 * identityToken, at any depth, in any case) becomes "<redacted>", the
	 * rest stays for debugging. A reply that is not JSON comes back as it
	 * is, cut to its first 256 bytes. Logs get shared, by Log upload among
	 * others, and a token in one signs its reader in as the player. */
	QByteArray redactTokens(const QByteArray& data);
} // namespace Parsers
