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

/*
 * FriendsController's requests: sending one with the token, the minute's
 * poll (status, then friends), and what a failure does to the state.
 */

#include "FriendsController.h"

#include <QEventLoop>
#include <QPointer>

#include <algorithm>

#include "FriendsDisplay.h"
#include "FriendsGameWatcher.h"
#include "core/LauncherContext.h"
#include "settings/SettingsObject.h"

namespace
{
	/* The status goes out when it changes, but not oftener than this. */
	constexpr int kMinStatusGapMs = 10 * 1000;
	/* A 429 without a Retry-After waits this long; one with it is believed
	 * up to the cap. */
	constexpr int kDefaultBackoffSeconds = 60;
	constexpr int kMaxBackoffSeconds = 60 * 60;
} // namespace

QString FriendsController::unreachableText() const
{
	return tr("Kite's friends server can't be reached.");
}

QString FriendsController::failureText(const FriendsApi::Reply& reply) const
{
	if (!reply.failureText.isEmpty()) {
		return reply.failureText;
	}
	if (reply.networkFailed()) {
		return unreachableText();
	}
	if (reply.status == 429) {
		return tr("Too many requests; try again in a minute.");
	}
	if (reply.status == 401) {
		return tr("Your Kite session expired; try again.");
	}
	if (reply.status == 502) {
		return tr("Minecraft's session server is not answering; try again "
				  "later.");
	}
	return tr("Something went wrong (%1).")
		.arg(FriendsApi::parseError(reply.body).code);
}

void FriendsController::noteRateLimit(const FriendsApi::Reply& reply)
{
	if (reply.status != 429) {
		return;
	}
	const int seconds =
		reply.retryAfterSeconds > 0
			? std::min(reply.retryAfterSeconds, kMaxBackoffSeconds)
			: kDefaultBackoffSeconds;
	m_backoffUntil = QDateTime::currentDateTimeUtc().addSecs(seconds);
}

bool FriendsController::inBackoff() const
{
	return m_backoffUntil.isValid() &&
		   QDateTime::currentDateTimeUtc() < m_backoffUntil;
}

void FriendsController::afterStatusReports(std::function<void()> then)
{
	if (m_statusInFlight == 0) {
		then();
		return;
	}
	m_afterStatus.push_back(std::move(then));
}

void FriendsController::statusReportAnswered()
{
	if (--m_statusInFlight > 0) {
		return;
	}
	auto waiting = std::move(m_afterStatus);
	m_afterStatus.clear();
	for (auto& then : waiting) {
		then();
	}
}

void FriendsController::waitForStatusReports(int timeoutMs)
{
	if (m_statusInFlight == 0) {
		return;
	}
	QEventLoop loop;
	/* The answer may come after the timeout, when the loop is gone. */
	QPointer<QEventLoop> guard(&loop);
	QTimer::singleShot(timeoutMs, &loop, &QEventLoop::quit);
	afterStatusReports([guard]() {
		if (guard) {
			guard->quit();
		}
	});
	loop.exec();
}

