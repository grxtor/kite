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
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTimer>

#include <functional>
#include <map>

#include "FriendsApi.h"
#include "FriendsChatApi.h"
#include "FriendsChatModel.h"

/*
 * One-to-one chats with friends (FriendsController.chat in QML). One
 * conversation is open at a time and shown by `messages`; the others read this
 * session stay in memory, never on disk. A message is shown as soon as it is
 * sent and replaced by the server's copy when it answers (chat.sent over the
 * socket, the 201 over HTTP while the socket is down), or marked failed with
 * the reason. The conversation on screen is marked read; reads, history and
 * reports go through the controller's signed-in requests.
 * Sending (FriendsChatSend.cpp), the rest (FriendsChat.cpp).
 */
class FriendsChat : public QObject
{
	Q_OBJECT

	/// The friend whose conversation is open (dashed, lower case); "" for none.
	Q_PROPERTY(QString openUuid READ openUuid NOTIFY openChanged)
	/// FriendsChatModel of the open conversation, newest first.
	Q_PROPERTY(QAbstractListModel* messages READ messages CONSTANT)
	/// The newest page of the open conversation is on its way.
	Q_PROPERTY(bool loading READ loading NOTIFY stateChanged)
	/// It has been read once; until then an empty list says nothing.
	Q_PROPERTY(bool loaded READ loaded NOTIFY stateChanged)
	Q_PROPERTY(bool loadingOlder READ loadingOlder NOTIFY stateChanged)
	/// Older messages are on the server (loadOlder()).
	Q_PROPERTY(bool hasOlder READ hasOlder NOTIFY stateChanged)
	/// Why the newest page could not be read, worded; "" when it was.
	Q_PROPERTY(QString loadError READ loadError NOTIFY stateChanged)
	/// QML sets it while the open conversation is on screen in an active window.
	Q_PROPERTY(bool viewing READ viewing WRITE setViewing NOTIFY viewingChanged)
	/// Milliseconds since the epoch until which the server refuses messages; 0 if it does not.
	Q_PROPERTY(qint64 rateLimitedUntilMs READ rateLimitedUntilMs NOTIFY stateChanged)
	Q_PROPERTY(int maxLength READ maxLength CONSTANT)

  public:
	/// How the chat reaches the server; FriendsController fills it in.
	struct Transport {
		/// A socket frame; false when the socket is not up.
		std::function<bool(const QJsonObject&)> sendFrame;
		/// A signed-in HTTP request; @p done always runs.
		std::function<void(const QByteArray& verb, const QString& path,
						   const QByteArray& body,
						   std::function<void(const FriendsApi::Reply&)> done)>
			request;
		/// How an action (report) went, for a toast.
		std::function<void(const QString& action, bool ok, const QString& message)> finished;
	};

	/// No answer to a send in this long and it failed.
	static constexpr int kSendTimeoutMs = 15 * 1000;
	/// Reads are told once things have settled on screen.
	static constexpr int kReadDelayMs = 400;
	/// A catch-up after this many pages starts the conversation afresh.
	static constexpr int kMaxCatchUpPages = 5;

	explicit FriendsChat(QObject* parent = nullptr);

	void setTransport(Transport transport);
	/// The signed-in player (dashed); messages from them are "mine".
	void setMe(const QString& uuid);

	QString openUuid() const;
	QAbstractListModel* messages() const;
	bool loading() const;
	bool loaded() const;
	bool loadingOlder() const;
	bool hasOlder() const;
	QString loadError() const;
	bool viewing() const;
	void setViewing(bool viewing);
	qint64 rateLimitedUntilMs() const;
	int maxLength() const;

	Q_INVOKABLE void open(const QString& uuid);
	Q_INVOKABLE void close();
	/// False (and nothing sent) for an empty or over-long text, or while rate limited.
	Q_INVOKABLE bool send(const QString& text);
	/// Sends a failed message again, as the newest.
	Q_INVOKABLE void retry(const QString& key);
	/// Drops a failed message.
	Q_INVOKABLE void discard(const QString& key);
	Q_INVOKABLE void loadOlder();
	/// Reads the open conversation's newest page again (after loadError).
	Q_INVOKABLE void reload();
	/// @p reason: "spam" | "harassment" | "other". Ends with finished("report", ...).
	Q_INVOKABLE void report(const QString& messageId, const QString& reason);

