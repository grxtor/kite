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

#include "AuthSession.h"
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

QString AuthSession::serializeUserProperties()
{
	// A plugin may fill user_properties through MMCO_HOOK_SESSION_FILL; it is
	// passed through as-is and must already be a valid JSON object literal.
	// Otherwise emit the empty-object default vanilla Mojang flows want.
	const QString trimmed = user_properties.trimmed();
	if (!trimmed.isEmpty()) {
		QJsonParseError err{};
		auto doc = QJsonDocument::fromJson(trimmed.toUtf8(), &err);
		if (err.error == QJsonParseError::NoError && doc.isObject())
			return doc.toJson(QJsonDocument::Compact);
		// Malformed: fall through to the empty default so the launch still
		// works.
	}
	return QJsonDocument(QJsonObject{}).toJson(QJsonDocument::Compact);
}

bool AuthSession::MakeOffline(QString offline_playername)
{
	if (status != PlayableOffline && status != PlayableOnline) {
		return false;
	}
	session = "-";
	player_name = offline_playername;
	status = PlayableOffline;
	return true;
}

void AuthSession::MakeDemo()
{
	player_name = "Player";
	demo = true;
}
