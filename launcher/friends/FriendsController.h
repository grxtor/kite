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
#include <QJsonObject>
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
#include "FriendsChat.h"
#include "FriendsHttp.h"
#include "FriendsNotices.h"
#include "modbridge/ModBridgeJson.h"

class BaseInstance;
class FriendsFaces;
class FriendsGameWatcher;
class FriendsSocket;

/*
 * The friends list for QML (QmlShell.friends). On unless the FriendsEnabled
 * setting is off; off sends nothing. On, it signs in with the default
 * Minecraft account, polls once a minute and keeps the server's socket open
 * for what happens in between (FriendsRealtime.cpp). See server/friends/README.md.
 * Every invokable that talks to the server ends with actionFinished().
 */
class FriendsController : public QObject
{
	Q_OBJECT

	/// Turning it on signs in and starts polling; off goes offline, signs
	/// out and forgets the token.
	Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
	/// "off" | "noAccount" | "signingIn" | "ready" | "error"
	Q_PROPERTY(QString state READ state NOTIFY changed)
	Q_PROPERTY(QString error READ error NOTIFY changed)
	/// True if only signing in to the Minecraft account again can fix the error ("Sign in", not "Try again").
	Q_PROPERTY(bool needsSignIn READ needsSignIn NOTIFY changed)
	/// Mojang refused the account for good; Kite stops asking until another account is default or refresh().
	Q_PROPERTY(bool refused READ refused NOTIFY changed)
	/// False until the lists have been read: empty then means nothing arrived, not no friends.
	Q_PROPERTY(bool loaded READ loaded NOTIFY changed)
	/// False while `settings` are still the defaults (server unreachable, sign-in under way).
	Q_PROPERTY(bool settingsLoaded READ settingsLoaded NOTIFY changed)
	/// Milliseconds since the epoch of the last read; 0 if never.
	Q_PROPERTY(qint64 updatedAtMs READ updatedAtMs NOTIFY updatedAtChanged)
	Q_PROPERTY(QString myName READ myName NOTIFY changed)
	/// uuid, name, since, state, lastSeenAt, instance, version, worldType, host,
	/// port, joinable, statusText, face. face (here and in the lists below) is the
	/// URL of the cached head, "" until fetched from Mojang (FriendsFaces).
	Q_PROPERTY(QVariantList friends READ friends NOTIFY changed)
	/// uuid, name, createdAt, face
	Q_PROPERTY(QVariantList incoming READ incoming NOTIFY changed)
	Q_PROPERTY(QVariantList outgoing READ outgoing NOTIFY changed)
	Q_PROPERTY(QVariantList blocked READ blocked NOTIFY changed)
	/// visibility ("friends" | "everyone" | "invisible"), hideServer,
	/// shareIpServers, allowJoin, allowInvites
	Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
	/// What friends can see of you: playing, worldType, host, serverHidden,
	/// joinable. The server's last answer while it matches the game, else our
	/// reading of the settings.
	Q_PROPERTY(QVariantMap myStatus READ myStatus NOTIFY changed)
	Q_PROPERTY(int onlineCount READ onlineCount NOTIFY changed)
	Q_PROPERTY(int incomingCount READ incomingCount NOTIFY changed)
	/// The socket to the server is up: events arrive as they happen, and the
	/// poll is only the minute's safety net.
	Q_PROPERTY(bool live READ live NOTIFY changed)
	/// One-to-one chats with friends.
	Q_PROPERTY(FriendsChat* chat READ chat CONSTANT)
	/// Invites to a server you were sent and that have not run out, newest
	/// first: uuid, name, face, host, port, createdAt, expiresAt, expiresAtMs,
	/// addressKind ("domain" | "ip" | "local"; FriendsInvites::toMap()).
	Q_PROPERTY(QVariantList invites READ invites NOTIFY changed)
	/// The invites you sent, the same way (uuid and name are the friend's).
	Q_PROPERTY(QVariantList outgoingInvites READ outgoingInvites NOTIFY changed)
	/// The server the game is on, to invite friends to: host and port, or
	/// empty while not on one. Even one friends cannot see.
	Q_PROPERTY(QVariantMap inviteTarget READ inviteTarget NOTIFY changed)

  public:
	/// @p dataDir is where friends.json lives.
	explicit FriendsController(const QString& dataDir,
							   QObject* parent = nullptr);
	~FriendsController() override;

	/// Whether this build has a friends server (Kite_FRIENDS_URL or FriendsServerUrl).
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
	bool live() const;
	FriendsChat* chat() const;
	QVariantList invites() const;
	QVariantList outgoingInvites() const;
	QVariantMap inviteTarget() const;

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
	/// Also works with Friends off: signs in for this one request and leaves Friends off.
	Q_INVOKABLE void deleteAccount();
	/// Invites the friend @p uuid to @p address ("host" or "host:port"); a
	/// second invite to the same friend replaces the first.
	Q_INVOKABLE void sendInvite(const QString& uuid, const QString& address);
	/// Drops the invite @p uuid sent you; they are not told.
	Q_INVOKABLE void dismissInvite(const QString& uuid);
	/// Takes back your invite to @p uuid.
	Q_INVOKABLE void cancelInvite(const QString& uuid);

