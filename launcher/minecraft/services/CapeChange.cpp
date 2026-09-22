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

#include "CapeChange.h"

#include <QNetworkRequest>
#include <QHttpMultiPart>

#include "core/LauncherContext.h"

CapeChange::CapeChange(QObject* parent, QString token, QString cape)
	: Task(parent), m_capeId(cape), m_token(token)
{
}

void CapeChange::setCape(QString& cape)
{
	QNetworkRequest request(QUrl(
		"https://api.minecraftservices.com/minecraft/profile/capes/active"));
	auto requestString = QString("{\"capeId\":\"%1\"}").arg(m_capeId);
	request.setRawHeader("Authorization",
						 QString("Bearer %1").arg(m_token).toLocal8Bit());
	QNetworkReply* rep =
		LAUNCHER->network()->put(request, requestString.toUtf8());

	setStatus(tr("Equipping cape"));

	m_reply = shared_qobject_ptr<QNetworkReply>(rep);
	connect(rep, &QNetworkReply::uploadProgress, this, &Task::setProgress);
	connect(rep, &QNetworkReply::errorOccurred, this,
			&CapeChange::downloadError);
	connect(rep, &QNetworkReply::finished, this, &CapeChange::downloadFinished);
}

void CapeChange::clearCape()
{
	QNetworkRequest request(QUrl(
		"https://api.minecraftservices.com/minecraft/profile/capes/active"));
	auto requestString = QString("{\"capeId\":\"%1\"}").arg(m_capeId);
	request.setRawHeader("Authorization",
						 QString("Bearer %1").arg(m_token).toLocal8Bit());
	QNetworkReply* rep = LAUNCHER->network()->deleteResource(request);

	setStatus(tr("Removing cape"));

	m_reply = shared_qobject_ptr<QNetworkReply>(rep);
	connect(rep, &QNetworkReply::uploadProgress, this, &Task::setProgress);
	connect(rep, &QNetworkReply::errorOccurred, this,
			&CapeChange::downloadError);
	connect(rep, &QNetworkReply::finished, this, &CapeChange::downloadFinished);
}

void CapeChange::executeTask()
{
	if (m_capeId.isEmpty()) {
		clearCape();
	} else {
		setCape(m_capeId);
	}
}

void CapeChange::downloadError(QNetworkReply::NetworkError error)
{
	// error happened during download.
	qCritical() << "Network error: " << error;
	emitFailed(m_reply->errorString());
}

void CapeChange::downloadFinished()
{
	// if the download failed
	if (m_reply->error() != QNetworkReply::NetworkError::NoError) {
		emitFailed(QString("Network error: %1").arg(m_reply->errorString()));
		m_reply.reset();
		return;
	}
	emitSucceeded();
}
