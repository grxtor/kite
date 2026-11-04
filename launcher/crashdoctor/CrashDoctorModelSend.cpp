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

#include "CrashDoctorModel.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>

#include "BuildConfig.h"
#include "CrashDoctorService.h"
#include "CrashFollowUp.h"
#include "CrashOutbox.h"
#include "CrashPayload.h"
#include "core/LauncherContext.h"
#include "minecraft/MinecraftInstance.h"
#include "settings/SettingsObject.h"

// Sending a crash to Kite, and what the strip says about crashes read again
// (CrashFollowUp). The rest of the model is CrashDoctorModel.cpp.

namespace
{
	const QString kServerUrlSetting = QStringLiteral("FriendsServerUrl");

	/// Whether this build and these settings have a server to send to.
	bool hasServer()
	{
		auto* context = LAUNCHER;
		return context && !context->settings()->get(kServerUrlSetting).toString().trimmed().isEmpty();
	}

	CrashOutbox::State stateOf(const std::optional<CrashDoctor::Report>& report)
	{
		return report && !report->signature.isEmpty()
				   ? CrashDoctorService::instance()->followUp()->outbox()->state(report->signature)
				   : CrashOutbox::State::None;
	}
} // namespace

bool CrashDoctorModel::canSend() const
{
	const auto report = CrashDoctorService::instance()->report(m_id);
	return report && hasServer() && CrashPayload::sendable(*report) && !report->solvedLaterFor.isValid() &&
		   stateOf(report) == CrashOutbox::State::None;
}

QString CrashDoctorModel::sendState() const
{
	switch (stateOf(CrashDoctorService::instance()->report(m_id))) {
		case CrashOutbox::State::Sending:
			return QStringLiteral("sending");
		case CrashOutbox::State::Queued:
			return QStringLiteral("queued");
		case CrashOutbox::State::Sent:
			return QStringLiteral("sent");
		case CrashOutbox::State::Failed:
			return QStringLiteral("failed");
		case CrashOutbox::State::None:
			break;
	}
	return QString();
}

bool CrashDoctorModel::autoSent() const
{
	const auto report = CrashDoctorService::instance()->report(m_id);
	return report && CrashDoctorService::instance()->followUp()->autoSent(report->signature);
}

QVariantMap CrashDoctorModel::payload() const
{
	const auto report = CrashDoctorService::instance()->report(m_id);
	if (!report || !CrashPayload::sendable(*report)) {
		return {};
	}
	return CrashPayload::readable(CrashPayload::build(*report, BuildConfig.printableVersionString()));
}

bool CrashDoctorModel::fixDidNotHelp() const
{
	const auto report = CrashDoctorService::instance()->report(m_id);
	return report && report->fixDidNotHelp;
}

bool CrashDoctorModel::solvedLater() const
{
	const auto report = CrashDoctorService::instance()->report(m_id);
	return report && report->solvedLaterFor.isValid();
}

void CrashDoctorModel::send(bool alwaysFromNow)
{
	CrashDoctorService::instance()->send(m_id, alwaysFromNow);
}

void CrashDoctorModel::noteFixTried(int causeIndex)
{
	const auto report = CrashDoctorService::instance()->report(m_id);
	if (!m_instance || !report || causeIndex < 0 || causeIndex >= report->causes.size()) {
		return;
	}
	const QJsonObject lastFix{{"rule", report->causes.at(causeIndex).ruleId},
							  {"at", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}};
	m_instance->settings()->set("CrashDoctorLastFix",
								QString::fromUtf8(QJsonDocument(lastFix).toJson(QJsonDocument::Compact)));
}