	/// Authenticated request for another controller of the same server (share
	/// codes): signs in first if needed; @p done gets the raw answer.
	void send(const QByteArray& verb, const QString& path,
			  const QByteArray& body, FriendsHttp::Done done);

	// What the Kite mod's bridge needs (FriendsBridge.cpp, modbridge/)
	/// Its rev changes whenever anything the mod is told does.
	ModBridgeJson::FriendsView bridgeView();
	/// Polls now, at most once in 10 s (the mod's refresh button).
	void bridgeRefresh();
	/// "" when there is none or @p uuid is not a friend.
	QString bridgeFacePath(const QString& uuid) const;
	/// The mod says where the player is; that is what friends are told.
	void reportWorld(BaseInstance* instance, const FriendsApi::World& world);

  public slots:
	/// The socket's `invite`, `invite.cancelled` and `friend.removed` events
	/// (README "Events"), as the JSON object of the frame: the invites change
	/// at once, without waiting for the next poll. Others are ignored.
	void applyInviteEvent(const QJsonObject& event);

  signals:
	void changed();
	/// Every fresh snapshot, even one that left the lists as they were.
	void updatedAtChanged();
	void actionFinished(QString action, bool ok, QString message);

  private:
	/// Reaches applyInviteEvent()'s state without a launcher behind it.
	friend class FriendsInvitesTest;

	static constexpr const char* kEnabledSetting = "FriendsEnabled";
	static constexpr const char* kServerUrlSetting = "FriendsServerUrl";
	static constexpr const char* kMojangUrlSetting = "FriendsMojangSessionUrl";
	static constexpr const char* kNotifySetting = "FriendsNotifications";
	static constexpr const char* kMessageNotifySetting = "FriendsMessageNotifications";

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
	/// Polls every ten seconds while the account plays and the socket is down, else every minute.
	void followPlay();
	void pushStatus();
	QString storePath() const;
	QString serverUrl() const;
	QString mojangUrl() const;
	bool sessionUsable() const;
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
	/// True if the default account changed since the sign-in started: the answer
	/// (and @p sessionToken, which is signed out) is dropped and sign-in restarts.
	bool dropIfAccountChanged(const QString& sessionToken = QString());
	void failSignIn(const QString& message, bool needsSignIn = false);
	/// Only a refused token asks for a new sign-in.
	void failMojangJoin(const FriendsApi::Reply& reply);
	void refuseAccount(const QString& message);
	bool stillRefused() const;
	/// True if the access token is (about to be) out of date and being renewed; the sign-in waits.
	bool waitForRenewedToken();
	void resumeSignIn();
	void onRenewalTimeout();
	/// Id, profile and token of the default account as one string; "" without an account.
	QString accountIdentity() const;
	QString defaultProfileId() const;
	bool working() const;
	/// True if the last sign-in failed too recently to retry on this tick (failures back off).
	bool signInBackedOff() const;
	void signOutQuietly(const QString& token);

	// Requests and the poll (FriendsPoll.cpp)
	/// @p keepAnswer: pass the answer on even if another account became default meanwhile (for a delete).
	void request(const QByteArray& verb, const QString& path,
				 const QByteArray& body, FriendsHttp::Done done,
				 bool retried = false, bool keepAnswer = false);
	/// Runs @p then once no status report is in flight: a report racing the DELETE could put the status back.
	void afterStatusReports(std::function<void()> then);
	void statusReportAnswered();
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
	/// For a change of account: also lets settings be read again and drops answers still on their way.
	void forgetAccountData();
	/// forgetAccountData() when the data held belongs to another account than @p profileId (empty: none).
	void dropDataOfOtherAccount(const QString& profileId);
	void attachFaces(QVariantList& list) const;
	void fetchFaces();
	void onFacesChanged();

	// The socket and what it pushes (FriendsRealtime.cpp)
	void setUpRealtime();
	/// The socket runs while Friends is on with a usable session, on its token.
	void followSession();
	void stopRealtime();
	void onSocketUp(bool up);
	void onSocketFrame(const QJsonObject& frame);
	void onSocketSessionEnded();
	void onChatMessage(const FriendsChatApi::Message& message);
	void onChatRead(const QString& uuid);
	/// Shows a snapshot a live event changed.
	void applyLive(const FriendsApi::FriendsSnapshot& snapshot);
	/// @p snapshot without unread messages in the conversation on screen.
	FriendsApi::FriendsSnapshot chatSeen(const FriendsApi::FriendsSnapshot& snapshot) const;
	/// friend.* frames: the lists change in place.
	void applyFriendEvent(const QJsonObject& frame);

