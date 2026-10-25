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

#include "FriendsSessionStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace FriendsSessionStore
{
	namespace
	{
		/* A token that expires within this is not worth starting with. */
		constexpr int kExpiryMarginSeconds = 60 * 60;
	} // namespace

	FriendsApi::Session load(const QString& path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return {};
		}
		const auto doc = QJsonDocument::fromJson(file.readAll());
		if (!doc.isObject()) {
			return {};
		}
		const auto object = doc.object();
		FriendsApi::Session session;
		session.token = object.value("token").toString();
		session.expiresAt = QDateTime::fromString(
			object.value("expiresAt").toString(), Qt::ISODateWithMs);
		session.uuid = object.value("uuid").toString();
		session.name = object.value("name").toString();
		if (session.token.isEmpty() || !session.expiresAt.isValid()) {
			return {};
		}
		return session;
	}

	bool save(const QString& path, const FriendsApi::Session& session)
	{
		QDir().mkpath(QFileInfo(path).absolutePath());
		const QJsonObject object{
			{"token", session.token},
			{"expiresAt", session.expiresAt.toUTC().toString(Qt::ISODateWithMs)},
			{"uuid", session.uuid},
			{"name", session.name}};
		QSaveFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		file.write(QJsonDocument(object).toJson());
		file.setPermissions(QFile::ReadOwner | QFile::WriteOwner |
							QFile::ReadUser | QFile::WriteUser);
		return file.commit();
	}

	void forget(const QString& path)
	{
		QFile::remove(path);
	}

	bool usable(const FriendsApi::Session& session, const QString& profileId,
				const QString& profileName, const QDateTime& now)
	{
		return !session.token.isEmpty() && session.expiresAt.isValid() &&
			   now.secsTo(session.expiresAt) > kExpiryMarginSeconds &&
			   FriendsApi::undashed(session.uuid) ==
				   FriendsApi::undashed(profileId) &&
			   session.name == profileName;
	}
} // namespace FriendsSessionStore
