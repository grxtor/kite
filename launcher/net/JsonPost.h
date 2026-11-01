/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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

#pragma once

#include <QByteArray>
#include <QNetworkReply>
#include <QString>
#include <QUrl>

#include <memory>

#include "tasks/Task.h"

namespace Net
{
	/* One JSON request that has to be a POST, as a task. Net::Download is a
	 * GET with a sink, cache entry and validators, none of which fits a call
	 * whose question is too big for a URL (which versions own these
	 * fingerprints?). So: no caching, no sink, no partial progress.
	 *
	 * The CurseForge key is attached under the same condition as in
	 * Net::Download; it belongs to that API host only, so the rule lives next
	 * to the host check. */
	class JsonPost : public Task
	{
		Q_OBJECT
	  public:
		JsonPost(QString name, QUrl url, QByteArray body,
				 QObject* parent = nullptr);
		~JsonPost() override = default;

		const QByteArray& response() const
		{
			return m_response;
		}

		bool canAbort() const override
		{
			return true;
		}
		bool abort() override;

	  protected:
		void executeTask() override;

	  private slots:
		void requestFinished();

	  private:
		const QString m_name;
		const QUrl m_url;
		const QByteArray m_body;
		QByteArray m_response;

		/* Latched by abort(): Qt still delivers the finished handler for an
		 * aborted reply, which must report the abort, not a network error. */
		bool m_aborted = false;

		std::unique_ptr<QNetworkReply> m_reply;
	};
} // namespace Net
