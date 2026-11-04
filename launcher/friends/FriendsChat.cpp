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
 * FriendsChat: which conversation is open, what the socket coming and going
 * does to the others, forgetting, and marking the open one read.
 */

#include "FriendsChat.h"

using FriendsChatApi::idLess;

namespace
{
	const QByteArray kPost = "POST";
} // namespace

FriendsChat::FriendsChat(QObject* parent)
	: QObject(parent), m_model(new FriendsChatModel(this))
{
	m_readTimer.setSingleShot(true);
	m_readTimer.setInterval(kReadDelayMs);
	connect(&m_readTimer, &QTimer::timeout, this, &FriendsChat::markRead);
}

void FriendsChat::setTransport(Transport transport)
{
	m_transport = std::move(transport);
}

void FriendsChat::setMe(const QString& uuid)
{
	m_me = uuid.toLower();
}

QString FriendsChat::openUuid() const
{
	return m_openUuid;
}

QAbstractListModel* FriendsChat::messages() const
{
	return m_model;
}

const FriendsChat::Conversation* FriendsChat::openConversation() const
{
	const auto it = m_conversations.find(m_openUuid);
	return it == m_conversations.end() ? nullptr : &it->second;
}

bool FriendsChat::loading() const
{
	const auto* conv = openConversation();
	return conv && conv->loading;
}

bool FriendsChat::loaded() const
{
	const auto* conv = openConversation();
	return conv && conv->loaded;
}

bool FriendsChat::loadingOlder() const
{
	const auto* conv = openConversation();
	return conv && conv->loadingOlder;
}

bool FriendsChat::hasOlder() const
{
	const auto* conv = openConversation();
	return conv && conv->hasOlder;
}

QString FriendsChat::loadError() const
{
	const auto* conv = openConversation();
	return conv ? conv->loadError : QString();
}

bool FriendsChat::viewing() const
{
	return m_viewing;
}

void FriendsChat::setViewing(bool viewing)
{
	if (viewing == m_viewing) {
		return;
	}
	m_viewing = viewing;
	emit viewingChanged();
	scheduleRead();
}

bool FriendsChat::isViewing(const QString& uuid) const
{
	return m_viewing && !m_openUuid.isEmpty() && m_openUuid == uuid.toLower();
}

qint64 FriendsChat::rateLimitedUntilMs() const
{
	const bool limited = m_rateLimitedUntil.isValid() &&
						 QDateTime::currentDateTimeUtc() < m_rateLimitedUntil;
	return limited ? m_rateLimitedUntil.toMSecsSinceEpoch() : 0;
}

int FriendsChat::maxLength() const
{
	return FriendsChatApi::kMaxTextLength;
}

FriendsChat::Conversation* FriendsChat::find(const QString& uuid)
{
	const auto it = m_conversations.find(uuid);
	return it == m_conversations.end() ? nullptr : &it->second;
}

FriendsChat::Conversation& FriendsChat::conversation(const QString& uuid)
{
	return m_conversations[uuid];
}

void FriendsChat::open(const QString& uuid)
{
	const QString key = uuid.toLower();
	if (!FriendsApi::isUuid(key) || key == m_openUuid) {
		return;
	}
	m_openUuid = key;
	Conversation& conv = conversation(key);
	m_model->show(&conv.entries);
	emit openChanged();
	if (!conv.loaded && !conv.loading) {
		loadNewest(key);
	}
	emit stateChanged();
	scheduleRead();
}

void FriendsChat::close()
{
	if (m_openUuid.isEmpty()) {
		return;
	}
	m_openUuid.clear();
	m_model->show(nullptr);
	m_readTimer.stop();
	emit openChanged();
	emit stateChanged();
}

void FriendsChat::reload()
{
	if (!m_openUuid.isEmpty() && !loading()) {
		loadNewest(m_openUuid);
	}
}

void FriendsChat::setLive(bool up)
{
	if (up == m_live) {
		return;
	}
	m_live = up;
	if (!up) {
		return;
	}
	/* Nothing sent while it was down is replayed. What is not on screen is read
	 * afresh when it is opened; the open one catches up. */
	for (auto it = m_conversations.begin(); it != m_conversations.end();) {
		it = it->first == m_openUuid ? std::next(it) : m_conversations.erase(it);
	}
	catchUp();
}

void FriendsChat::forget(const QString& uuid)
{
	const QString key = uuid.toLower();
	if (key == m_openUuid) {
		close();
	}
	/* An answer still on its way finds no conversation and is dropped. */
	m_conversations.erase(key);
}

void FriendsChat::reset()
{
	close();
	m_conversations.clear();
	m_rateLimitedUntil = QDateTime();
	m_live = false;
	m_me.clear();
	++m_gen;
	emit stateChanged();
}

void FriendsChat::scheduleRead()
{
	if (m_viewing && !m_openUuid.isEmpty()) {
		m_readTimer.start();
	}
}

void FriendsChat::markRead()
{
	Conversation* conv = m_viewing ? find(m_openUuid) : nullptr;
	if (!conv || conv->theirNewest.isEmpty() || !idLess(conv->readUpTo, conv->theirNewest)) {
		return;
	}
	const QString upTo = newestStoredId(*conv);
	conv->readUpTo = upTo;
	const QString uuid = m_openUuid;
	emit markedRead(uuid);
	const QString ref = QStringLiteral("r%1").arg(++m_nextLocal);
	if (m_transport.sendFrame(FriendsChatApi::readFrame(ref, uuid, upTo))) {
		return;
	}
	/* Unanswered: a marker that did not move is moved by the next one. */
	m_transport.request(kPost, FriendsChatApi::Path::read(uuid),
						FriendsChatApi::readBody(upTo), [](const FriendsApi::Reply&) {});
}
