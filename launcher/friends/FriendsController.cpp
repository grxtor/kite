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
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QPointer>

#include "FriendsDisplay.h"
#include "FriendsFaces.h"
#include "FriendsGameWatcher.h"
#include "FriendsSessionStore.h"
#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "settings/SettingsObject.h"

namespace
{
	constexpr int kPollIntervalMs = 60 * 1000;
	constexpr int kFacesCoalesceMs = 150;
	/* While the account plays, every ten seconds: a friend joining a server is told
	 * by the game's mod, and a minute late is too late. Six polls a minute stay
	 * inside the server's limit (120 per account). */
	constexpr int kPlayingPollIntervalMs = 10 * 1000;
	/* The most time telling the server we left may take as the launcher quits. */
	constexpr int kQuitGoOfflineMs = 1500;
	/* A change in the accounts starts no new sign-in sooner than this after the last. */
	constexpr int kMinSignInGapMs = 5000;
	/* A sign-in parked for the account's renewal waits this long, then goes on
	 * with the token it has. */
	constexpr int kRenewalWaitMs = 90 * 1000;
	const QString kSessionFile = QStringLiteral("friends.json");
	const QString kFacesDir = QStringLiteral("cache/friendfaces");
} // namespace

FriendsController::FriendsController(const QString& dataDir, QObject* parent)
	: QObject(parent),
	  m_dataDir(dataDir),
	  m_http(new FriendsHttp(this)),
	  m_faces(new FriendsFaces(QDir(dataDir).filePath(kFacesDir), this))
{
	/* Faces arrive one by one; the lists are rebuilt once for a burst. */
	m_facesTimer.setSingleShot(true);
	m_facesTimer.setInterval(kFacesCoalesceMs);
	connect(m_faces, &FriendsFaces::changed, this, [this]() {
		if (!m_facesTimer.isActive()) {
			m_facesTimer.start();
		}
	});
	connect(&m_facesTimer, &QTimer::timeout, this,
			&FriendsController::onFacesChanged);
	m_settings = FriendsDisplay::settingsMap({});
	m_pollTimer.setInterval(kPollIntervalMs);
	connect(&m_pollTimer, &QTimer::timeout, this, &FriendsController::tick);
	m_pushTimer.setSingleShot(true);
	connect(&m_pushTimer, &QTimer::timeout, this,
			&FriendsController::pushStatus);
	m_renewalTimer.setSingleShot(true);
	m_renewalTimer.setInterval(kRenewalWaitMs);
	connect(&m_renewalTimer, &QTimer::timeout, this,
			&FriendsController::onRenewalTimeout);
	if (QCoreApplication::instance()) {
		connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit,
				this, &FriendsController::onAboutToQuit);
	}
	if (LAUNCHER && LAUNCHER->accounts()) {
		connect(LAUNCHER->accounts().get(), &AccountList::listChanged, this,
				&FriendsController::onAccountsChanged);
		connect(LAUNCHER->accounts().get(), &AccountList::listActivityChanged,
				this, &FriendsController::onAccountActivity);
		connect(LAUNCHER->accounts().get(), &AccountList::defaultAccountChanged,
				this, &FriendsController::onDefaultAccountChanged);
	}
	setUpRealtime();
	if (LAUNCHER && LAUNCHER->settings()->get(kEnabledSetting).toBool()) {
		start();
	}
}

FriendsController::~FriendsController() = default;

bool FriendsController::available()
{
	return LAUNCHER &&
		   !LAUNCHER->settings()->get(kServerUrlSetting).toString().trimmed().isEmpty();
}

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

bool FriendsController::needsSignIn() const
{
	return m_needsSignIn;
}

bool FriendsController::refused() const
{
	return stillRefused();
}

bool FriendsController::loaded() const
{
	return m_loaded;
}

bool FriendsController::settingsLoaded() const
{
	return m_settingsLoaded;
}

