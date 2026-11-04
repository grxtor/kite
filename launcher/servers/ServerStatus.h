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

#include <QByteArray>
#include <QJsonValue>
#include <QMetaType>
#include <QString>

/* What a Minecraft server says about itself in the server list (the status
 * response of the 1.7+ Server List Ping), read into what the launcher shows. */
namespace ServerStatus
{
	/// The lines of a description the game's server list shows.
	constexpr int kMotdLines = 2;

	struct Status {
		/// -1 when the server keeps it to itself.
		int online = -1;
		int max = -1;
		/// version.name as plain text ("Paper 1.21.4", "Requires MC 1.8 / 1.21").
		QString version;
		int protocol = -1;
		/// The description as Text.StyledText, its colours made readable on a
		/// dark surface (motd) or a light one (motdLight); motdText is plain.
		QString motd;
		QString motdLight;
		QString motdText;
		/// "data:image/png;base64,..." or "" when the server has none.
		QString favicon;
		/// Round trip of the ping, in milliseconds; -1 if unmeasured.
		int latencyMs = -1;
	};

	struct Motd {
		QString dark;
		QString light;
		QString text;
	};

	/// Reads a status response's JSON. False, and why in @p error, when it is
	/// not a JSON object.
	bool parse(const QByteArray& json, Status* status, QString* error = nullptr);

	/// A description (a JSON text component, or a string with § codes) as
	/// StyledText: escaped, at most kMotdLines lines, each trimmed, colours
	/// and bold/italic/underline/strikethrough kept, obfuscated text left out
	/// (it is random letters in the game).
	Motd formatMotd(const QJsonValue& description);

	/// @p text with every § code taken out.
	QString stripCodes(const QString& text);
} // namespace ServerStatus

Q_DECLARE_METATYPE(ServerStatus::Status)
