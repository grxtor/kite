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

#include "SkinUpload.h"

#include <QNetworkRequest>
#include <QHttpMultiPart>

#include "core/LauncherContext.h"

QByteArray getVariant(SkinUpload::Model model)
{
	switch (model) {
		default:
			qDebug() << "Unknown skin type!";
		case SkinUpload::STEVE:
			return "CLASSIC";
		case SkinUpload::ALEX:
			return "SLIM";
	}
}

SkinUpload::SkinUpload(QObject* parent, QString token, QByteArray skin,
					   SkinUpload::Model model)
	: Task(parent), m_model(model), m_skin(skin), m_token(token)
{
}

void SkinUpload::executeTask()
{
	QNetworkRequest request(
		QUrl("https://api.minecraftservices.com/minecraft/profile/skins"));
	request.setRawHeader("Authorization",
						 QString("Bearer %1").arg(m_token).toLocal8Bit());
	QHttpMultiPart* multiPart =
		new QHttpMultiPart(QHttpMultiPart::FormDataType);

	QHttpPart skin;
	skin.setHeader(QNetworkRequest::ContentTypeHeader, QVariant("image/png"));
	skin.setHeader(QNetworkRequest::ContentDispositionHeader,
				   QVariant("form-data; name=\"file\"; filename=\"skin.png\""));
	skin.setBody(m_skin);

	QHttpPart model;
	model.setHeader(QNetworkRequest::ContentDispositionHeader,
					QVariant("form-data; name=\"variant\""));
	model.setBody(getVariant(m_model));

	multiPart->append(skin);
	multiPart->append(model);

	QNetworkReply* rep = LAUNCHER->network()->post(request, multiPart);
	m_reply = shared_qobject_ptr<QNetworkReply>(rep);

	setStatus(tr("Uploading skin"));
	connect(rep, &QNetworkReply::uploadProgress, this, &Task::setProgress);
	connect(rep, &QNetworkReply::errorOccurred, this,
			&SkinUpload::downloadError);
	connect(rep, &QNetworkReply::finished, this, &SkinUpload::downloadFinished);
}

void SkinUpload::downloadError(QNetworkReply::NetworkError error)
{
	// error happened during download.
	qCritical() << "Network error: " << error;
	emitFailed(m_reply->errorString());
}

void SkinUpload::downloadFinished()
{
	// if the download failed
	if (m_reply->error() != QNetworkReply::NetworkError::NoError) {
		emitFailed(QString("Network error: %1").arg(m_reply->errorString()));
		m_reply.reset();
		return;
	}
	emitSucceeded();
}
