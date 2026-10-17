/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include "MinecraftAccount.h"

#include <QCryptographicHash>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QStringList>
#include <QJsonDocument>

#include <QDebug>

#include <QPainter>

#include "flows/MSA.h"

MinecraftAccount::MinecraftAccount(QObject* parent) : QObject(parent)
{
	data.internalId =
		QUuid::createUuid().toString().remove(QRegularExpression("[{}-]"));
}

MinecraftAccountPtr MinecraftAccount::loadFromJsonV3(const QJsonObject& json)
{
	MinecraftAccountPtr account(new MinecraftAccount());
	if (account->data.resumeStateFromV3(json)) {
		return account;
	}
	return nullptr;
}

QString MinecraftAccount::gameUuid() const
{
	return isOffline() ? uuidFromUsername(data.offlineUsername)
					   : data.profileId();
}

QString MinecraftAccount::uuidFromUsername(const QString& username)
{
	/* UUID.nameUUIDFromBytes(): the MD5 of the name, with the version
	 * nibble set to 3 and the variant bits to IETF. */
	constexpr int kVersionByte = 6;
	constexpr int kVariantByte = 8;
	QByteArray digest = QCryptographicHash::hash(
		QStringLiteral("OfflinePlayer:%1").arg(username).toUtf8(),
		QCryptographicHash::Md5);
	digest[kVersionByte] = static_cast<char>(
		(static_cast<quint8>(digest[kVersionByte]) & 0x0f) | 0x30);
	digest[kVariantByte] = static_cast<char>(
		(static_cast<quint8>(digest[kVariantByte]) & 0x3f) | 0x80);
	return QUuid::fromRfc4122(digest).toString(QUuid::Id128);
}

MinecraftAccountPtr MinecraftAccount::createBlankMSA()
{
	MinecraftAccountPtr account(new MinecraftAccount());
	account->data.type = AccountType::MSA;
	return account;
}

MinecraftAccountPtr MinecraftAccount::createOffline(const QString& username)
{
	MinecraftAccountPtr account(new MinecraftAccount());
	account->data.type = AccountType::Offline;
	account->data.offlineUsername = username;
	account->data.accountState = AccountState::Online;
	return account;
}

QJsonObject MinecraftAccount::saveToJson() const
{
	return data.saveState();
}

AccountState MinecraftAccount::accountState() const
{
	return data.accountState;
}

QPixmap MinecraftAccount::getFace() const
{
	QPixmap skinTexture;
	if (!skinTexture.loadFromData(data.minecraftProfile.skin.data, "PNG")) {
		return QPixmap();
	}
	QPixmap skin = QPixmap(8, 8);
	QPainter painter(&skin);
	painter.drawPixmap(0, 0, skinTexture.copy(8, 8, 8, 8));
	painter.drawPixmap(0, 0, skinTexture.copy(40, 8, 8, 8));
	return skin.scaled(64, 64, Qt::KeepAspectRatio);
}

shared_qobject_ptr<AccountTask> MinecraftAccount::loginMSA()
{
	Q_ASSERT(m_currentTask.get() == nullptr);

	m_currentTask.reset(new MSAInteractive(&data));
	connect(m_currentTask.get(), &AccountTask::succeeded, this,
			&MinecraftAccount::authSucceeded);
	connect(m_currentTask.get(), &AccountTask::failed, this,
			&MinecraftAccount::authFailed);
	emit activityChanged(true);
	return m_currentTask;
}

shared_qobject_ptr<AccountTask> MinecraftAccount::refresh()
{
	if (m_currentTask) {
		return m_currentTask;
	}

	m_currentTask.reset(new MSASilent(&data));

	connect(m_currentTask.get(), &AccountTask::succeeded, this,
			&MinecraftAccount::authSucceeded);
	connect(m_currentTask.get(), &AccountTask::failed, this,
			&MinecraftAccount::authFailed);
	emit activityChanged(true);
	return m_currentTask;
}

shared_qobject_ptr<AccountTask> MinecraftAccount::currentTask()
{
	return m_currentTask;
}

void MinecraftAccount::authSucceeded()
{
	m_currentTask.reset();
	emit changed();
	emit activityChanged(false);
}

