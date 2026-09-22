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

#include "SkinDelete.h"

#include <QNetworkRequest>
#include <QHttpMultiPart>

#include "core/LauncherContext.h"

SkinDelete::SkinDelete(QObject* parent, QString token)
	: Task(parent), m_token(token)
{
}

void SkinDelete::executeTask()
{
	QNetworkRequest request(QUrl(
		"https://api.minecraftservices.com/minecraft/profile/skins/active"));
	request.setRawHeader("Authorization",
						 QString("Bearer %1").arg(m_token).toLocal8Bit());
	QNetworkReply* rep = LAUNCHER->network()->deleteResource(request);
	m_reply = shared_qobject_ptr<QNetworkReply>(rep);

	setStatus(tr("Deleting skin"));
	connect(rep, &QNetworkReply::uploadProgress, this, &Task::setProgress);
	connect(rep, &QNetworkReply::errorOccurred, this,
			&SkinDelete::downloadError);
	connect(rep, &QNetworkReply::finished, this, &SkinDelete::downloadFinished);
}

void SkinDelete::downloadError(QNetworkReply::NetworkError error)
{
	// error happened during download.
	qCritical() << "Network error: " << error;
	emitFailed(m_reply->errorString());
}

void SkinDelete::downloadFinished()
{
	// if the download failed
	if (m_reply->error() != QNetworkReply::NetworkError::NoError) {
		emitFailed(QString("Network error: %1").arg(m_reply->errorString()));
		m_reply.reset();
		return;
	}
	emitSucceeded();
}
