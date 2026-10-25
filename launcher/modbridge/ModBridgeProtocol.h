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
#include <QHash>
#include <QString>

#include <optional>

/*
 * The transport of the Kite mod <-> launcher bridge (protocol v1, see
 * kite-mod/docs/BRIDGE.md), as plain functions over plain structs: reading
 * a request, the checks that keep a browser or another program out, the
 * token, and the answer's bytes. No socket and no Qt object lives here, so
 * every rule of the document is tested without a network. The messages
 * themselves are in ModBridgeJson; ModBridge carries them over a socket.
 */
namespace ModBridgeProtocol
{
	constexpr int kVersion = 1;
	/// The request line and headers; more is answered 413.
	constexpr qsizetype kMaxHeadBytes = 8 * 1024;
	constexpr qsizetype kMaxBodyBytes = 4 * 1024;
	/// A request that is not complete by then is dropped.
	constexpr int kRequestTimeoutMs = 5000;
	constexpr int kMaxConnections = 8;
	/// This many wrong tokens lock the bridge for kLockoutMs.
	constexpr int kMaxBadTokens = 20;
	constexpr int kLockoutMs = 60 * 1000;
	constexpr int kTokenBytes = 32;
	/// The port of a Minecraft server that names none.
	constexpr int kDefaultServerPort = 25565;
	/// How the game finds the bridge: the port as a JVM property, the token
	/// in the environment (argv is logged and shows in `ps`).
	inline constexpr char kPortProperty[] = "kite.bridge.port";
	inline constexpr char kTokenEnvVar[] = "KITE_BRIDGE_TOKEN";

	/// The statuses the bridge answers with.
	namespace Http
	{
		constexpr int kOk = 200;
		constexpr int kAccepted = 202;
		constexpr int kNoContent = 204;
		constexpr int kBadRequest = 400;
		constexpr int kUnauthorized = 401;
		constexpr int kForbidden = 403;
		constexpr int kNotFound = 404;
		constexpr int kMethodNotAllowed = 405;
		constexpr int kConflict = 409;
		constexpr int kPayloadTooLarge = 413;
		constexpr int kUnsupportedMediaType = 415;
		constexpr int kMisdirected = 421;
		constexpr int kTooManyRequests = 429;
	} // namespace Http

	/// What one game is given when its session is registered.
	struct Grant {
		quint16 port = 0;
		QString token;

		bool valid() const
		{
			return port != 0 && !token.isEmpty();
		}
	};

	/// A request read off the wire. Header names are lower case.
	struct Request {
		QByteArray method;
		/// The path of the target, without a query.
		QByteArray path;
		QHash<QByteArray, QByteArray> headers;
		QByteArray body;

		QByteArray header(const QByteArray& lowerName) const
		{
			return headers.value(lowerName);
		}
	};

	struct Parsed {
		/// NeedMore: wait for more bytes. Complete: @p request is whole.
		/// Rejected: answer @p status and close.
		enum class State { NeedMore, Complete, Rejected };
		State state = State::NeedMore;
		int status = 0;
		Request request;
	};

	/// Reads one request from @p buffer (the bytes received so far): the
	/// head up to the empty line, then Content-Length bytes of body. A head
	/// over kMaxHeadBytes, a body over kMaxBodyBytes (413), Transfer-Encoding,
	/// a duplicated header, a folded line or anything not HTTP/1.x (400) is
	/// refused as soon as it is seen.
	Parsed parseRequest(const QByteArray& buffer);

	/// The status of the first rule the request breaks, 0 if none: a Host
	/// other than exactly 127.0.0.1:<port> (421), an Origin or Sec-Fetch-*
	/// header, which only a browser sends (403), a method other than GET and
	/// POST (405).
	int checkRequest(const Request& request, quint16 port);

	/// 415 when a POST is not application/json (parameters are allowed), 0
	/// otherwise.
	int checkContentType(const Request& request);

	/// The token of "Authorization: Bearer <token>".
	std::optional<QByteArray> bearerToken(const Request& request);

	/// Whether the two are equal, taking as long for a wrong guess as for a
	/// right one.
	bool tokenMatches(const QByteArray& expected, const QByteArray& presented);

	/// 32 random bytes, base64url without padding.
	QString newToken();

	/// Counts wrong tokens and locks everything for a while after too many.
	/// @p nowMs is any monotonic clock in milliseconds.
	class Lockout
	{
	  public:
		bool locked(qint64 nowMs) const;
		void recordFailure(qint64 nowMs);

	  private:
		int m_failures = 0;
		qint64 m_lockedUntilMs = 0;
	};

	/// The "<uuid>" of /v1/faces/<uuid>.png (with dashes, as friends
	/// carry it), nothing for any other path.
	std::optional<QString> faceUuid(const QByteArray& path);

	/// A whole answer: the head, no CORS headers, Cache-Control: no-store,
	/// Connection: close, and @p body (none for 204).
	QByteArray response(int status, const QByteArray& body = QByteArray(),
						const QByteArray& contentType = "application/json");
	/// response() with {"error": @p code}.
	QByteArray errorResponse(int status, const QByteArray& code);
} // namespace ModBridgeProtocol
