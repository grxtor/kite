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

#pragma once

#include <QDateTime>
#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "FriendsApi.h"
#include "FriendsHttp.h"
#include "modbridge/ModBridgeJson.h"

class BaseInstance;
class FriendsFaces;
class FriendsGameWatcher;

/*
 * The friends list, for the QML side (QmlShell.friends).
 *
 * On unless the user turned it off (the FriendsEnabled setting, on by
 * default); while it is off nothing is sent to anyone. On, it signs in to
 * Kite's friends server with the default Minecraft account, then once a
 * minute reports what the player is doing and reads the friends, the
 * requests and their statuses. See server/friends/README.md for the other
 * end and FriendsApi for the messages.
 *
 * Every invokable that talks to the server ends with actionFinished(); a
 * failed poll only sets state/error and is tried again a minute later.
 */
class FriendsController : public QObject
{
	Q_OBJECT

	/// Turning it on signs in and starts polling; off goes offline, signs
	/// out and forgets the token.
	Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
	/// "off" | "noAccount" | "signingIn" | "ready" | "error"
	Q_PROPERTY(QString state READ state NOTIFY changed)
	/// What went wrong (error) or is missing (noAccount), translated; empty
	/// otherwise.
	Q_PROPERTY(QString error READ error NOTIFY changed)
	/// Whether the error is one only signing in to the Minecraft account
	/// again can fix (no Microsoft account, or Minecraft refused it), so the
	/// view offers "Sign in" and not "Try again".
	Q_PROPERTY(bool needsSignIn READ needsSignIn NOTIFY changed)
	/// Whether Mojang refused the default account's sign-in for a reason no
	/// new sign-in changes (it may not play multiplayer, or is banned), and
	/// Kite has stopped asking until another account is the default or
	/// refresh() is called. Home's panel stays out of the way of that; the
	/// Friends page says why.
	Q_PROPERTY(bool refused READ refused NOTIFY changed)
	/// Whether the lists have been read from the server since friends was
	/// turned on: until then they are empty because nothing has arrived,
	/// not because there is no one.
	Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
	/// Whether `settings` are the account's, read from the server: until
	/// then they are the defaults (the server cannot be reached, a sign-in is
	/// under way, the account just changed), which are not what it has.
	Q_PROPERTY(bool settingsLoaded READ settingsLoaded NOTIFY changed)
	/// When the lists were last read, in milliseconds since the epoch, 0 if
	/// they have not been (a view of a failed poll says "as of" this).
	Q_PROPERTY(qint64 updatedAtMs READ updatedAtMs NOTIFY changed)
	Q_PROPERTY(QString myName READ myName NOTIFY changed)
	/// uuid, name, since, state, lastSeenAt, instance, version, worldType,
	/// host, port, joinable, statusText (see FriendsDisplay::friendMap), face.
	/// face (here and in the lists below) is the URL of the person's cached
	/// head, "" until it has been fetched from Mojang (FriendsFaces).
	Q_PROPERTY(QVariantList friends READ friends NOTIFY changed)
	/// uuid, name, createdAt, face
	Q_PROPERTY(QVariantList incoming READ incoming NOTIFY changed)
	Q_PROPERTY(QVariantList outgoing READ outgoing NOTIFY changed)
	Q_PROPERTY(QVariantList blocked READ blocked NOTIFY changed)
	/// visibility ("friends" | "everyone" | "invisible"), hideServer,
	/// shareIpServers, allowJoin
	Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
	/// What your friends can see of you right now: playing, worldType, host,
	/// serverHidden, joinable. What the server answered to the last status
	/// report when it is still the one that matches what the game is doing
	/// (FriendsDisplay::selfStatusFromView), our own reading of the settings
	/// otherwise (selfStatusMap).
	Q_PROPERTY(QVariantMap myStatus READ myStatus NOTIFY changed)
	/// Friends who are not offline.
	Q_PROPERTY(int onlineCount READ onlineCount NOTIFY changed)
	Q_PROPERTY(int incomingCount READ incomingCount NOTIFY changed)

