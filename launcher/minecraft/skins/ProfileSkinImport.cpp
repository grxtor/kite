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

#include "ProfileSkinImport.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>

#include "core/LauncherContext.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/Parsers.h"
#include "net/Download.h"
#include "net/NetJob.h"

namespace
{
	const char* const kNameLookupEndpoint =
		"https://api.minecraftservices.com/minecraft/profile/lookup/name/";
	const char* const kSessionProfileEndpoint =
		"https://sessionserver.mojang.com/session/minecraft/profile/";
} // namespace

ProfileSkinImport::ProfileSkinImport(QObject* parent, QString username,
									 QString targetPath)
	: Task(parent), m_username(std::move(username)),
	  m_targetPath(std::move(targetPath))
{
}

bool ProfileSkinImport::abort()
{
	if (m_job) {
		m_job->abort();
	}
	emitAborted();
	return true;
}

void ProfileSkinImport::executeTask()
{
	lookUpUuid();
}

void ProfileSkinImport::lookUpUuid()
{
	setStatus(tr("Looking up %1").arg(m_username));

	m_response.clear();
	m_job = new NetJob(tr("Look up user"), LAUNCHER->network());
	/* Percent-encoded: Mojang names are restricted to word characters today,
	 * but this string comes straight out of a text field. */
	const QString endpoint =
		QString::fromLatin1(kNameLookupEndpoint) +
		QString::fromUtf8(QUrl::toPercentEncoding(m_username));
	m_job->addNetAction(
		Net::Download::makeByteArray(QUrl(endpoint), &m_response));

	connect(m_job.get(), &NetJob::failed, this, [this](QString reason) {
		qCritical() << "Could not look up the UUID for" << m_username << ":"
					<< reason;
		emitFailed(tr("failed to get user UUID"));
	});
	connect(m_job.get(), &NetJob::succeeded, this, [this] {
		QJsonParseError parseError{};
		const QJsonDocument doc =
			QJsonDocument::fromJson(m_response, &parseError);
		if (parseError.error != QJsonParseError::NoError) {
			qWarning() << "The name lookup response is not valid JSON at"
					   << parseError.offset << ":"
					   << parseError.errorString();
			emitFailed(tr("failed to parse get user UUID response"));
			return;
		}

		m_uuid = doc.object().value(QStringLiteral("id")).toString();
		if (m_uuid.isEmpty()) {
			/* A name that nobody owns answers with a body that has no id
			 * rather than with an HTTP error, so this is the branch an
			 * ordinary typo lands in. */
			emitFailed(tr("user id is empty"));
			return;
		}
		fetchProfile();
	});

	m_job->start();
}

void ProfileSkinImport::fetchProfile()
{
	setStatus(tr("Fetching the profile of %1").arg(m_username));

	m_response.clear();
	m_job = new NetJob(tr("Download user profile"), LAUNCHER->network());
	const QString endpoint =
		QString::fromLatin1(kSessionProfileEndpoint) + m_uuid;
	m_job->addNetAction(
		Net::Download::makeByteArray(QUrl(endpoint), &m_response));

	connect(m_job.get(), &NetJob::failed, this, [this](QString reason) {
		qCritical() << "Could not fetch the profile of" << m_username << ":"
					<< reason;
		emitFailed(tr("failed to get user profile"));
	});
	connect(m_job.get(), &NetJob::succeeded, this, [this] {
		MinecraftProfile profile;
		if (!Parsers::parseMojangSessionProfile(m_response, profile)) {
			emitFailed(tr("failed to parse get user profile response"));
			return;
		}

		m_textureUrl = profile.skin.url;
		m_arms = profile.skin.variant.toUpper() == QLatin1String("SLIM")
					 ? SkinEntry::Arms::Slim
					 : SkinEntry::Arms::Classic;
		m_capeId = profile.currentCape;

		if (m_textureUrl.isEmpty()) {
			/* The parser substitutes a default skin URL when the player has
			 * none, so an empty one here means the payload was malformed in
			 * a way the parser accepted. */
			emitFailed(tr("failed to parse get user profile response"));
			return;
		}
		downloadTexture();
	});

	m_job->start();
}

void ProfileSkinImport::downloadTexture()
{
	setStatus(tr("Downloading the skin of %1").arg(m_username));

	m_job = new NetJob(tr("Download user skin"), LAUNCHER->network());
	m_job->addNetAction(
		Net::Download::makeFile(QUrl(m_textureUrl), m_targetPath));

	connect(m_job.get(), &NetJob::failed, this, [this](QString reason) {
		qCritical() << "Could not download the skin of" << m_username << ":"
					<< reason;
		emitFailed(tr("failed to download skin"));
	});
	connect(m_job.get(), &NetJob::succeeded, this,
			[this] { emitSucceeded(); });

	m_job->start();
}