	/// chat.message, chat.sent, chat.read, chat.deleted and error frames; false for any other.
	bool handleFrame(const QJsonObject& frame);
	/// The socket came up (catch up, forget what was not watched) or went down.
	void setLive(bool up);
	/// The open conversation's newer messages over HTTP: after a reconnect, or polled while the socket is down.
	void catchUp();
	/// A block: the history with @p uuid is gone for both.
	void forget(const QString& uuid);
	/// Signed out, or another account: everything goes.
	void reset();
	bool isViewing(const QString& uuid) const;

  signals:
	void openChanged();
	void stateChanged();
	void viewingChanged();
	/// A new message to us from a friend arrived (not one read from history).
	void messageReceived(const FriendsChatApi::Message& message);
	/// The conversation with @p uuid was marked read here: its unread count is 0.
	void markedRead(const QString& uuid);
	/// Our read marker moved on another socket: the unread counts need reading again.
	void readElsewhere(const QString& uuid);

  private:
	struct Conversation {
		/// Oldest first: the stored messages by id, then ours that are not stored yet.
		QList<ChatEntry> entries;
		bool loaded = false;
		bool loading = false;
		bool loadingOlder = false;
		bool hasOlder = false;
		QString loadError;
		/// How far we marked it read; their newest message we hold.
		QString readUpTo;
		QString theirNewest;
	};

	// FriendsChat.cpp
	Conversation* find(const QString& uuid);
	Conversation& conversation(const QString& uuid);
	const Conversation* openConversation() const;
	void loadNewest(const QString& uuid);
	void onNewest(const QString& uuid, int gen, const FriendsApi::Reply& reply);
	void catchUpPage(const QString& uuid, int gen, int page);
	/// Adds a stored message where its id belongs; false if it was there already.
	bool addStored(const QString& uuid, const FriendsChatApi::Message& message);
	void insertEntry(const QString& uuid, int at, ChatEntry entry);
	void removeEntry(const QString& uuid, int at);
	void entryChanged(const QString& uuid, int at);
	int indexOfKey(const Conversation& conv, const QString& key) const;
	QString newestStoredId(const Conversation& conv) const;
	void markDeleted(const QString& uuid);
	void scheduleRead();
	void markRead();
	QString loadErrorText(const FriendsApi::Reply& reply) const;

	// FriendsChatSend.cpp
	void deliver(const QString& uuid, const QString& key);
	void onSent(const QString& ref, const FriendsChatApi::Message& message);
	void onHttpSent(const QString& uuid, const QString& key, int gen,
					const FriendsApi::Reply& reply);
	void onFrameError(const FriendsChatApi::FrameError& error);
	void onIncoming(const FriendsChatApi::Message& message);
	void fail(const QString& uuid, const QString& key, const QString& why);
	void failIfSending(const QString& uuid, const QString& key, const QString& ref);
	/// The conversation and key of our message that went out with @p ref.
	bool findRef(const QString& ref, QString* uuid, QString* key) const;
	void noteRateLimit(int retryAfterSeconds);
	QString sendErrorText(const QString& code) const;
	void reportAnswered(const FriendsApi::Reply& reply);

	Transport m_transport;
	FriendsChatModel* m_model;
	/// Node-based: the open conversation's entries stay put while others come and go.
	std::map<QString, Conversation> m_conversations;
	QString m_me;
	QString m_openUuid;
	bool m_viewing = false;
	bool m_live = false;
	QDateTime m_rateLimitedUntil;
	QTimer m_readTimer;
	/// Counts reset()s and forget()s so a late answer does not land in what replaced it.
	int m_gen = 0;
	int m_nextLocal = 0;
};