  public:
	/// @p dataDir is where friends.json lives.
	explicit FriendsController(const QString& dataDir,
							   QObject* parent = nullptr);
	~FriendsController() override;

	/// Whether this build has a friends server to talk to: Kite_FRIENDS_URL,
	/// or FriendsServerUrl set by hand. Without one there is no feature.
	static bool available();

	bool enabled() const;
	void setEnabled(bool enabled);
	QString state() const;
	QString error() const;
	bool needsSignIn() const;
	bool refused() const;
	bool loaded() const;
	bool settingsLoaded() const;
	qint64 updatedAtMs() const;
	QString myName() const;
	QVariantList friends() const;
	QVariantList incoming() const;
	QVariantList outgoing() const;
	QVariantList blocked() const;
	QVariantMap settings() const;
	QVariantMap myStatus() const;
	int onlineCount() const;
	int incomingCount() const;

	Q_INVOKABLE void refresh();
	Q_INVOKABLE void sendRequest(const QString& name);
	Q_INVOKABLE void accept(const QString& uuid);
	Q_INVOKABLE void decline(const QString& uuid);
	Q_INVOKABLE void cancelRequest(const QString& uuid);
	Q_INVOKABLE void removeFriend(const QString& uuid);
	Q_INVOKABLE void block(const QString& uuid);
	Q_INVOKABLE void unblock(const QString& uuid);
	Q_INVOKABLE void refreshBlocked();
	Q_INVOKABLE void setSettings(const QVariantMap& partial);
	/// Deletes the account and everything the server holds about it, then
	/// turns the feature off. Works with Friends off too: it signs in for
	/// this one request, reports nothing and leaves Friends off.
	Q_INVOKABLE void deleteAccount();

	// What the Kite mod's bridge needs (FriendsBridge.cpp, modbridge/)
	/// The lists as of the last poll, as the mod is told them. Its rev
	/// changes whenever anything the mod is told does.
	ModBridgeJson::FriendsView bridgeView();
	/// Polls the server now, but not more than once in 10 s however often
	/// it is asked (the mod's refresh button).
	void bridgeRefresh();
	/// The cached face of friend @p uuid, "" when there is none or @p uuid
	/// is not a friend (a request, or a blocked person, is not the mod's
	/// business).
	QString bridgeFacePath(const QString& uuid) const;
	/// The game of @p instance says where the player is, as the mod sees it:
	/// that is what friends are told (FriendsGameWatcher::setReportedWorld).
	void reportWorld(BaseInstance* instance, const FriendsApi::World& world);

  signals:
	void changed();
	void actionFinished(QString action, bool ok, QString message);

  private:
	static constexpr const char* kEnabledSetting = "FriendsEnabled";
	static constexpr const char* kServerUrlSetting = "FriendsServerUrl";
	static constexpr const char* kMojangUrlSetting = "FriendsMojangSessionUrl";

	enum class State { Off, NoAccount, SigningIn, Ready, Error };
	using SessionDone = std::function<void(bool)>;
	using Interpret =
		std::function<QString(const FriendsApi::Reply&, bool* ok)>;

	// Lifecycle (FriendsController.cpp)
	void start();
	void stop(bool tellServer);
	void setState(State state, const QString& error = QString(),
				  bool needsSignIn = false);
	void onAccountsChanged();
	void onDefaultAccountChanged();
	void onAccountActivity();
	void onAboutToQuit();
	void onPresenceChanged();
	/// Polls every ten seconds while the account plays, else every minute.
	void followPlay();
	void pushStatus();
	QString storePath() const;
	QString serverUrl() const;
	QString mojangUrl() const;
	bool sessionUsable() const;
	/// What the game says about the default account's play (the watcher's
	/// presence for its profile).
	FriendsApi::Presence gamePresence() const;
	void setSession(const FriendsApi::Session& session);
	void clearSession();

