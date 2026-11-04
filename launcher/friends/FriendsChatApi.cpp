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

#include "FriendsChatApi.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QUrlQuery>

#include <algorithm>

namespace FriendsChatApi
{
	namespace
	{
		const QString kChats = QStringLiteral("/v1/chats/");

		QJsonObject objectOf(const QByteArray& body, bool* ok)
		{
			QJsonParseError error{};
			const auto doc = QJsonDocument::fromJson(body, &error);
			*ok = error.error == QJsonParseError::NoError && doc.isObject();
			return *ok ? doc.object() : QJsonObject();
		}

		QByteArray compact(const QJsonObject& object)
		{
			return QJsonDocument(object).toJson(QJsonDocument::Compact);
		}

		QString chat(const QString& uuid)
		{
			return kChats + uuid.toLower();
		}
	} // namespace

	QString cleanText(const QString& raw)
	{
		/* The server's own set (server/friends/src/status.ts). */
		static const QRegularExpression unsafe(QStringLiteral(
			"[\\x{0000}-\\x{001f}\\x{007f}-\\x{009f}\\x{200e}\\x{200f}"
			"\\x{202a}-\\x{202e}\\x{2066}-\\x{2069}]"));
		QString text = raw;
		text.remove(unsafe);
		return text.trimmed();
	}

	bool sendable(const QString& text)
	{
		return text.size() <= kMaxTextLength && !cleanText(text).isEmpty();
	}

	bool idLess(const QString& a, const QString& b)
	{
		/* Digits without leading zeros: a shorter id is a smaller number. */
		if (a.size() != b.size()) {
			return a.size() < b.size();
		}
		return a < b;
	}

	bool isRef(const QString& ref)
	{
		static const QRegularExpression rule(
			QStringLiteral("^[A-Za-z0-9_-]{1,64}$"));
		return rule.match(ref).hasMatch();
	}

	std::optional<ReportReason> reasonOf(const QString& name)
	{
		if (name == QLatin1String("spam")) {
			return ReportReason::Spam;
		}
		if (name == QLatin1String("harassment")) {
			return ReportReason::Harassment;
		}
		if (name == QLatin1String("other")) {
			return ReportReason::Other;
		}
		return std::nullopt;
	}

	namespace Path
	{
		QString socket()
		{
			return QStringLiteral("/v1/ws");
		}
		QString messages(const QString& uuid)
		{
			return chat(uuid) + QStringLiteral("/messages");
		}
		QString page(const QString& uuid, const QString& before,
					 const QString& after, int limit)
		{
			QUrlQuery query;
			if (!before.isEmpty()) {
				query.addQueryItem(QStringLiteral("before"), before);
			} else if (!after.isEmpty()) {
				query.addQueryItem(QStringLiteral("after"), after);
			}
			query.addQueryItem(QStringLiteral("limit"),
							   QString::number(std::clamp(limit, 1, kMaxPageLimit)));
			return messages(uuid) + QLatin1Char('?') + query.toString();
		}
		QString read(const QString& uuid)
		{
			return chat(uuid) + QStringLiteral("/read");
		}
		QString report(const QString& uuid, const QString& id)
		{
			return messages(uuid) + QLatin1Char('/') + id + QStringLiteral("/report");
		}
	} // namespace Path

	QByteArray sendBody(const QString& text)
	{
		return compact({{"text", text}});
	}

	QByteArray readBody(const QString& upTo)
	{
		return compact({{"upTo", upTo}});
	}

	QByteArray reportBody(ReportReason reason)
	{
		QString name = QStringLiteral("other");
		if (reason == ReportReason::Spam) {
			name = QStringLiteral("spam");
		} else if (reason == ReportReason::Harassment) {
			name = QStringLiteral("harassment");
		}
		return compact({{"reason", name}});
	}

	QJsonObject sendFrame(const QString& ref, const QString& to,
						  const QString& text)
	{
		return {{"type", "chat.send"}, {"ref", ref}, {"to", to.toLower()},
				{"text", text}};
	}

	QJsonObject readFrame(const QString& ref, const QString& with,
						  const QString& upTo)
	{
		return {{"type", "chat.read"}, {"ref", ref}, {"with", with.toLower()},
				{"upTo", upTo}};
	}

	std::optional<Message> parseMessage(const QJsonObject& object)
	{
		Message message;
		message.id = object.value("id").toString();
		message.from = object.value("from").toString().toLower();
		message.to = object.value("to").toString().toLower();
		message.text = object.value("text").toString();
		message.createdAt = QDateTime::fromString(
			object.value("createdAt").toString(), Qt::ISODateWithMs);
		message.deleted =
			object.value("deleted").toBool() || object.value("text").isNull();
		if (message.deleted) {
			message.text.clear();
		}
		if (message.id.isEmpty() || message.from.isEmpty() ||
			message.to.isEmpty()) {
			return std::nullopt;
		}
		return message;
	}

	std::optional<Message> parseMessageBody(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		return ok ? parseMessage(object) : std::nullopt;
	}

	std::optional<Page> parsePage(const QByteArray& body)
	{
		bool ok = false;
		const auto object = objectOf(body, &ok);
		if (!ok || !object.value("messages").isArray()) {
			return std::nullopt;
		}
		Page page;
		page.more = object.value("more").toBool();
		for (const auto& entry : object.value("messages").toArray()) {
			if (auto message = parseMessage(entry.toObject())) {
				page.messages.append(*message);
			}
		}
		return page;
	}

	FrameError parseFrameError(const QJsonObject& frame)
	{
		FrameError error;
		error.ref = frame.value("ref").toString();
		error.code = frame.value("error").toString();
		error.message = frame.value("message").toString();
		error.retryAfterSeconds = frame.value("retryAfter").toInt();
		return error;
	}
} // namespace FriendsChatApi
