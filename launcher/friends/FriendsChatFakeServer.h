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

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QStringList>

#include <functional>

#include "FriendsChat.h"

/* For tests: stands in for FriendsController behind a FriendsChat, recording
 * what it sends and answering when told. */
namespace FriendsChatFake
{
	inline const QString kMe = QStringLiteral("069a79f4-44e9-4726-a5be-fca90e38aaf5");
	inline const QString kJeb = QStringLiteral("853c80ef-3c37-49fd-aa49-938b674adae6");

	inline QJsonObject message(const QString& id, const QString& from, const QString& text,
							   const QString& at = QStringLiteral("2026-11-04T10:00:00.000Z"))
	{
		return {{"id", id}, {"from", from}, {"to", from == kMe ? kJeb : kMe},
				{"text", text}, {"createdAt", at}, {"deleted", false}};
	}

	inline QByteArray page(const QJsonArray& messages, bool more = false)
	{
		return QJsonDocument(QJsonObject{{"messages", messages}, {"more", more}})
			.toJson(QJsonDocument::Compact);
	}

	inline FriendsApi::Reply reply(int status, const QByteArray& body = QByteArray(),
								   int retryAfter = 0)
	{
		FriendsApi::Reply answer;
		answer.status = status;
		answer.body = body;
		answer.retryAfterSeconds = retryAfter;
		return answer;
	}

	struct Server {
		struct Request {
			QByteArray verb;
			QString path;
			QByteArray body;
			std::function<void(const FriendsApi::Reply&)> done;
		};
		bool socketUp = true;
		QList<QJsonObject> frames;
		QList<Request> requests;
		QStringList finished;

		FriendsChat::Transport transport()
		{
			FriendsChat::Transport t;
			t.sendFrame = [this](const QJsonObject& frame) {
				if (socketUp) {
					frames.append(frame);
				}
				return socketUp;
			};
			t.request = [this](const QByteArray& verb, const QString& path, const QByteArray& body,
							   std::function<void(const FriendsApi::Reply&)> done) {
				requests.append({verb, path, body, std::move(done)});
			};
			t.finished = [this](const QString& action, bool ok, const QString&) {
				finished.append(action + (ok ? QStringLiteral(":ok") : QStringLiteral(":failed")));
			};
			return t;
		}
		/* Answers the oldest request still waiting. */
		void answer(const FriendsApi::Reply& reply)
		{
			auto request = requests.takeFirst();
			request.done(reply);
		}
	};
} // namespace FriendsChatFake