	// Sign-in (FriendsSignIn.cpp)
	void ensureSession(SessionDone done);
	void startSignIn();
	void joinMojang(const FriendsApi::Challenge& challenge,
					const QString& accessToken, const QString& profileId);
	void login(const FriendsApi::Challenge& challenge, int attempt);
	void finishSignIn(bool ok);
	/// Whether the default account is no longer the one the sign-in under way
	/// started with: then what just came back (and @p sessionToken, a session
	/// it made, which is signed out again) is dropped and the sign-in starts
	/// over for the new default account. Returns true if it did.
	bool dropIfAccountChanged(const QString& sessionToken = QString());
	void failSignIn(const QString& message, bool needsSignIn = false);
	/// Words Mojang's refusal of the join: only a refused token asks for a
	/// new sign-in.
	void failMojangJoin(const FriendsApi::Reply& reply);
	/// A failed sign-in for good: Mojang refused the account itself.
	void refuseAccount(const QString& message);
	/// Whether the default account is the one Mojang refused.
	bool stillRefused() const;
	/// Whether the default account's access token is (about to be) out of
	/// date and is being renewed, so this sign-in has to wait for that.
	bool waitForRenewedToken();
	/// Goes on with a sign-in that waited for the account's renewal.
	void resumeSignIn();
	/// The renewal took too long: signs in with what there is.
	void onRenewalTimeout();
	/// Id, profile and token of the default account, as one string that
	/// changes when any of them does ("" without an account).
	QString accountIdentity() const;
	QString defaultProfileId() const;
	/// Friends is on, or is signing in only to delete the account.
	bool working() const;
	/// Whether the last sign-in failed too recently to try again on this
	/// tick (a failure that repeats is retried less and less often).
	bool signInBackedOff() const;
	void signOutQuietly(const QString& token);

	// Requests and the poll (FriendsPoll.cpp)
	/// @p keepAnswer: once the request has gone out, its answer is passed on
	/// also when another account became the default meanwhile (for a delete,
	/// which has happened by then, whatever the lists do).
	void request(const QByteArray& verb, const QString& path,
				 const QByteArray& body, FriendsHttp::Done done,
				 bool retried = false, bool keepAnswer = false);
	/// Runs @p then now, or once no status report is on its way any more: a
	/// report sent at the same time as the DELETE that takes the status off
	/// can reach the server after it and put the status back for its lifetime.
	void afterStatusReports(std::function<void()> then);
	void statusReportAnswered();
	/// Spins the event loop until no report is on its way or @p timeoutMs.
	void waitForStatusReports(int timeoutMs);
	void noteRateLimit(const FriendsApi::Reply& reply);
	bool inBackoff() const;
	void tick();
	void poll(std::function<void(bool, const QString&)> done = {});
	/// Skipped (as a success) within 10 s of the last one unless @p force.
	void sendStatus(std::function<void(bool)> done, bool force = false);
	void fetchFriends(std::function<void(bool)> done);
	void fetchSettings(std::function<void(bool)> done);
	void softFail(const FriendsApi::Reply& reply);
	QString failureText(const FriendsApi::Reply& reply) const;
	QString unreachableText() const;
	void applySnapshot(const FriendsApi::FriendsSnapshot& snapshot);
	void clearData();
	/// clearData() for a change of account: also lets the settings be read
	/// again and drops the answers still on their way for the one before.
	void forgetAccountData();
	/// forgetAccountData() when the lists and settings held are another
	/// account's than @p profileId (empty: no account).
	void dropDataOfOtherAccount(const QString& profileId);
	/// Sets "face" on every person of @p list from the cache.
	void attachFaces(QVariantList& list) const;
	/// Asks FriendsFaces for the heads of everyone in the four lists.
	void fetchFaces();
	void onFacesChanged();