qint64 FriendsController::updatedAtMs() const
{
	return m_updatedAt.isValid() ? m_updatedAt.toMSecsSinceEpoch() : 0;
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

FriendsApi::Presence FriendsController::gamePresence() const
{
	/* Only the account Friends signs in as; offline, demo or other-account play is not reported. */
	return m_watcher ? m_watcher->presence(defaultProfileId())
					 : FriendsApi::Presence();
}

QVariantMap FriendsController::myStatus() const
{
	const auto presence = gamePresence();
	if (m_myView && m_myView->sent == presence) {
		return FriendsDisplay::selfStatusFromView(presence, m_myView->status);
	}
	return FriendsDisplay::selfStatusMap(presence, m_settings);
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

void FriendsController::setState(State state, const QString& error,
								 bool needsSignIn)
{
	if (state == m_state && error == m_error &&
		needsSignIn == m_needsSignIn) {
		return;
	}
	m_state = state;
	m_error = error;
	m_needsSignIn = needsSignIn;
	qDebug() << "Friends: state" << this->state() << m_error;
	emit changed();
}

void FriendsController::start()
{
	++m_epoch;
	if (m_deletingOff) {
		/* Turned on while the delete with Friends off was still pending: the new epoch
		 * drops its answer, so say so instead of leaving it unanswered. */
		m_deletingOff = false;
		finishAction("deleteAccount", false,
					 tr("Friends was turned on before the delete finished; "
						"delete again if you still want it"));
	}
	/* Only that delete can have a sign-in under way with Friends off; its answers
	 * are dropped, so nothing else would end it. */
	finishSignIn(false);
	m_enabled = true;
	m_settingsLoaded = false;
	m_signInFailures = 0;
	m_refusedProfile.clear();
	m_session = FriendsSessionStore::load(storePath());
	m_dataProfile = FriendsApi::undashed(m_session.uuid);
	m_watcher = std::make_unique<FriendsGameWatcher>(LAUNCHER->instances());
	connect(m_watcher.get(), &FriendsGameWatcher::changed, this,
			&FriendsController::onPresenceChanged);
	followPlay();
	m_pollTimer.start();
	setState(sessionUsable() ? State::Ready : State::SigningIn);
	emit changed();
	followSession();
	tick();
}

void FriendsController::followPlay()
{
	/* With the socket up, friends' statuses come as they change. */
	const int interval = gamePresence().playing && !m_live ? kPlayingPollIntervalMs
														   : kPollIntervalMs;
	/* setInterval() restarts a running timer: only when it changes. */
	if (m_pollTimer.interval() != interval) {
		m_pollTimer.setInterval(interval);
	}
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
	/* The DELETE below (or the status lifetime) drops the status: turned on again
	 * within the gap, the first report must not be skipped as "too soon". */
	m_lastStatusSent.invalidate();
	const QString token = m_session.token;
	if (tellServer && !token.isEmpty()) {
		/* Off first, then out: the token is gone once signed out. Not while a report is
		 * on its way, which could land after this and put the status back. */
		afterStatusReports([this, token]() {
			if (m_enabled) {
				signOutQuietly(token);
				return;
			}
			m_http->send(
				"DELETE",
				FriendsApi::url(serverUrl(), FriendsApi::Path::status()),
				QByteArray(), token, [this, token](const FriendsApi::Reply&) {
					signOutQuietly(token);
				});
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
	m_dataProfile = FriendsApi::undashed(session.uuid);
	/* A delete with Friends off signs in for that request alone: nothing goes to
	 * disk, where a quit before the answer would leave a valid token behind. */
	if (!m_deletingOff) {
		FriendsSessionStore::save(storePath(), session);
	}
	emit changed();
	followSession();
}

void FriendsController::clearSession()
{
	m_session = {};
	FriendsSessionStore::forget(storePath());
	followSession();
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

QString FriendsController::defaultProfileId() const
{
	auto accounts = LAUNCHER ? LAUNCHER->accounts() : nullptr;
	auto account = accounts ? accounts->defaultAccount() : nullptr;
	return account && account->isMSA() && account->hasProfile()
			   ? account->profileId()
			   : QString();
}

QString FriendsController::accountIdentity() const
{
	auto accounts = LAUNCHER ? LAUNCHER->accounts() : nullptr;
	auto account = accounts ? accounts->defaultAccount() : nullptr;
	if (!account) {
		return QString();
	}
	/* The token is only compared, not kept: hashed. */
	const QByteArray token = QCryptographicHash::hash(
		account->accessToken().toUtf8(), QCryptographicHash::Sha256);
	return account->internalId() + QLatin1Char('|') + account->profileId() +
		   QLatin1Char('|') + account->profileName() + QLatin1Char('|') +
		   QString::fromLatin1(token.toHex());
}

bool FriendsController::stillRefused() const
{
	return !m_refusedProfile.isEmpty() &&
		   m_refusedProfile == defaultProfileId();
}

bool FriendsController::working() const
{
	return m_enabled || m_deletingOff;
}

void FriendsController::onAccountsChanged()
{
	if (!working()) {
		return;
	}
	if (m_awaitingRenewal) {
		resumeSignIn();
		return;
	}
	if (!m_enabled) {
		/* Deleting with Friends off: only its own parked sign-in is ours. */
		return;
	}
	if (m_signingIn) {
		return;
	}
	if (!m_refusedProfile.isEmpty() && !stillRefused()) {
		m_refusedProfile.clear();
		emit changed();
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
	/* The list changes whenever a game starts or stops and an account renews
	 * itself: only a change to what the sign-in failed with is a reason to try at once. */
	if (!FriendsApi::accountChangeRetriesSignIn(
			m_signInFailures, m_failedIdentity, accountIdentity(),
			m_refusedProfile, defaultProfileId())) {
		return;
	}
	m_signInFailures = 0;
	ensureSession([this](bool ok) {
		if (ok) {
			poll();
		}
	});
}

void FriendsController::onDefaultAccountChanged()
{
	if (!working()) {
		return;
	}
	/* The lists and settings on screen are the previous account's; nor does the
	 * gap after the last sign-in apply, that one was the other account's. */
	dropDataOfOtherAccount(defaultProfileId());
	m_lastSignInAttempt.invalidate();
	/* A renewal asked for, or given up on, was the other account's: the new default
	 * gets its own full wait. */
	m_renewalAsked = false;
	m_renewalTimedOut = false;
	if (m_awaitingRenewal) {
		m_renewalTimer.start();
	}
	onAccountsChanged();
}

void FriendsController::onAccountActivity()
{
	/* A renewal that failed offline changes only the activity, and the list ends
	 * its count after this signal (AccountList::accountActivityChanged), so decide
	 * once that is done. */
	if (working() && m_awaitingRenewal) {
		QTimer::singleShot(0, this, [this]() {
			if (working() && m_awaitingRenewal) {
				resumeSignIn();
			}
		});
	}
}

void FriendsController::onAboutToQuit()
{
	if (!m_enabled || m_session.token.isEmpty()) {
		return;
	}
	/* The waits below spin the event loop: a timer firing there must not put a
	 * new status on the wire behind the go-offline DELETE. */
	m_quitting = true;
	m_pollTimer.stop();
	m_pushTimer.stop();
	/* A status report on its way is answered first; both within one budget. */
	QElapsedTimer budget;
	budget.start();
	waitForStatusReports(kQuitGoOfflineMs);
	const int left = kQuitGoOfflineMs - static_cast<int>(budget.elapsed());
	if (left <= 0) {
		return;
	}
	m_http->sendAndWait("DELETE",
						FriendsApi::url(serverUrl(), FriendsApi::Path::status()),
						QByteArray(), m_session.token, left);
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