	// Desktop notifications (FriendsNotify.cpp)
	/// The notices for invites pushed by the socket that were not seen yet.
	void notifyInvites(const QList<FriendsApi::Invite>& incoming);
	/// Tells the plugins (DesktopNotifier) who came online and which requests
	/// arrived since the last snapshot, while kNotifySetting is on.
	void notifyChanges(const FriendsApi::FriendsSnapshot& snapshot,
					   const QDateTime& now);
	/// A message from a friend whose conversation is not on screen, while kMessageNotifySetting is on.
	void notifyMessage(const FriendsChatApi::Message& message);
	void dispatchNotices(const QList<FriendsNotices::Notice>& notices,
						 const QDateTime& now);

	// Invites (FriendsInviteActions.cpp)
	/// Both return whether what QML reads changed; neither emits changed().
	bool setInvites(const FriendsApi::InviteLists& lists);
	/// Rebuilds the lists QML reads from m_inviteData without the invites that
	/// ran out, and arms the timer for the next to run out.
	bool publishInvites();
	void onInviteExpiry();
	QString inviteErrorText(const FriendsApi::Reply& reply,
							const QString& name) const;

	// Actions (FriendsActions.cpp)
	/// Words the outcome with @p interpret (sets *ok, returns the message); a success re-reads the lists.
	void act(const QString& action, const QByteArray& verb,
			 const QString& path, const QByteArray& body, Interpret interpret,
			 bool thenRefresh = true);
	/// actionFinished(), and not before the caller has returned.
	void finishAction(const QString& action, bool ok, const QString& message);
	bool canAct(const QString& action, const QString* uuid);
	void loadBlocked(std::function<void(const FriendsApi::Reply&, bool)> done);
	QString nameOf(const QString& uuid) const;
	QString requestErrorText(const FriendsApi::Reply& reply,
							 const QString& name) const;

  private:
	QString m_dataDir;
	FriendsHttp* m_http;
	FriendsFaces* m_faces;
	std::unique_ptr<FriendsGameWatcher> m_watcher;
	QTimer m_pollTimer;
	QTimer m_pushTimer;
	QTimer m_facesTimer;
	QTimer m_renewalTimer;
	FriendsSocket* m_socket = nullptr;
	FriendsChat* m_chat = nullptr;
	bool m_live = false;

	bool m_enabled = false;
	bool m_quitting = false;
	State m_state = State::Off;
	QString m_error;
	bool m_needsSignIn = false;
	bool m_loaded = false;
	/// Counts enable/disable so a late answer does not touch the new state.
	int m_epoch = 0;
	/// deleteAccount() with Friends off: the session is for that request alone.
	bool m_deletingOff = false;

	FriendsApi::Session m_session;
	bool m_signingIn = false;
	bool m_awaitingRenewal = false;
	/// Asked once per sign-in; finishSignIn() clears it.
	bool m_renewalAsked = false;
	/// A retry of a failed sign-in: keeps showing the error instead of "Connecting…".
	bool m_quietSignIn = false;
	bool m_renewalTimedOut = false;
	int m_signInFailures = 0;
	/// The account the last sign-in failed with; an unrelated account-list change is no reason to retry sooner.
	QString m_failedIdentity;
	QString m_refusedProfile;
	QString m_refusedText;
	std::vector<SessionDone> m_waiters;
	QElapsedTimer m_lastSignInAttempt;
	/// The account a sign-in started with: its answers are about it even if another becomes default.
	struct SignInTarget {
		QString internalId;
		QString profileId;
		QString identity;
	};
	SignInTarget m_signInTarget;
	/// Status reports (PUT) sent and not yet answered.
	int m_statusInFlight = 0;
	std::vector<std::function<void()>> m_afterStatus;

	bool m_polling = false;
	bool m_pollAgain = false;
	bool m_settingsLoaded = false;
	/// Profile id (no dashes) these lists and settings were read for; empty if none.
	QString m_dataProfile;
	/// Counts account changes so an answer asked for the old account is not taken for the new one's.
	int m_accountGen = 0;
	QElapsedTimer m_lastStatusSent;
	QDateTime m_backoffUntil;

	QVariantList m_friends;
	QVariantList m_incoming;
	QVariantList m_outgoing;
	QVariantList m_blocked;
	QVariantMap m_settings;
	FriendsApi::InviteLists m_inviteData;
	QVariantList m_invites;
	QVariantList m_outgoingInvites;
	QTimer m_inviteExpiryTimer;
	QDateTime m_updatedAt;
	/// What the next snapshot's notices are worked out against; reset with the data.
	FriendsNotices::Memory m_noticeMemory;
	/// The lists as last shown, which live events change.
	FriendsApi::FriendsSnapshot m_snapshot;

	struct SelfView {
		FriendsApi::Presence sent;
		FriendsApi::Status status;
	};
	std::optional<SelfView> m_myView;

	/// bridgeView() as last said, apart from rev and time, and the rev that went with it.
	QByteArray m_bridgeDigest;
	qint64 m_bridgeRev = 0;
	QElapsedTimer m_lastBridgeRefresh;
};
