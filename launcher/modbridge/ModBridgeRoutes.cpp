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

/*
 * What a request to the bridge means: the order of its checks and the
 * endpoints of BRIDGE.md. The bytes of each message are ModBridgeJson's.
 */

#include "ModBridge.h"

#include <QFile>

#include "BaseInstance.h"

using namespace ModBridgeProtocol;

namespace
{
	/* A face is an 8x8 PNG; anything near this is not one. */
	constexpr qint64 kMaxFaceBytes = 64 * 1024;
	const QByteArray kPngType = QByteArrayLiteral("image/png");
	const QByteArray kGet = QByteArrayLiteral("GET");
	const QByteArray kPost = QByteArrayLiteral("POST");

	QByteArray wrongMethod()
	{
		return errorResponse(Http::kMethodNotAllowed, "method_not_allowed");
	}
} // namespace

ModBridgeJson::FriendsView ModBridge::friendsView() const
{
	return m_source ? m_source->friendsView() : ModBridgeJson::FriendsView();
}

QByteArray ModBridge::respond(const Request& request)
{
	if (m_lockout.locked(m_clock.elapsed())) {
		return errorResponse(Http::kTooManyRequests, "locked");
	}
	if (const int status = checkRequest(request, m_server.serverPort())) {
		return errorResponse(status, "refused");
	}
	BaseInstance* instance = authenticate(request);
	if (!instance) {
		return errorResponse(Http::kUnauthorized, "unauthorized");
	}
	if (const int status = checkContentType(request)) {
		return errorResponse(status, "unsupported_media_type");
	}
	return route(instance, request);
}

BaseInstance* ModBridge::authenticate(const Request& request)
{
	const auto token = bearerToken(request);
	BaseInstance* found = nullptr;
	if (token) {
		/* Every session is compared, so the time says nothing about which
		 * one a guess was close to. */
		for (auto it = m_sessions.constBegin(); it != m_sessions.constEnd();
			 ++it) {
			if (tokenMatches(it->token, *token)) {
				found = it.key();
			}
		}
	}
	if (!found) {
		m_lockout.recordFailure(m_clock.elapsed());
	}
	return found;
}

QByteArray ModBridge::route(BaseInstance* instance, const Request& request)
{
	const QByteArray& path = request.path;
	const bool get = request.method == kGet;
	if (path == "/v1/hello") {
		return get ? hello(instance) : wrongMethod();
	}
	if (path == "/v1/friends") {
		return get ? response(Http::kOk, ModBridgeJson::friendsBody(friendsView()))
				   : wrongMethod();
	}
	if (path == "/v1/friends/refresh") {
		if (request.method != kPost) {
			return wrongMethod();
		}
		if (m_source) {
			m_source->requestRefresh();
		}
		return response(Http::kAccepted, "{}");
	}
	if (path == "/v1/world") {
		return request.method == kPost ? reportWorld(instance, request)
									   : wrongMethod();
	}
	if (path == "/v1/join") {
		return request.method == kPost ? join(instance, request)
									   : wrongMethod();
	}
	if (const auto uuid = faceUuid(path)) {
		return get ? face(*uuid) : wrongMethod();
	}
	return errorResponse(Http::kNotFound, "not_found");
}

QByteArray ModBridge::hello(BaseInstance* instance)
{
	const auto view = friendsView();
	const ModBridgeJson::InstanceInfo info{instance->id(), instance->name(),
										   instance->gameVersion()};
	return response(Http::kOk, ModBridgeJson::helloBody(
								   m_launcher, info, view.enabled, view.state));
}

QByteArray ModBridge::reportWorld(BaseInstance* instance,
								  const Request& request)
{
	const auto world = ModBridgeJson::parseWorld(request.body);
	if (!world) {
		return errorResponse(Http::kBadRequest, "bad_world");
	}
	emit worldReported(instance, *world);
	return response(Http::kNoContent);
}

QByteArray ModBridge::join(BaseInstance* instance, const Request& request)
{
	const auto uuid = ModBridgeJson::parseJoinUuid(request.body);
	if (!uuid) {
		return errorResponse(Http::kBadRequest, "bad_uuid");
	}
	const auto answer = ModBridgeJson::checkJoin(friendsView(), *uuid,
												 instance->gameVersion());
	return response(answer.ok ? Http::kOk : Http::kConflict,
					ModBridgeJson::joinBody(answer));
}

QByteArray ModBridge::face(const QString& uuid)
{
	const QString path = m_source ? m_source->facePath(uuid) : QString();
	QFile file(path);
	if (path.isEmpty() || !file.open(QIODevice::ReadOnly)) {
		return errorResponse(Http::kNotFound, "no_face");
	}
	return response(Http::kOk, file.read(kMaxFaceBytes), kPngType);
}
