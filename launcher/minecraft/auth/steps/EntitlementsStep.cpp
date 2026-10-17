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

#include "EntitlementsStep.h"

#include <QNetworkRequest>
#include <QUuid>

#include "Logging.h"

#include "minecraft/auth/AuthRequest.h"
#include "minecraft/auth/Parsers.h"

EntitlementsStep::EntitlementsStep(AccountData* data) : AuthStep(data) {}

EntitlementsStep::~EntitlementsStep() noexcept = default;

QString EntitlementsStep::describe()
{
	return tr("Determining game ownership.");
}

void EntitlementsStep::perform()
{
	auto uuid = QUuid::createUuid();
	m_entitlementsRequestId = uuid.toString().remove('{').remove('}');
	auto url =
		"https://api.minecraftservices.com/entitlements/license?requestId=" +
		m_entitlementsRequestId;
	QNetworkRequest request = QNetworkRequest(url);
	request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	request.setRawHeader("Accept", "application/json");
	request.setRawHeader(
		"Authorization",
		QString("Bearer %1").arg(m_data->yggdrasilToken.token).toUtf8());
	AuthRequest* requestor = new AuthRequest(this);
	connect(requestor, &AuthRequest::finished, this,
			&EntitlementsStep::onRequestDone);
	requestor->get(request);
	qCDebug(minecraftauthLog) << "Getting entitlements...";
}

void EntitlementsStep::rehydrate()
{
	// NOOP, for now. We only save bools and there's nothing to check.
}

void EntitlementsStep::onRequestDone(QNetworkReply::NetworkError,
									 QByteArray data,
									 QList<QNetworkReply::RawHeaderPair>)
{
	auto requestor = qobject_cast<AuthRequest*>(QObject::sender());
	requestor->deleteLater();

#ifndef NDEBUG
	qCDebug(minecraftauthLog) << Parsers::redactTokens(data);
#endif

	// TODO: check presence of same entitlementsRequestId?
	// TODO: validate JWTs?
	Parsers::parseMinecraftEntitlements(data, m_data->minecraftEntitlement);

	emit finished(AccountTaskState::STATE_WORKING, tr("Got entitlements"));
}
