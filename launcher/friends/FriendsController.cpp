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

#include "FriendsController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>

#include "FriendsDisplay.h"
#include "FriendsFaces.h"
#include "FriendsGameWatcher.h"
#include "FriendsSessionStore.h"
#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "minecraft/auth/AccountList.h"
#include "settings/SettingsObject.h"

namespace
{
	/* The poll: a status and the friends, once a minute. */
	constexpr int kPollIntervalMs = 60 * 1000;
	/* Telling the server we left, as the launcher quits, may take this long
	 * and no longer. */
	constexpr int kQuitGoOfflineMs = 1500;
	/* A change in the accounts does not start a new sign-in sooner than
	 * this after the last one. */
	constexpr int kMinSignInGapMs = 5000;
	const QString kSessionFile = QStringLiteral("friends.json");
	/* The friends' heads, under the data folder. */
	const QString kFacesDir = QStringLiteral("cache/friendfaces");
} // namespace

FriendsController::FriendsController(const QString& dataDir, QObject* parent)
	: QObject(parent),
	  m_dataDir(dataDir),
	  m_http(new FriendsHttp(this)),
	  m_faces(new FriendsFaces(QDir(dataDir).filePath(kFacesDir), this))
{
	connect(m_faces, &FriendsFaces::changed, this,
			&FriendsController::onFacesChanged);
	m_settings = FriendsDisplay::settingsMap({});
	m_pollTimer.setInterval(kPollIntervalMs);
	connect(&m_pollTimer, &QTimer::timeout, this, &FriendsController::tick);
	m_pushTimer.setSingleShot(true);
	connect(&m_pushTimer, &QTimer::timeout, this,
			&FriendsController::pushStatus);
	if (QCoreApplication::instance()) {
		connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
				this, &FriendsController::onAboutToQuit);
	}
	if (LAUNCHER && LAUNCHER->accounts()) {
		connect(LAUNCHER->accounts().get(), &AccountList::listChanged, this,
				&FriendsController::onAccountsChanged);
	}
	if (LAUNCHER && LAUNCHER->settings()->get(kEnabledSetting).toBool()) {
		start();
	}
}

FriendsController::~FriendsController() = default;

QString FriendsController::storePath() const
{
	return QDir(m_dataDir).filePath(kSessionFile);
}

QString FriendsController::serverUrl() const
{
	return LAUNCHER->settings()->get(kServerUrlSetting).toString();
}

QString FriendsController::mojangUrl() const
{
	return LAUNCHER->settings()->get(kMojangUrlSetting).toString();
}

bool FriendsController::enabled() const
{
	return m_enabled;
}

void FriendsController::setEnabled(bool enabled)
{
	if (enabled == m_enabled) {
		return;
	}
	LAUNCHER->settings()->set(kEnabledSetting, enabled);
	if (enabled) {
		start();
	} else {
		stop(true);
		/* Their heads are their data too. */
		m_faces->clearCache();
	}
}

QString FriendsController::state() const
{
	switch (m_state) {
		case State::NoAccount:
			return QStringLiteral("noAccount");
		case State::SigningIn:
			return QStringLiteral("signingIn");
		case State::Ready:
			return QStringLiteral("ready");
		case State::Error:
			return QStringLiteral("error");
		case State::Off:
			break;
	}
	return QStringLiteral("off");
}

QString FriendsController::error() const
{
	return m_error;
}

QString FriendsController::myName() const
{
	return m_session.name;
}

QVariantList FriendsController::friends() const
{
	return m_friends;
}

QVariantList FriendsController::incoming() const
{
	return m_incoming;
}

QVariantList FriendsController::outgoing() const
{
	return m_outgoing;
}

QVariantList FriendsController::blocked() const
{
	return m_blocked;
}

QVariantMap FriendsController::settings() const
{
	return m_settings;
}

int FriendsController::onlineCount() const
{
	int count = 0;
	for (const QVariant& one : m_friends) {
		if (one.toMap().value("state").toString() != "offline") {
			++count;
		}
	}
	return count;
}

