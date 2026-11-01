/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

	bool parseXTokenResponse(QByteArray& data, Katabasis::Token& output,
							 QString name);
	bool parseMojangResponse(QByteArray& data, Katabasis::Token& output);

	bool parseMinecraftProfile(QByteArray& data, MinecraftProfile& output);

	// Parses a public profile from sessionserver.mojang.com/session/minecraft/
	// profile/<id>. Unlike parseMinecraftProfile() (the signed-in account's own
	// profile) it describes any player: id, name and base64 texture URLs only.
	// Fills output.skin (the default Steve/Alex texture if none was set) and,
	// for a cape, one entry under a placeholder id. Used to copy another
	// player's skin.
	bool parseMojangSessionProfile(QByteArray& data, MinecraftProfile& output);

	bool parseMinecraftEntitlements(QByteArray& data,
									MinecraftEntitlement& output);

	// @p data, a sign-in service's reply, made fit for the log: every token
	// field (Token, access_token, refresh_token, id_token, identityToken, at
	// any depth and case) becomes "<redacted>". A reply that is not JSON comes
	// back cut to 256 bytes. Logs get shared and a token in one signs its
	// reader in.
	QByteArray redactTokens(const QByteArray& data);
} // namespace Parsers
