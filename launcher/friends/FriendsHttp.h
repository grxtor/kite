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

#include <QObject>
#include <QUrl>

#include <functional>

#include "FriendsApi.h"

/*
 * One JSON request over the launcher's QNetworkAccessManager, answered once
 * with the status and body (or status 0 when nothing came back). It knows
 * nothing about the friends API; FriendsController does.
 */
class FriendsHttp : public QObject
{
	Q_OBJECT

  public:
	using Done = std::function<void(const FriendsApi::Reply&)>;

	explicit FriendsHttp(QObject* parent = nullptr);

	/// Sends @p verb to @p url with @p body (JSON, may be empty) and, when
	/// @p bearer is not empty, an Authorization header. @p done runs on the
	/// main thread, and not at all once this object is gone.
	void send(const QByteArray& verb, const QUrl& url, const QByteArray& body,
			  const QString& bearer, Done done);

	/// Like send(), but spins the event loop until the answer comes or
	/// @p timeoutMs passes. For the moment the launcher quits.
	void sendAndWait(const QByteArray& verb, const QUrl& url,
					 const QByteArray& body, const QString& bearer,
					 int timeoutMs);
};
