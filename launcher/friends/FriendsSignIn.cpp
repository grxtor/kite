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
 * FriendsController's sign-in: the same three steps a Minecraft server takes
 * to know who is joining (server/friends/README.md, "Signing in"). The
 * access token only ever goes to Mojang's session server.
 */

#include "FriendsController.h"

#include <QPointer>

#include "FriendsSessionStore.h"
#include "core/LauncherContext.h"
#include "minecraft/auth/AccountData.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "settings/SettingsObject.h"

namespace
{
	/* Mojang may not know the join yet when the login asks: retried after a
	 * second, this many attempts in all. */
	constexpr int kLoginAttempts = 3;
	constexpr int kNotJoinedRetryMs = 1000;
	constexpr int kMojangNoContent = 204;
	constexpr int kHttpUnauthorized = 401;
	constexpr int kHttpForbidden = 403;
	constexpr int kHttpTooManyRequests = 429;
	constexpr int kHttpServerError = 500;
	/* A refused sign-in is not retried (refused()); if it is, it counts as this
	 * many failures already. */
	constexpr int kRefusedSignInFailures = 4;
	/* Ticks are a minute apart and never exact; this much early is on time. */
	constexpr int kBackoffSlackMs = 30 * 1000;
} // namespace

void FriendsController::ensureSession(SessionDone done)
{
	if (sessionUsable()) {
		done(true);
		return;
	}
	m_waiters.push_back(std::move(done));
	if (!m_signingIn) {
		startSignIn();
	}
}

void FriendsController::finishSignIn(bool ok)
{
	m_signingIn = false;
	m_awaitingRenewal = false;
	m_renewalTimer.stop();
	m_renewalTimedOut = false;
	m_quietSignIn = false;
	m_renewalAsked = false;
	if (ok) {
		m_signInFailures = 0;
	}
	auto waiters = std::move(m_waiters);
	m_waiters.clear();
	for (auto& waiter : waiters) {
		waiter(ok);
	}
}

void FriendsController::failSignIn(const QString& message, bool needsSignIn)
{
	setState(State::Error, message, needsSignIn);
	++m_signInFailures;
	/* The account the sign-in started with, not the default now; only a change to
	 * it is news (onAccountsChanged). */
	m_failedIdentity = m_signInTarget.identity;
	finishSignIn(false);
}

void FriendsController::refuseAccount(const QString& message)
{
	/* Set first, so the view that hears of the error knows it is final. */
	m_refusedProfile = m_signInTarget.profileId;
	m_refusedText = message;
	failSignIn(message);
	m_signInFailures = kRefusedSignInFailures;
	/* The error may already be shown (a refused retry); views still have to hear it is final. */
	emit changed();
}

bool FriendsController::signInBackedOff() const
{
	if (m_signInFailures == 0 || !m_lastSignInAttempt.isValid()) {
		return false;
	}
	return m_lastSignInAttempt.elapsed() <
		   FriendsApi::signInBackoffMs(m_signInFailures) - kBackoffSlackMs;
}

bool FriendsController::waitForRenewedToken()
{
	auto accounts = LAUNCHER->accounts();
	auto account = accounts ? accounts->defaultAccount() : nullptr;
	if (!account) {
		return false;
	}
	const QDateTime expiry = account->accountData()->yggdrasilToken.notAfter;
	const QDateTime now = QDateTime::currentDateTimeUtc();
	auto step = [&]() {
		return FriendsApi::renewalStep(
			expiry, now, account->isActive(),
			accounts->isRefreshQueued(account->internalId()), m_renewalAsked,
			m_renewalTimedOut);
	};
	if (step() == FriendsApi::RenewalStep::AskForRenewal) {
		m_renewalAsked = true;
		accounts->requestRefresh(account->internalId());
	}
	/* Asked again after asking: the account is refreshing, still queued, or ran
	 * and this is what it left. */
	return step() == FriendsApi::RenewalStep::Wait;
}

void FriendsController::resumeSignIn()
{
	if (waitForRenewedToken()) {
		return;
	}
	m_awaitingRenewal = false;
	m_renewalTimer.stop();
	startSignIn();
}

void FriendsController::onRenewalTimeout()
{
	if (!m_awaitingRenewal) {
		return;
	}
	/* Whatever holds the renewal up, signing in with the current token can only say so. */
	m_renewalTimedOut = true;
	m_awaitingRenewal = false;
	startSignIn();
}

void FriendsController::signOutQuietly(const QString& token)
{
	m_http->send("POST", FriendsApi::url(serverUrl(), FriendsApi::Path::logout()),
				 QByteArray(), token, [](const FriendsApi::Reply&) {});
}

bool FriendsController::dropIfAccountChanged(const QString& sessionToken)
{
	auto accounts = LAUNCHER ? LAUNCHER->accounts() : nullptr;
	auto account = accounts ? accounts->defaultAccount() : nullptr;
	if (account && account->internalId() == m_signInTarget.internalId &&
		account->profileId() == m_signInTarget.profileId) {
		return false;
	}
	/* The default account changed mid sign-in (onAccountsChanged() leaves one under
	 * way alone): what comes back is about the previous account, so none of it is
	 * kept or counted against the new one. Sign in again for the new default;
	 * whoever waits for a session keeps waiting. */
	if (!sessionToken.isEmpty()) {
		signOutQuietly(sessionToken);
	}
	startSignIn();
	return true;
}

