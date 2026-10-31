/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
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

#include "net/JsonPost.h"

#include <QDebug>
#include <QNetworkRequest>

#include <utility>

#include "BuildConfig.h"
#include "modplatform/flame/FlameApi.h"
#include "core/LauncherContext.h"

namespace Net
{
	JsonPost::JsonPost(QString name, QUrl url, QByteArray body,
					   QObject* parent)
		: Task(parent), m_name(std::move(name)), m_url(std::move(url)),
		  m_body(std::move(body))
	{
	}

	void JsonPost::executeTask()
	{
		setStatus(m_name);
		/* Indeterminate: the duration is the server's to decide, and 0% reads as
		 * stuck. */
		setProgress(0, 0);

		QNetworkRequest request(m_url);
		/* The uncached identity: never served from or put in a cache; the
		 * answer describes the user's files right now. */
		request.setHeader(QNetworkRequest::UserAgentHeader,
						  BuildConfig.USER_AGENT_UNCACHED);
		request.setHeader(QNetworkRequest::ContentTypeHeader,
						  QStringLiteral("application/json"));
		request.setRawHeader("Accept", "application/json");

		/* The same condition as Net::Download. */
		if (!BuildConfig.CURSEFORGE_API_KEY.isEmpty() &&
			m_url.host() == FlameApi::apiHost()) {
			request.setRawHeader("x-api-key",
								 BuildConfig.CURSEFORGE_API_KEY.toUtf8());
		}

		m_reply.reset(LAUNCHER->network()->post(request, m_body));
		connect(m_reply.get(), &QNetworkReply::finished, this,
				&JsonPost::requestFinished);
	}

	bool JsonPost::abort()
	{
		m_aborted = true;
		if (m_reply) {
			/* Triggers finished(), where the single verdict is emitted. */
			m_reply->abort();
			return true;
		}

		emitAborted();
		return true;
	}

	void JsonPost::requestFinished()
	{
		if (m_aborted) {
			m_reply.reset();
			emitAborted();
			return;
		}

		const QNetworkReply::NetworkError error = m_reply->error();
		const QString errorString = m_reply->errorString();
		m_response = m_reply->readAll();
		m_reply.reset();

		if (error != QNetworkReply::NoError) {
			qWarning() << m_name << "failed:" << errorString;
			emitFailed(errorString);
			return;
		}

		emitSucceeded();
	}
} // namespace Net
