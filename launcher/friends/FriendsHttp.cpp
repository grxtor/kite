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

#include "FriendsHttp.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>

#include "BuildConfig.h"
#include "core/LauncherContext.h"

namespace
{
	/* No answer after this long is a failure; a poll that fails is tried
	 * again a minute later. */
	constexpr int kRequestTimeoutMs = 15000;
} // namespace

FriendsHttp::FriendsHttp(QObject* parent) : QObject(parent) {}

void FriendsHttp::send(const QByteArray& verb, const QUrl& url,
					   const QByteArray& body, const QString& bearer,
					   Done done)
{
	auto network = LAUNCHER ? LAUNCHER->network() : nullptr;
	if (!network) {
		done(FriendsApi::Reply());
		return;
	}
	QNetworkRequest request(url);
	request.setTransferTimeout(kRequestTimeoutMs);
	request.setRawHeader("User-Agent", BuildConfig.USER_AGENT.toUtf8());
	request.setRawHeader("Accept", "application/json");
	if (!body.isEmpty()) {
		request.setHeader(QNetworkRequest::ContentTypeHeader,
						  "application/json");
	}
	if (!bearer.isEmpty()) {
		request.setRawHeader("Authorization", "Bearer " + bearer.toUtf8());
	}
	QNetworkReply* reply = network->sendCustomRequest(request, verb, body);
	QPointer<FriendsHttp> self(this);
	connect(reply, &QNetworkReply::finished, this,
			[self, reply, done = std::move(done)]() {
				FriendsApi::Reply answer;
				answer.status =
					reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
						.toInt();
				answer.body = reply->readAll();
				answer.retryAfterSeconds =
					reply->rawHeader("Retry-After").trimmed().toInt();
				reply->deleteLater();
				if (self) {
					done(answer);
				}
			});
}

void FriendsHttp::sendAndWait(const QByteArray& verb, const QUrl& url,
							  const QByteArray& body, const QString& bearer,
							  int timeoutMs)
{
	QEventLoop loop;
	QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
	/* The answer may come after the timeout, when the loop is gone. */
	QPointer<QEventLoop> guard(&loop);
	send(verb, url, body, bearer, [guard](const FriendsApi::Reply&) {
		if (guard) {
			guard->quit();
		}
	});
	loop.exec();
}
