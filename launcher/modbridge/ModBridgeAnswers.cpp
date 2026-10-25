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

#include "ModBridgeProtocol.h"

#include <QRandomGenerator>

#include "friends/FriendsApi.h"

/* The token, the lockout and the answer's bytes (the other half of
 * ModBridgeProtocol.h; reading a request is ModBridgeProtocol.cpp). */
namespace ModBridgeProtocol
{
	namespace
	{
		const QString kFacePrefix = QStringLiteral("/v1/faces/");
		const QString kFaceSuffix = QStringLiteral(".png");

		const char* reason(int status)
		{
			switch (status) {
				case Http::kOk:
					return "OK";
				case Http::kAccepted:
					return "Accepted";
				case Http::kNoContent:
					return "No Content";
				case Http::kUnauthorized:
					return "Unauthorized";
				case Http::kForbidden:
					return "Forbidden";
				case Http::kNotFound:
					return "Not Found";
				case Http::kMethodNotAllowed:
					return "Method Not Allowed";
				case Http::kConflict:
					return "Conflict";
				case Http::kPayloadTooLarge:
					return "Payload Too Large";
				case Http::kUnsupportedMediaType:
					return "Unsupported Media Type";
				case Http::kMisdirected:
					return "Misdirected Request";
				case Http::kTooManyRequests:
					return "Too Many Requests";
				default:
					return "Bad Request";
			}
		}
	} // namespace

	bool tokenMatches(const QByteArray& expected, const QByteArray& presented)
	{
		const qsizetype size = qMax(expected.size(), presented.size());
		/* Every byte of the longer one is looked at whatever is found. */
		quint8 difference = expected.size() == presented.size() ? 0 : 1;
		for (qsizetype i = 0; i < size; ++i) {
			const quint8 a = i < expected.size() ? expected.at(i) : 0;
			const quint8 b = i < presented.size() ? presented.at(i) : 0;
			difference |= a ^ b;
		}
		return difference == 0;
	}

	QString newToken()
	{
		QByteArray bytes(kTokenBytes, Qt::Uninitialized);
		QRandomGenerator::system()->fillRange(
			reinterpret_cast<quint32*>(bytes.data()),
			kTokenBytes / static_cast<int>(sizeof(quint32)));
		return QString::fromLatin1(bytes.toBase64(
			QByteArray::Base64UrlEncoding | QByteArray::OmitTrailingEquals));
	}

	bool Lockout::locked(qint64 nowMs) const
	{
		return nowMs < m_lockedUntilMs;
	}

	void Lockout::recordFailure(qint64 nowMs)
	{
		if (++m_failures >= kMaxBadTokens) {
			m_failures = 0;
			m_lockedUntilMs = nowMs + kLockoutMs;
		}
	}

	std::optional<QString> faceUuid(const QByteArray& path)
	{
		const QString text = QString::fromLatin1(path);
		if (!text.startsWith(kFacePrefix) || !text.endsWith(kFaceSuffix)) {
			return std::nullopt;
		}
		const QString uuid =
			text.mid(kFacePrefix.size(),
					 text.size() - kFacePrefix.size() - kFaceSuffix.size());
		if (!FriendsApi::isUuid(uuid)) {
			return std::nullopt;
		}
		return uuid;
	}

	QByteArray response(int status, const QByteArray& body,
						const QByteArray& contentType)
	{
		QByteArray out = "HTTP/1.1 " + QByteArray::number(status) + ' ' +
						 reason(status) + "\r\n";
		if (status == Http::kUnauthorized) {
			out += "WWW-Authenticate: Bearer\r\n";
		}
		if (status != Http::kNoContent) {
			out += "Content-Type: " + contentType + "\r\n";
			out += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
		}
		out += "Cache-Control: no-store\r\nConnection: close\r\n\r\n";
		if (status != Http::kNoContent) {
			out += body;
		}
		return out;
	}

	QByteArray errorResponse(int status, const QByteArray& code)
	{
		return response(status, "{\"error\":\"" + code + "\"}");
	}
} // namespace ModBridgeProtocol
