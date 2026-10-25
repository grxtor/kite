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

namespace ModBridgeProtocol
{
	namespace
	{
		const QByteArray kHeadEnd = QByteArrayLiteral("\r\n\r\n");
		const QByteArray kLineEnd = QByteArrayLiteral("\r\n");
		const QByteArray kSecFetchPrefix = QByteArrayLiteral("sec-fetch-");
		const QByteArray kJsonType = QByteArrayLiteral("application/json");
		/* A Content-Length of more digits than this cannot be a body the
		 * bridge takes. */
		constexpr int kMaxLengthDigits = 9;

		Parsed reject(int status)
		{
			Parsed parsed;
			parsed.state = Parsed::State::Rejected;
			parsed.status = status;
			return parsed;
		}

		/* Everything before the first @p c (all of it when there is none). */
		QByteArray beforeFirst(const QByteArray& text, char c)
		{
			const qsizetype at = text.indexOf(c);
			return at < 0 ? text : text.left(at);
		}

		QByteArray withoutCr(const QByteArray& line)
		{
			return line.endsWith('\r') ? line.chopped(1) : line;
		}

		bool isTokenChar(char c)
		{
			/* RFC 9110 tchar: what a method or a header name is made of. */
			return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
				   (c >= 'A' && c <= 'Z') ||
				   QByteArrayLiteral("!#$%&'*+-.^_`|~").contains(c);
		}

		bool isToken(const QByteArray& text)
		{
			if (text.isEmpty()) {
				return false;
			}
			for (char c : text) {
				if (!isTokenChar(c)) {
					return false;
				}
			}
			return true;
		}

		/* "GET /v1/hello HTTP/1.1" */
		bool parseRequestLine(const QByteArray& line, Request& request)
		{
			const QList<QByteArray> parts = line.split(' ');
			if (parts.size() != 3 || !isToken(parts[0]) ||
				!parts[1].startsWith('/') ||
				!(parts[2] == "HTTP/1.1" || parts[2] == "HTTP/1.0")) {
				return false;
			}
			request.method = parts[0];
			request.path = beforeFirst(parts[1], '?');
			return true;
		}

		/* "Name: value"; false for a folded line, a name with a space before
		 * the colon, or a header sent twice. */
		bool parseHeaderLine(const QByteArray& line, Request& request)
		{
			if (line.startsWith(' ') || line.startsWith('\t')) {
				return false;
			}
			const qsizetype colon = line.indexOf(':');
			if (colon <= 0) {
				return false;
			}
			const QByteArray name = line.left(colon);
			if (!isToken(name)) {
				return false;
			}
			const QByteArray key = name.toLower();
			if (request.headers.contains(key)) {
				return false;
			}
			request.headers.insert(key, line.mid(colon + 1).trimmed());
			return true;
		}

		/* The Content-Length: -1 for none sent, -2 for one that is not a
		 * number. */
		qint64 contentLength(const Request& request)
		{
			if (!request.headers.contains("content-length")) {
				return -1;
			}
			const QByteArray text = request.header("content-length");
			if (text.isEmpty() || text.size() > kMaxLengthDigits) {
				return -2;
			}
			for (char c : text) {
				if (c < '0' || c > '9') {
					return -2;
				}
			}
			return text.toLongLong();
		}
	} // namespace

	Parsed parseRequest(const QByteArray& buffer)
	{
		const qsizetype headEnd = buffer.indexOf(kHeadEnd);
		if (headEnd < 0) {
			return buffer.size() > kMaxHeadBytes
					   ? reject(Http::kPayloadTooLarge)
					   : Parsed();
		}
		if (headEnd > kMaxHeadBytes) {
			return reject(Http::kPayloadTooLarge);
		}
		Parsed parsed;
		const QByteArray head = buffer.left(headEnd);
		/* Lines end in \r\n; a bare \r or \n inside one (which servers read
		 * differently) is refused. */
		if (head.count('\n') != head.count(kLineEnd) ||
			head.count('\r') != head.count(kLineEnd)) {
			return reject(Http::kBadRequest);
		}
		const QList<QByteArray> lines = head.split('\n');
		if (!parseRequestLine(withoutCr(lines[0]), parsed.request)) {
			return reject(Http::kBadRequest);
		}
		for (qsizetype i = 1; i < lines.size(); ++i) {
			if (!parseHeaderLine(withoutCr(lines[i]), parsed.request)) {
				return reject(Http::kBadRequest);
			}
		}
		if (parsed.request.headers.contains("transfer-encoding")) {
			return reject(Http::kBadRequest);
		}
		const qint64 length = contentLength(parsed.request);
		if (length == -2) {
			return reject(Http::kBadRequest);
		}
		if (length > kMaxBodyBytes) {
			return reject(Http::kPayloadTooLarge);
		}
		const qsizetype bodyStart = headEnd + kHeadEnd.size();
		const qsizetype bodySize = length < 0 ? 0 : static_cast<qsizetype>(length);
		if (buffer.size() < bodyStart + bodySize) {
			return Parsed();
		}
		parsed.request.body = buffer.mid(bodyStart, bodySize);
		parsed.state = Parsed::State::Complete;
		return parsed;
	}

	int checkRequest(const Request& request, quint16 port)
	{
		const QByteArray host = QByteArrayLiteral("127.0.0.1:") +
								QByteArray::number(static_cast<int>(port));
		if (request.header("host") != host) {
			return Http::kMisdirected;
		}
		for (auto it = request.headers.constBegin();
			 it != request.headers.constEnd(); ++it) {
			if (it.key() == "origin" || it.key().startsWith(kSecFetchPrefix)) {
				return Http::kForbidden;
			}
		}
		if (request.method != "GET" && request.method != "POST") {
			return Http::kMethodNotAllowed;
		}
		return 0;
	}

	int checkContentType(const Request& request)
	{
		if (request.method != "POST") {
			return 0;
		}
		const QByteArray value = request.header("content-type");
		const QByteArray type = beforeFirst(value, ';').trimmed().toLower();
		return type == kJsonType ? 0 : Http::kUnsupportedMediaType;
	}

	std::optional<QByteArray> bearerToken(const Request& request)
	{
		const QByteArray value = request.header("authorization");
		const qsizetype space = value.indexOf(' ');
		if (space <= 0 || value.left(space).toLower() != "bearer") {
			return std::nullopt;
		}
		const QByteArray token = value.mid(space + 1).trimmed();
		if (token.isEmpty()) {
			return std::nullopt;
		}
		return token;
	}
} // namespace ModBridgeProtocol