void MinecraftAccount::authFailed(QString)
{
	switch (m_currentTask->taskState()) {
		case AccountTaskState::STATE_OFFLINE:
		case AccountTaskState::STATE_FAILED_SOFT: {
			// NOTE: this doesn't do much. There was an error of some sort.
		} break;
		case AccountTaskState::STATE_FAILED_HARD: {
			data.msaToken.token = QString();
			data.msaToken.refresh_token = QString();
			data.msaToken.validity = Katabasis::Validity::None;
			data.validity_ = Katabasis::Validity::None;
			emit changed();
		} break;
		case AccountTaskState::STATE_FAILED_GONE: {
			data.validity_ = Katabasis::Validity::None;
			emit changed();
		} break;
		case AccountTaskState::STATE_CREATED:
		case AccountTaskState::STATE_WORKING:
		case AccountTaskState::STATE_SUCCEEDED: {
			// Not reachable here, as they are not failures.
		}
	}
	m_currentTask.reset();
	emit activityChanged(false);
}

bool MinecraftAccount::isActive() const
{
	return m_currentTask;
}

bool MinecraftAccount::shouldRefresh() const
{
	/*
	 * Never refresh accounts that are being used by the game, it breaks the
	 * game session. Always refresh accounts that have not been refreshed yet
	 * during this session. Don't refresh broken accounts. Refresh accounts that
	 * would expire in the next 12 hours (fresh token validity is 24 hours).
	 */
	if (isInUse()) {
		return false;
	}
	switch (data.validity_) {
		case Katabasis::Validity::Certain: {
			break;
		}
		case Katabasis::Validity::None: {
			return false;
		}
		case Katabasis::Validity::Assumed: {
			return true;
		}
	}
	auto now = QDateTime::currentDateTimeUtc();
	auto issuedTimestamp = data.msaToken.issueInstant;
	auto expiresTimestamp = data.msaToken.notAfter;

	if (!expiresTimestamp.isValid()) {
		expiresTimestamp = issuedTimestamp.addSecs(24 * 3600);
	}
	if (now.secsTo(expiresTimestamp) < (12 * 3600)) {
		return true;
	}
	return false;
}

bool MinecraftAccount::shouldFetchGameFiles(
	bool wantsOnline, QNetworkInformation::Reachability reachability)
{
	return wantsOnline &&
		   reachability != QNetworkInformation::Reachability::Disconnected;
}

QNetworkInformation::Reachability MinecraftAccount::networkReachability()
{
	// Loaded once; where the platform has no backend there is no answer, and
	// "unknown" is treated as "try".
	static const bool loaded = QNetworkInformation::loadDefaultBackend();
	if (!loaded || !QNetworkInformation::instance()) {
		return QNetworkInformation::Reachability::Unknown;
	}
	return QNetworkInformation::instance()->reachability();
}

void MinecraftAccount::fillSession(AuthSessionPtr session)
{
	if (data.type == AccountType::Offline) {
		session->status = AuthSession::PlayableOffline;
		/* No sign-in, so never "online" -- but the game still has to come
		 * from somewhere. Offline here means "no login", not "no network":
		 * only an explicit offline launch (wants_online was cleared by
		 * LaunchController: Play offline, or skipping the account refresh)
		 * or a missing network keeps the update offline. */
		session->fetch_game_files =
			shouldFetchGameFiles(session->wants_online, networkReachability());
		session->wants_online = false;
		session->player_name = data.offlineUsername;
		session->username = data.offlineUsername;
		session->uuid = gameUuid();
		session->session = "-";
		session->user_type = "legacy";
		return;
	}
	if (ownsMinecraft() && !hasProfile()) {
		session->status = AuthSession::RequiresProfileSetup;
	} else {
		if (session->wants_online) {
			session->status = AuthSession::PlayableOnline;
		} else {
			session->status = AuthSession::PlayableOffline;
		}
	}

	// the user name
	session->username = data.profileName();
	// volatile auth token
	session->access_token = data.accessToken();
	// the semi-permanent client token
	session->client_token = QString();
	// profile name
	session->player_name = data.profileName();
	// profile ID
	session->uuid = data.profileId();
	// 'legacy' or 'mojang', depending on account type
	session->user_type = typeString();
	if (!session->access_token.isEmpty()) {
		session->session =
			"token:" + data.accessToken() + ":" + data.profileId();
	} else {
		session->session = "-";
	}
}

void MinecraftAccount::decrementUses()
{
	Usable::decrementUses();
	if (!isInUse()) {
		emit changed();
		// FIXME: we now need a better way to identify accounts...
		qWarning() << "Profile" << data.profileId() << "is no longer in use.";
	}
}

void MinecraftAccount::incrementUses()
{
	bool wasInUse = isInUse();
	Usable::incrementUses();
	if (!wasInUse) {
		emit changed();
		// FIXME: we now need a better way to identify accounts...
		qWarning() << "Profile" << data.profileId() << "is now in use.";
	}
}
