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

#include "MinecraftProfileStep.h"

#include <QNetworkRequest>

#include "Logging.h"

#include "minecraft/auth/AuthRequest.h"
#include "minecraft/auth/Parsers.h"

MinecraftProfileStep::MinecraftProfileStep(AccountData* data) : AuthStep(data)
{
}

MinecraftProfileStep::~MinecraftProfileStep() noexcept = default;

QString MinecraftProfileStep::describe()
{
	return tr("Fetching the Minecraft profile.");
}

void MinecraftProfileStep::perform()
{
	auto url = QUrl("https://api.minecraftservices.com/minecraft/profile");
	QNetworkRequest request = QNetworkRequest(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader(
		"Authorization",
		QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8());

	AuthRequest* requestor = new AuthRequest(this);
	connect(requestor, &AuthRequest::finished, this,
			&MinecraftProfileStep::onRequestDone);
	requestor->get(request);
}

void MinecraftProfileStep::rehydrate()
{
	// NOOP, for now. We only save bools and there's nothing to check.
}

void MinecraftProfileStep::onRequestDone(QNetworkReply::NetworkError error,
										 QByteArray data,
										 QList<QNetworkReply::RawHeaderPair>)
{
	auto requestor = qobject_cast<AuthRequest*>(QObject::sender());
	requestor->deleteLater();

#ifndef NDEBUG
	qCDebug(minecraftauthLog) << Parsers::redactTokens(data);
#endif
	if (error == QNetworkReply::ContentNotFoundError) {
		// NOTE: Succeed even if we do not have a profile. This is a valid
		// account state.
		m_data->minecraftProfile = MinecraftProfile();
		emit finished(AccountTaskState::STATE_SUCCEEDED,
					  tr("Account has no Minecraft profile."));
		return;
	}
	if (error != QNetworkReply::NoError) {
		qWarning() << "Error getting profile:";
		qWarning() << " HTTP Status:        " << requestor->httpStatus_;
		qWarning() << " Internal error no.: " << error;
		qWarning() << " Error string:       " << requestor->errorString_;

		qWarning() << " Response:";
		qWarning() << Parsers::redactTokens(data);

		emit finished(AccountTaskState::STATE_FAILED_SOFT,
					  tr("Minecraft Java profile acquisition failed."));
		return;
	}
	if (!Parsers::parseMinecraftProfile(data, m_data->minecraftProfile)) {
		m_data->minecraftProfile = MinecraftProfile();
		emit finished(
			AccountTaskState::STATE_FAILED_SOFT,
			tr("Minecraft Java profile response could not be parsed"));
		return;
	}

	emit finished(AccountTaskState::STATE_WORKING,
				  tr("Minecraft Java profile acquisition succeeded."));
}
