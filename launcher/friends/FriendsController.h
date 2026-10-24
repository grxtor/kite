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
#include <vector>

#include "FriendsApi.h"
#include "FriendsHttp.h"

class FriendsFaces;
class FriendsGameWatcher;

/*
 * The friends list, for the QML side (QmlShell.friends).
 *
 * Off until the user turns it on (the FriendsEnabled setting): nothing is
 * sent to anyone before that. On, it signs in to Kite's friends server with
 * the default Minecraft account, then once a minute reports what the player
 * is doing and reads the friends, the requests and their statuses. See
 * server/friends/README.md for the other end and FriendsApi for the
 * messages.
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
	Q_PROPERTY(QString myName READ myName NOTIFY changed)
	/// uuid, name, since, state, lastSeenAt, instance, version, worldType,
	/// host, port, statusText (see FriendsDisplay::friendMap), face.
	/// face (here and in the lists below) is the URL of the person's cached
	/// head, "" until it has been fetched from Mojang (FriendsFaces).
	Q_PROPERTY(QVariantList friends READ friends NOTIFY changed)
	/// uuid, name, createdAt, face
	Q_PROPERTY(QVariantList incoming READ incoming NOTIFY changed)
	Q_PROPERTY(QVariantList outgoing READ outgoing NOTIFY changed)
	Q_PROPERTY(QVariantList blocked READ blocked NOTIFY changed)
	/// visibility ("friends" | "everyone" | "invisible"), hideServer,
	/// shareIpServers
	Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
	/// Friends who are not offline.
	Q_PROPERTY(int onlineCount READ onlineCount NOTIFY changed)
	Q_PROPERTY(int incomingCount READ incomingCount NOTIFY changed)

  public:
	/// @p dataDir is where friends.json lives.
	explicit FriendsController(const QString& dataDir,
							   QObject* parent = nullptr);
	~FriendsController() override;

	bool enabled() const;
	void setEnabled(bool enabled);
	QString state() const;
	QString error() const;
	QString myName() const;
	QVariantList friends() const;
	QVariantList incoming() const;
	QVariantList outgoing() const;
	QVariantList blocked() const;
	QVariantMap settings() const;
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
	/// turns the feature off.
	Q_INVOKABLE void deleteAccount();

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
	void setState(State state, const QString& error = QString());
	void onAccountsChanged();
	void onAboutToQuit();
	void onPresenceChanged();
	void pushStatus();
	QString storePath() const;
	QString serverUrl() const;
	QString mojangUrl() const;
	bool sessionUsable() const;
	void setSession(const FriendsApi::Session& session);
	void clearSession();

	// Sign-in (FriendsSignIn.cpp)
	void ensureSession(SessionDone done);
	void startSignIn();
	void joinMojang(const FriendsApi::Challenge& challenge,
					const QString& accessToken, const QString& profileId);
	void login(const FriendsApi::Challenge& challenge, int attempt);
	void finishSignIn(bool ok);
	void failSignIn(const QString& message);
	void signOutQuietly(const QString& token);

	// Requests and the poll (FriendsPoll.cpp)
	void request(const QByteArray& verb, const QString& path,
				 const QByteArray& body, FriendsHttp::Done done,
				 bool retried = false);
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

	bool m_enabled = false;
	State m_state = State::Off;
	QString m_error;
	/// Counts enable/disable, so an answer that arrives after a switch
	/// does not touch the new state.
	int m_epoch = 0;

	FriendsApi::Session m_session;
	bool m_signingIn = false;
	std::vector<SessionDone> m_waiters;
	QElapsedTimer m_lastSignInAttempt;

	bool m_polling = false;
	bool m_pollAgain = false;
	bool m_settingsLoaded = false;
	QElapsedTimer m_lastStatusSent;
	QDateTime m_backoffUntil;

	QVariantList m_friends;
	QVariantList m_incoming;
	QVariantList m_outgoing;
	QVariantList m_blocked;
	QVariantMap m_settings;
};
