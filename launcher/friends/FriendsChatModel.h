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

#include <QAbstractListModel>
#include <QDateTime>
#include <QList>
#include <QString>

/// One line of a conversation: a stored message, or one of ours on its way.
struct ChatEntry {
	enum class State { Sent, Sending, Failed };
	/// The message id once stored, else "local-<n>"; unique in a conversation.
	QString key;
	/// Empty until the server stored it.
	QString id;
	/// The socket `ref` it went out with, while sending over the socket.
	QString ref;
	bool mine = false;
	QString text;
	QDateTime createdAt;
	bool deleted = false;
	State state = State::Sent;
	/// Why it was not sent, worded for the user.
	QString error;

	bool stored() const
	{
		return !id.isEmpty();
	}
};

/*
 * The open conversation for QML, newest first (row 0 is the newest), so a
 * ListView laid out bottom to top keeps the newest at the bottom and grows
 * upwards as history is read. It shows entries FriendsChat owns, oldest
 * first; FriendsChat tells it of every change by chronological index.
 */
class FriendsChatModel : public QAbstractListModel
{
	Q_OBJECT

  public:
	enum Role {
		KeyRole = Qt::UserRole + 1,
		IdRole,
		MineRole,
		TextRole,
		DeletedRole,
		CreatedAtRole,
		/// "sent" | "sending" | "failed"
		StateRole,
		ErrorRole,
		/// The first of a run from one side (a head and name go above it).
		GroupStartRole,
		/// The first of its day (a day separator goes above it).
		DayStartRole,
	};

	/// A run from one side breaks after this long a pause.
	static constexpr qint64 kGroupGapMs = 7 * 60 * 1000;

	explicit FriendsChatModel(QObject* parent = nullptr);

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;

	/// Shows @p entries (nullptr: none); they must outlive the showing.
	void show(const QList<ChatEntry>* entries);
	const QList<ChatEntry>* shown() const;

	// Around a change to the shown entries; @p at is the chronological index.
	void beginInsert(int at);
	void endInsert(int at);
	void beginRemove(int at);
	void endRemove(int at);
	void changed(int at);

	static bool startsGroup(const QList<ChatEntry>& entries, int at);
	static bool startsDay(const QList<ChatEntry>& entries, int at);

  private:
	int rowOf(int at) const;
	void neighbourChanged(int at);

	const QList<ChatEntry>* m_entries = nullptr;
};
