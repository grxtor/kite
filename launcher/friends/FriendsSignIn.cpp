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
#include "minecraft/auth/AccountList.h"
#include "settings/SettingsObject.h"

namespace
{
	/* Mojang may not know the join yet when the login asks: ask again
	 * after a second, up to this many times in all. */
	constexpr int kLoginAttempts = 3;
	constexpr int kNotJoinedRetryMs = 1000;
	constexpr int kMojangNoContent = 204;
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
	auto waiters = std::move(m_waiters);
	m_waiters.clear();
	for (auto& waiter : waiters) {
		waiter(ok);
	}
}

void FriendsController::failSignIn(const QString& message)
{
	setState(State::Error, message);
	finishSignIn(false);
}

void FriendsController::signOutQuietly(const QString& token)
{
	m_http->send("POST", FriendsApi::url(serverUrl(), FriendsApi::Path::logout()),
				 QByteArray(), token, [](const FriendsApi::Reply&) {});
}

void FriendsController::startSignIn()
{
	m_signingIn = true;
	m_lastSignInAttempt.restart();
	auto accounts = LAUNCHER->accounts();
	auto account = accounts ? accounts->defaultAccount() : nullptr;
	if (!account || !account->isMSA() || !account->hasProfile()) {
		/* An offline account has no Mojang session to prove anything with. */
		if (!m_session.token.isEmpty()) {
			signOutQuietly(m_session.token);
			clearSession();
		}
		setState(State::NoAccount,
				 tr("Friends need a Minecraft account that is signed in with "
					"Microsoft"));
		finishSignIn(false);
		return;
	}
	if (account->accessToken().isEmpty()) {
		failSignIn(tr("Your Minecraft account needs to sign in again"));
		return;
	}
	if (!m_session.token.isEmpty()) {
		/* Another account, a new name or an old token. */
		signOutQuietly(m_session.token);
		clearSession();
	}
	setState(State::SigningIn);
	const int epoch = m_epoch;
	m_http->send(
		"POST", FriendsApi::url(serverUrl(), FriendsApi::Path::challenge()),
		FriendsApi::challengeBody(account->profileName()), QString(),
		[this, epoch, account](const FriendsApi::Reply& reply) {
			if (epoch != m_epoch) {
				return;
			}
			noteRateLimit(reply);
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
			if (reply.networkFailed()) {
				failSignIn(tr("Minecraft's session server can't be reached"));
			} else if (reply.status != kMojangNoContent) {
				failSignIn(tr("Minecraft did not accept the sign-in; sign in "
							  "to your account again"));
			} else {
				login(challenge, 1);
			}
		});
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
										  "sign-in; try again in a moment")
									 : failureText(reply));
				return;
			}
			const auto session = FriendsApi::parseSession(reply.body);
			if (!session) {
				failSignIn(failureText(reply));
				return;
			}
			setSession(*session);
			setState(State::Ready);
			finishSignIn(true);
		});
}