int FriendsController::incomingCount() const
{
	return m_incoming.size();
}

void FriendsController::setState(State state, const QString& error)
{
	if (state == m_state && error == m_error) {
		return;
	}
	m_state = state;
	m_error = error;
	qDebug() << "Friends: state" << this->state() << m_error;
	emit changed();
}

void FriendsController::start()
{
	++m_epoch;
	m_enabled = true;
	m_settingsLoaded = false;
	m_session = FriendsSessionStore::load(storePath());
	m_watcher = std::make_unique<FriendsGameWatcher>(LAUNCHER->instances());
	connect(m_watcher.get(), &FriendsGameWatcher::changed, this,
			&FriendsController::onPresenceChanged);
	m_pollTimer.start();
	setState(sessionUsable() ? State::Ready : State::SigningIn);
	emit changed();
	tick();
}

void FriendsController::stop(bool tellServer)
{
	++m_epoch;
	m_enabled = false;
	m_pollTimer.stop();
	m_pushTimer.stop();
	m_faces->cancel();
	m_watcher.reset();
	m_polling = false;
	const QString token = m_session.token;
	if (tellServer && !token.isEmpty()) {
		/* Off first, then out: the token is gone once it is signed out. */
		m_http->send(
			"DELETE", FriendsApi::url(serverUrl(), FriendsApi::Path::status()),
			QByteArray(), token, [this, token](const FriendsApi::Reply&) {
				signOutQuietly(token);
			});
	}
	clearSession();
	clearData();
	finishSignIn(false);
	setState(State::Off);
	emit changed();
}

void FriendsController::setSession(const FriendsApi::Session& session)
{
	m_session = session;
	FriendsSessionStore::save(storePath(), session);
	emit changed();
}

void FriendsController::clearSession()
{
	m_session = {};
	FriendsSessionStore::forget(storePath());
}

bool FriendsController::sessionUsable() const
{
	auto accounts = LAUNCHER ? LAUNCHER->accounts() : nullptr;
	auto account = accounts ? accounts->defaultAccount() : nullptr;
	if (!account || !account->isMSA() || !account->hasProfile()) {
		return false;
	}
	return FriendsSessionStore::usable(m_session, account->profileId(),
									   account->profileName(),
									   QDateTime::currentDateTimeUtc());
}

void FriendsController::onAccountsChanged()
{
	if (!m_enabled || m_signingIn) {
		return;
	}
	if (sessionUsable()) {
		if (m_state == State::NoAccount || m_state == State::SigningIn) {
			setState(State::Ready);
		}
		return;
	}
	if (m_lastSignInAttempt.isValid() &&
		m_lastSignInAttempt.elapsed() < kMinSignInGapMs) {
		return;
	}
	ensureSession([this](bool ok) {
		if (ok) {
			poll();
		}
	});
}

void FriendsController::onAboutToQuit()
{
	if (!m_enabled || m_session.token.isEmpty()) {
		return;
	}
	m_http->sendAndWait("DELETE",
						FriendsApi::url(serverUrl(), FriendsApi::Path::status()),
						QByteArray(), m_session.token, kQuitGoOfflineMs);
}

void FriendsController::attachFaces(QVariantList& list) const
{
	for (QVariant& one : list) {
		QVariantMap person = one.toMap();
		person["face"] = m_faces->faceUrl(person.value("uuid").toString());
		one = person;
	}
}

void FriendsController::fetchFaces()
{
	if (!m_enabled) {
		return;
	}
	QStringList uuids;
	for (const auto& list : {m_friends, m_incoming, m_outgoing, m_blocked}) {
		for (const QVariant& one : list) {
			uuids.append(one.toMap().value("uuid").toString());
		}
	}
	m_faces->ensure(uuids, mojangUrl());
}

void FriendsController::onFacesChanged()
{
	attachFaces(m_friends);
	attachFaces(m_incoming);
	attachFaces(m_outgoing);
	attachFaces(m_blocked);
	emit changed();
}