void FriendsController::startSignIn()
{
	m_signingIn = true;
	m_lastSignInAttempt.restart();
	auto accounts = LAUNCHER->accounts();
	auto account = accounts ? accounts->defaultAccount() : nullptr;
	if (!account || !account->isMSA() || !account->hasProfile()) {
		if (!m_session.token.isEmpty()) {
			signOutQuietly(m_session.token);
			clearSession();
		}
		dropDataOfOtherAccount(QString());
		setState(State::NoAccount,
				 tr("Friends needs a Minecraft account that is signed in with "
					"Microsoft."),
				 true);
		finishSignIn(false);
		return;
	}
	/* This outcome is about this account even if another becomes default (dropIfAccountChanged()). */
	m_signInTarget = {account->internalId(), account->profileId(),
					  accountIdentity()};
	/* Not the lists of the previous default, whatever becomes of this sign-in. */
	dropDataOfOtherAccount(account->profileId());
	if (account->accessToken().isEmpty()) {
		failSignIn(tr("Your Minecraft account needs to sign in again."), true);
		return;
	}
	if (stillRefused()) {
		/* Mojang said no and nothing has changed: do not ask again (refresh() and a new default clear it). */
		setState(State::Error, m_refusedText);
		finishSignIn(false);
		return;
	}
	/* The launcher renews the access token as it starts, and Mojang refuses the old
	 * one; "sign in again" would be wrong to say. Wait for the renewal
	 * (onAccountsChanged()), but not for ever. */
	if (waitForRenewedToken()) {
		if (!m_quietSignIn) {
			setState(State::SigningIn);
		}
		m_awaitingRenewal = true;
		if (!m_renewalTimer.isActive()) {
			m_renewalTimer.start();
		}
		return;
	}
	if (!m_session.token.isEmpty()) {
		signOutQuietly(m_session.token);
		clearSession();
	}
	if (!m_quietSignIn) {
		setState(State::SigningIn);
	}
	const int epoch = m_epoch;
	m_http->send(
		"POST", FriendsApi::url(serverUrl(), FriendsApi::Path::challenge()),
		FriendsApi::challengeBody(account->profileName()), QString(),
		[this, epoch, account](const FriendsApi::Reply& reply) {
			if (epoch != m_epoch) {
				return;
			}
			noteRateLimit(reply);
			if (dropIfAccountChanged()) {
				return;
			}
			const auto challenge = reply.ok()
									   ? FriendsApi::parseChallenge(reply.body)
									   : std::nullopt;
			if (!challenge) {
				failSignIn(failureText(reply));
				return;
			}
			joinMojang(*challenge, account->accessToken(),
					   account->profileId());
		});
}

void FriendsController::joinMojang(const FriendsApi::Challenge& challenge,
								   const QString& accessToken,
								   const QString& profileId)
{
	const int epoch = m_epoch;
	m_http->send(
		"POST", FriendsApi::mojangJoinUrl(mojangUrl()),
		FriendsApi::mojangJoinBody(accessToken, profileId, challenge.serverId),
		QString(), [this, epoch, challenge](const FriendsApi::Reply& reply) {
			if (epoch != m_epoch) {
				return;
			}
			noteRateLimit(reply);
			if (dropIfAccountChanged()) {
				return;
			}
			if (reply.networkFailed()) {
				failSignIn(tr("Minecraft's session server can't be reached."));
			} else if (reply.status != kMojangNoContent) {
				failMojangJoin(reply);
			} else {
				login(challenge, 1);
			}
		});
}

void FriendsController::failMojangJoin(const FriendsApi::Reply& reply)
{
	const QString error = FriendsApi::parseMojangError(reply.body);
	qWarning() << "Friends: Mojang's session server refused the join with"
			   << reply.status << error;
	if (error == FriendsApi::MojangError::InsufficientPrivileges) {
		/* Not the token's fault: no sign-in fixes it, and asking every minute adds to Mojang's load. */
		refuseAccount(
			tr("Multiplayer is turned off for this Microsoft account."));
	} else if (error == FriendsApi::MojangError::UserBanned) {
		refuseAccount(tr("This Minecraft account is banned from multiplayer."));
	} else if (reply.status == kHttpUnauthorized ||
			   reply.status == kHttpForbidden) {
		failSignIn(tr("Minecraft did not accept the sign-in; sign in to your "
					  "account again."),
				   true);
	} else if (reply.status == kHttpTooManyRequests ||
			   reply.status >= kHttpServerError) {
		failSignIn(tr("Minecraft's session server is not answering; try again "
					  "later."));
	} else {
		failSignIn(tr("Something went wrong (Minecraft %1).").arg(reply.status));
	}
}

void FriendsController::login(const FriendsApi::Challenge& challenge,
							  int attempt)
{
	const int epoch = m_epoch;
	m_http->send(
		"POST", FriendsApi::url(serverUrl(), FriendsApi::Path::login()),
		FriendsApi::loginBody(challenge.challengeId), QString(),
		[this, epoch, challenge, attempt](const FriendsApi::Reply& reply) {
			if (epoch != m_epoch) {
				return;
			}
			noteRateLimit(reply);
			const auto session =
				reply.ok() ? FriendsApi::parseSession(reply.body) : std::nullopt;
			if (dropIfAccountChanged(session ? session->token : QString())) {
				return;
			}
			if (!reply.ok()) {
				const bool notJoined =
					FriendsApi::parseError(reply.body).code ==
					FriendsApi::Code::NotJoined;
				if (notJoined && attempt < kLoginAttempts) {
					QTimer::singleShot(kNotJoinedRetryMs, this,
									   [this, epoch, challenge, attempt]() {
										   if (epoch == m_epoch) {
											   login(challenge, attempt + 1);
										   }
									   });
					return;
				}
				failSignIn(notJoined ? tr("Minecraft did not confirm the "
										  "sign-in; try again in a moment.")
									 : failureText(reply));
				return;
			}
			if (!session) {
				failSignIn(failureText(reply));
				return;
			}
			setSession(*session);
			setState(State::Ready);
			finishSignIn(true);
		});
}