void FriendsController::request(const QByteArray& verb, const QString& path,
								const QByteArray& body, FriendsHttp::Done done,
								bool retried, bool keepAnswer)
{
	const int epoch = m_epoch;
	/* Another account is the default: what is held is the one before's, and
	 * this request is for the new one. */
	dropDataOfOtherAccount(defaultProfileId());
	/* It is held for the account asked for from here on, also before that
	 * account has a session: another account becoming the default meanwhile
	 * (while the sign-in waits for a token, say) must drop this request too,
	 * and not let it go out as the new one. */
	if (m_dataProfile.isEmpty()) {
		m_dataProfile = FriendsApi::undashed(defaultProfileId());
	}
	const int accountGen = m_accountGen;
	/* Asked for the account that is the default now: if another one takes
	 * over before the answer (forgetAccountData()), it is about someone else
	 * and must not land in the lists. Answered, not dropped, so whatever
	 * waits for it (the poll, an action) ends. */
	auto accountChanged = [=]() {
		FriendsApi::Reply failed;
		failed.failureText = tr("Your Minecraft account changed; try again.");
		done(failed);
	};
	ensureSession([=](bool signedIn) {
		if (epoch != m_epoch) {
			return;
		}
		if (accountGen != m_accountGen) {
			accountChanged();
			return;
		}
		if (!signedIn) {
			FriendsApi::Reply failed;
			failed.failureText =
				m_error.isEmpty()
					? tr("Still signing in; try again in a moment.")
					: m_error;
			done(failed);
			return;
		}
		/* Counted from here to its answer, whatever becomes of the answer
		 * (afterStatusReports()). */
		const bool statusReport =
			verb == "PUT" && path == FriendsApi::Path::status();
		if (statusReport) {
			++m_statusInFlight;
		}
		m_http->send(
			verb, FriendsApi::url(serverUrl(), path), body, m_session.token,
			[=](const FriendsApi::Reply& reply) {
				if (statusReport) {
					statusReportAnswered();
				}
				if (epoch != m_epoch) {
					return;
				}
				if (accountGen != m_accountGen) {
					/* Gone out for the account before: what it did is done,
					 * but its answer is no list for this one. */
					if (keepAnswer) {
						done(reply);
					} else {
						accountChanged();
					}
					return;
				}
				noteRateLimit(reply);
				if (reply.status == 401 && !retried) {
					/* Expired or revoked: sign in again, once. */
					clearSession();
					emit changed();
					request(verb, path, body, done, true, keepAnswer);
					return;
				}
				done(reply);
			});
	});
}

void FriendsController::softFail(const FriendsApi::Reply& reply)
{
	if (reply.status == 429 || !reply.failureText.isEmpty()) {
		/* Waiting it out; or the sign-in already set the state. */
		return;
	}
	setState(State::Error, failureText(reply));
}

void FriendsController::tick()
{
	if (!m_enabled || inBackoff() || (!sessionUsable() && signInBackedOff())) {
		return;
	}
	if (!sessionUsable() && stillRefused()) {
		/* Mojang refused this account for good: nothing to ask. */
		return;
	}
	if (m_signingIn) {
		/* One is under way (it may be waiting for the account's renewal):
		 * its end polls, and another waiter would only poll twice. */
		return;
	}
	/* A retry of a failed sign-in leaves the error on screen until it
	 * ends (finishSignIn()). */
	m_quietSignIn = m_state == State::Error && !sessionUsable();
	ensureSession([this](bool ok) {
		if (ok) {
			poll();
		}
	});
}

void FriendsController::poll(std::function<void(bool, const QString&)> done)
{
	if (!m_enabled) {
		if (done) {
			done(false, QString());
		}
		return;
	}
	if (m_polling) {
		/* The one under way may have read the lists before the change
		 * that asks for this; read again once it is done. */
		m_pollAgain = true;
		if (done) {
			done(true, QString());
		}
		return;
	}
	m_polling = true;
	const int epoch = m_epoch;
	auto finish = [this, epoch, done](bool ok) {
		if (epoch != m_epoch) {
			return;
		}
		m_polling = false;
		if (ok && m_state == State::Error) {
			setState(State::Ready);
		}
		if (m_pollAgain) {
			m_pollAgain = false;
			poll();
		}
		if (done) {
			done(ok, ok ? QString() : m_error);
		}
	};
	auto afterSettings = [this, finish](bool ok) {
		if (!ok) {
			finish(false);
			return;
		}
		sendStatus([this, finish](bool statusOk) {
			if (!statusOk) {
				finish(false);
				return;
			}
			fetchFriends(finish);
		});
	};
	if (m_settingsLoaded) {
		afterSettings(true);
	} else {
		fetchSettings(afterSettings);
	}
}

