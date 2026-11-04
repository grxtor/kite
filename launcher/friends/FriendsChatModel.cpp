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

#include "FriendsChatModel.h"

FriendsChatModel::FriendsChatModel(QObject* parent) : QAbstractListModel(parent) {}

int FriendsChatModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() || !m_entries ? 0 : static_cast<int>(m_entries->size());
}

QHash<int, QByteArray> FriendsChatModel::roleNames() const
{
	return {{KeyRole, "key"},
			{IdRole, "messageId"},
			{MineRole, "mine"},
			{TextRole, "text"},
			{DeletedRole, "deleted"},
			{CreatedAtRole, "createdAtMs"},
			{StateRole, "sendState"},
			{ErrorRole, "error"},
			{GroupStartRole, "groupStart"},
			{DayStartRole, "dayStart"}};
}

QVariant FriendsChatModel::data(const QModelIndex& index, int role) const
{
	if (!m_entries || !index.isValid() || index.row() >= rowCount()) {
		return QVariant();
	}
	const int at = rowOf(index.row());
	const ChatEntry& entry = m_entries->at(at);
	switch (role) {
		case KeyRole:
			return entry.key;
		case IdRole:
			return entry.id;
		case MineRole:
			return entry.mine;
		case TextRole:
			return entry.text;
		case DeletedRole:
			return entry.deleted;
		case CreatedAtRole:
			return entry.createdAt.toMSecsSinceEpoch();
		case StateRole:
			return entry.state == ChatEntry::State::Sending	 ? QStringLiteral("sending")
				   : entry.state == ChatEntry::State::Failed ? QStringLiteral("failed")
															 : QStringLiteral("sent");
		case ErrorRole:
			return entry.error;
		case GroupStartRole:
			return startsGroup(*m_entries, at);
		case DayStartRole:
			return startsDay(*m_entries, at);
		default:
			return QVariant();
	}
}

void FriendsChatModel::show(const QList<ChatEntry>* entries)
{
	beginResetModel();
	m_entries = entries;
	endResetModel();
}

const QList<ChatEntry>* FriendsChatModel::shown() const
{
	return m_entries;
}

int FriendsChatModel::rowOf(int at) const
{
	/* The same formula maps both ways: newest first is oldest first reversed. */
	return rowCount() - 1 - at;
}

void FriendsChatModel::beginInsert(int at)
{
	/* After the insert there is one more row, so the new entry's row is size - at. */
	const int row = rowCount() - at;
	beginInsertRows(QModelIndex(), row, row);
}

void FriendsChatModel::endInsert(int at)
{
	endInsertRows();
	neighbourChanged(at + 1);
}

void FriendsChatModel::beginRemove(int at)
{
	const int row = rowOf(at);
	beginRemoveRows(QModelIndex(), row, row);
}

void FriendsChatModel::endRemove(int at)
{
	endRemoveRows();
	neighbourChanged(at);
}

void FriendsChatModel::changed(int at)
{
	if (!m_entries || at < 0 || at >= rowCount()) {
		return;
	}
	const QModelIndex row = index(rowOf(at));
	emit dataChanged(row, row);
}

void FriendsChatModel::neighbourChanged(int at)
{
	/* Whether an entry starts a run or a day depends on the one before it. */
	changed(at);
}

bool FriendsChatModel::startsDay(const QList<ChatEntry>& entries, int at)
{
	if (at <= 0) {
		return true;
	}
	return entries.at(at).createdAt.toLocalTime().date() !=
		   entries.at(at - 1).createdAt.toLocalTime().date();
}

bool FriendsChatModel::startsGroup(const QList<ChatEntry>& entries, int at)
{
	if (startsDay(entries, at)) {
		return true;
	}
	const ChatEntry& entry = entries.at(at);
	const ChatEntry& before = entries.at(at - 1);
	return entry.mine != before.mine ||
		   before.createdAt.msecsTo(entry.createdAt) > kGroupGapMs;
}