	// Actions (FriendsActions.cpp)
	/// Sends the request and words its outcome with @p interpret, which
	/// sets *ok and returns the message. A success reads the lists again.
	void act(const QString& action, const QByteArray& verb,
			 const QString& path, const QByteArray& body, Interpret interpret,
			 bool thenRefresh = true);
	/// actionFinished(), and not before the caller has returned.
	void finishAction(const QString& action, bool ok, const QString& message);
	/// Whether an action may start: on, and @p uuid (when given) is one.
	bool canAct(const QString& action, const QString* uuid);
	void loadBlocked(std::function<void(const FriendsApi::Reply&, bool)> done);
	QString nameOf(const QString& uuid) const;
	/// The message of an error answer to a friend request for @p name.
	QString requestErrorText(const FriendsApi::Reply& reply,
							 const QString& name) const;

  private:
	QString m_dataDir;
	FriendsHttp* m_http;
	FriendsFaces* m_faces;
	std::unique_ptr<FriendsGameWatcher> m_watcher;
	QTimer m_pollTimer;
	QTimer m_pushTimer;
	QTimer m_renewalTimer;

	bool m_enabled = false;
	State m_state = State::Off;
	QString m_error;
	bool m_needsSignIn = false;
	/// The friends and requests have been read at least once.
	bool m_loaded = false;
	/// Counts enable/disable, so an answer that arrives after a switch
	/// does not touch the new state.
	int m_epoch = 0;
	/// deleteAccount() with Friends off: the session it signs in with is
	/// for that request alone.
	bool m_deletingOff = false;

	FriendsApi::Session m_session;
	bool m_signingIn = false;
	/// The sign-in is parked until the account has its renewed token.
	bool m_awaitingRenewal = false;
	/// The renewal was asked for during this sign-in; ask once. Per sign-in:
	/// finishSignIn() clears it however the sign-in ended.
	bool m_renewalAsked = false;
	/// The sign-in under way is a retry of a failed one: it keeps showing the
	/// error until it has an answer, instead of "Connecting…" every minute
	/// (until finishSignIn(), also when it waits for an account's renewal).
	bool m_quietSignIn = false;
	/// The wait for the account's renewal has gone on too long.
	bool m_renewalTimedOut = false;
	/// Sign-ins that failed in a row.
	int m_signInFailures = 0;
	/// The default account (accountIdentity()) the last sign-in failed with:
	/// a change of the account list that leaves it alone is no reason to try
	/// again sooner than the schedule says.
	QString m_failedIdentity;
	/// The profile Mojang refused for good, and what it said; empty if none.
	QString m_refusedProfile;
	QString m_refusedText;
	std::vector<SessionDone> m_waiters;
	QElapsedTimer m_lastSignInAttempt;
	/// The default account a sign-in started with (startSignIn()): its
	/// answers are about it, also when another account is the default by the
	/// time they come.
	struct SignInTarget {
		QString internalId;
		QString profileId;
		/// accountIdentity() then.
		QString identity;
	};
	SignInTarget m_signInTarget;
	/// Status reports (PUT) sent and not yet answered, and what waits for
	/// there to be none (afterStatusReports()).
	int m_statusInFlight = 0;
	std::vector<std::function<void()>> m_afterStatus;

	bool m_polling = false;
	bool m_pollAgain = false;
	bool m_settingsLoaded = false;
	/// Whose lists and settings these are (the profile id without dashes of
	/// the session they were read with); empty while it holds none.
	QString m_dataProfile;
	/// Counts changes of account, so an answer asked for the account before
	/// is not taken for the new one's (request()).
	int m_accountGen = 0;
	QElapsedTimer m_lastStatusSent;
	QDateTime m_backoffUntil;

	QVariantList m_friends;
	QVariantList m_incoming;
	QVariantList m_outgoing;
	QVariantList m_blocked;
	QVariantMap m_settings;
	QDateTime m_updatedAt;

	/// What the server answered to the status report @p sent.
	struct SelfView {
		FriendsApi::Presence sent;
		FriendsApi::Status status;
	};
	std::optional<SelfView> m_myView;

	/// What bridgeView() last said apart from its rev and time, and the rev
	/// that went with it.
	QByteArray m_bridgeDigest;
	qint64 m_bridgeRev = 0;
	QElapsedTimer m_lastBridgeRefresh;
};