void FriendsController::fetchSettings(std::function<void(bool)> done)
{
	request("GET", FriendsApi::Path::me(), QByteArray(),
			[this, done](const FriendsApi::Reply& reply) {
				const auto settings = reply.ok()
										  ? FriendsApi::parseMe(reply.body)
										  : std::nullopt;
				if (!settings) {
					softFail(reply);
					done(false);
					return;
				}
				m_settings = FriendsDisplay::settingsMap(*settings);
				m_settingsLoaded = true;
				emit changed();
				done(true);
			});
}

void FriendsController::sendStatus(std::function<void(bool)> done, bool force)
{
	if (!force && m_lastStatusSent.isValid() &&
		m_lastStatusSent.elapsed() < kMinStatusGapMs) {
		done(true);
		return;
	}
	m_lastStatusSent.restart();
	const auto presence = gamePresence();
	request("PUT", FriendsApi::Path::status(),
			FriendsApi::statusBody(presence),
			[this, done, presence](const FriendsApi::Reply& reply) {
				if (!reply.ok()) {
					softFail(reply);
				} else if (const auto view =
							   FriendsApi::parseStatusReply(reply.body)) {
					/* What friends see of you, from the one who shows it. */
					const auto before = myStatus();
					m_myView = SelfView{presence, *view};
					if (myStatus() != before) {
						emit changed();
					}
				}
				done(reply.ok());
			});
}

void FriendsController::fetchFriends(std::function<void(bool)> done)
{
	request("GET", FriendsApi::Path::friends(), QByteArray(),
			[this, done](const FriendsApi::Reply& reply) {
				const auto snapshot =
					reply.ok() ? FriendsApi::parseFriends(reply.body)
							   : std::nullopt;
				if (!snapshot) {
					softFail(reply);
					done(false);
					return;
				}
				applySnapshot(*snapshot);
				done(true);
			});
}

void FriendsController::applySnapshot(
	const FriendsApi::FriendsSnapshot& snapshot)
{
	/* The status lines carry "5 min ago", so they are worded afresh each
	 * time, and the view hears of it each time. */
	m_loaded = true;
	m_updatedAt = QDateTime::currentDateTimeUtc();
	m_friends = FriendsDisplay::friendList(snapshot.friends,
										   QDateTime::currentDateTimeUtc());
	m_incoming = FriendsDisplay::personList(snapshot.incoming);
	m_outgoing = FriendsDisplay::personList(snapshot.outgoing);
	attachFaces(m_friends);
	attachFaces(m_incoming);
	attachFaces(m_outgoing);
	emit changed();
	fetchFaces();
}

void FriendsController::clearData()
{
	m_friends.clear();
	m_incoming.clear();
	m_outgoing.clear();
	m_blocked.clear();
	m_loaded = false;
	m_updatedAt = QDateTime();
	m_myView.reset();
	m_settings = FriendsDisplay::settingsMap({});
	/* The defaults above are not the account's. */
	m_settingsLoaded = false;
	m_dataProfile.clear();
}

void FriendsController::forgetAccountData()
{
	++m_accountGen;
	clearData();
	/* The new account's settings are read again, and its status goes out at
	 * once instead of after the other account's gap. */
	m_lastStatusSent.invalidate();
	emit changed();
}

void FriendsController::dropDataOfOtherAccount(const QString& profileId)
{
	if (!m_dataProfile.isEmpty() &&
		m_dataProfile != FriendsApi::undashed(profileId)) {
		forgetAccountData();
	}
}

void FriendsController::onPresenceChanged()
{
	if (m_enabled) {
		/* Your own card says what friends see of you. */
		emit changed();
	}
	if (!m_enabled || !sessionUsable() || m_pushTimer.isActive()) {
		return;
	}
	const qint64 elapsed = m_lastStatusSent.isValid()
							   ? m_lastStatusSent.elapsed()
							   : kMinStatusGapMs;
	if (elapsed >= kMinStatusGapMs) {
		pushStatus();
	} else {
		m_pushTimer.start(static_cast<int>(kMinStatusGapMs - elapsed));
	}
}

void FriendsController::pushStatus()
{
	if (!m_enabled || !sessionUsable() || inBackoff()) {
		return;
	}
	sendStatus([](bool) {}, true);
}
