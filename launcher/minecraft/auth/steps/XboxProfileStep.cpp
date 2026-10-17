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

#include "XboxProfileStep.h"

#include <QNetworkRequest>
#include <QUrlQuery>

#include "Logging.h"

#include "minecraft/auth/AuthRequest.h"
#include "minecraft/auth/Parsers.h"

XboxProfileStep::XboxProfileStep(AccountData* data) : AuthStep(data) {}

XboxProfileStep::~XboxProfileStep() noexcept = default;

QString XboxProfileStep::describe()
{
	return tr("Fetching Xbox profile.");
}

void XboxProfileStep::rehydrate()
{
	// NOOP, for now. We only save bools and there's nothing to check.
}

void XboxProfileStep::perform()
{
	auto url = QUrl("https://profile.xboxlive.com/users/me/profile/settings");
	QUrlQuery q;
	q.addQueryItem(
		"settings",
		"GameDisplayName,AppDisplayName,AppDisplayPicRaw,GameDisplayPicRaw,"
		"PublicGamerpic,ShowUserAsAvatar,Gamerscore,Gamertag,ModernGamertag,"
		"ModernGamertagSuffix,"
		"UniqueModernGamertag,AccountTier,TenureLevel,XboxOneRep,"
		"PreferredColor,Location,Bio,Watermarks,"
		"RealName,RealNameOverride,IsQuarantined");
	url.setQuery(q);

	QNetworkRequest request = QNetworkRequest(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("Accept", "application/json");
	request.setRawHeader("x-xbl-contract-version", "3");
	request.setRawHeader("Authorization",
						 QString("XBL3.0 x=%1;%2")
							 .arg(m_data->userToken.extra["uhs"].toString(),
								  m_data->xboxApiToken.token)
							 .toUtf8());
	AuthRequest* requestor = new AuthRequest(this);
	connect(requestor, &AuthRequest::finished, this,
			&XboxProfileStep::onRequestDone);
	requestor->get(request);
	qCDebug(minecraftauthLog) << "Getting Xbox profile...";
}

void XboxProfileStep::onRequestDone(QNetworkReply::NetworkError error,
									QByteArray data,
									QList<QNetworkReply::RawHeaderPair>)
{
	auto requestor = qobject_cast<AuthRequest*>(QObject::sender());
	requestor->deleteLater();

	if (error != QNetworkReply::NoError) {
		qWarning() << "Reply error:" << error;
#ifndef NDEBUG
		qCDebug(minecraftauthLog) << Parsers::redactTokens(data);
#endif
		finished(AccountTaskState::STATE_FAILED_SOFT,
				 tr("Failed to retrieve the Xbox profile."));
		return;
	}

#ifndef NDEBUG
	qCDebug(minecraftauthLog) << "XBox profile: " << Parsers::redactTokens(data);
#endif

	emit finished(AccountTaskState::STATE_WORKING, tr("Got Xbox profile"));
}
