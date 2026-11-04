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
#include <QDateTime>
#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

/* The chat part of the friends server (server/friends/README.md, "Chat" and
 * "Realtime socket"): messages, history pages and the socket frames, as plain
 * functions over plain structs, like FriendsApi. */
namespace FriendsChatApi
{
	/// The server's limit, in UTF-16 code units (QString::size()).
	constexpr int kMaxTextLength = 1000;
	/// The most messages one history page holds.
	constexpr int kPageLimit = 50;
	constexpr int kMaxPageLimit = 100;

	struct Message {
		/// Digits; ids only grow (idLess()).
		QString id;
		/// Dashed, lower case.
		QString from;
		QString to;
		/// Empty when deleted.
		QString text;
		QDateTime createdAt;
		/// The sender deleted their account.
		bool deleted = false;
	};

	struct Page {
		/// Oldest first.
		QList<Message> messages;
		/// More past the page in the direction asked for.
		bool more = false;
	};

	/// An `error` frame: `code` as in FriendsApi::Code; `ref` empty when the
	/// frame it answers had none (a socket over its frame limit).
	struct FrameError {
		QString ref;
		QString code;
		QString message;
		int retryAfterSeconds = 0;
	};

	enum class ReportReason { Spam, Harassment, Other };

	/// What the server keeps of @p raw: control and bidi characters (line
	/// breaks too) removed, the ends trimmed.
	QString cleanText(const QString& raw);
	/// Whether @p text may be sent: not empty once cleaned, not over the limit.
	bool sendable(const QString& text);
	/// Numeric order of two message ids; an empty id is the oldest.
	bool idLess(const QString& a, const QString& b);
	/// A socket `ref`: 1 to 64 of A-Z a-z 0-9 _ -.
	bool isRef(const QString& ref);
	/// "spam" | "harassment" | "other", nullopt for anything else.
	std::optional<ReportReason> reasonOf(const QString& name);

	namespace Path
	{
		QString socket();
		QString messages(const QString& uuid);
		/// The newest page without @p before or @p after (one of them, not both).
		QString page(const QString& uuid, const QString& before,
					 const QString& after, int limit);
		QString read(const QString& uuid);
		QString report(const QString& uuid, const QString& id);
	} // namespace Path

	QByteArray sendBody(const QString& text);
	QByteArray readBody(const QString& upTo);
	QByteArray reportBody(ReportReason reason);
	QJsonObject sendFrame(const QString& ref, const QString& to,
						  const QString& text);
	QJsonObject readFrame(const QString& ref, const QString& with,
						  const QString& upTo);

	/// A message object; nullopt without an id, sender or recipient.
	std::optional<Message> parseMessage(const QJsonObject& object);
	/// The body of POST /v1/chats/{uuid}/messages.
	std::optional<Message> parseMessageBody(const QByteArray& body);
	/// The body of GET /v1/chats/{uuid}/messages.
	std::optional<Page> parsePage(const QByteArray& body);
	FrameError parseFrameError(const QJsonObject& frame);
} // namespace FriendsChatApi
