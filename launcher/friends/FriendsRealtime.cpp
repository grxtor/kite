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
 * FriendsController's socket (GET /v1/ws): open while Friends is on with a
 * session, closed with it. What it pushes goes where the poll's answers go:
 * friends' statuses and requests into the lists, chat into FriendsChat,
 * invites (and friend.removed, whose invites go too) to applyInviteEvent().
 * Up, the poll slows to the minute's safety net; every (re)connect reads the
 * lists again, as events are not replayed.
 */

#include "FriendsController.h"

#include "FriendsEvents.h"
#include "FriendsSocket.h"

void FriendsController::setUpRealtime()
{
	m_socket = new FriendsSocket(this);
	m_chat = new FriendsChat(this);
	connect(m_socket, &FriendsSocket::upChanged, this, &FriendsController::onSocketUp);
	connect(m_socket, &FriendsSocket::frameReceived, this,
			&FriendsController::onSocketFrame);
	connect(m_socket, &FriendsSocket::sessionEnded, this,
			&FriendsController::onSocketSessionEnded);
	connect(m_chat, &FriendsChat::messageReceived, this,
			&FriendsController::onChatMessage);
	connect(m_chat, &FriendsChat::markedRead, this, &FriendsController::onChatRead);
	/* Another socket of ours moved the marker: only the server knows what is left. */
	connect(m_chat, &FriendsChat::readElsewhere, this, [this]() { poll(); });
	/* Without the socket the open conversation is read along with each poll. */
	connect(this, &FriendsController::updatedAtChanged, this, [this]() {
		if (!m_live) {
			m_chat->catchUp();
		}
	});

	FriendsChat::Transport transport;
	transport.sendFrame = [this](const QJsonObject& frame) {
		return m_socket->send(frame);
	};
	transport.request = [this](const QByteArray& verb, const QString& path,
							   const QByteArray& body, FriendsHttp::Done done) {
		if (!m_enabled) {
			FriendsApi::Reply off;
			off.failureText = tr("Friends is turned off");
			done(off);
			return;
		}
		request(verb, path, body, std::move(done));
	};
	transport.finished = [this](const QString& action, bool ok, const QString& message) {
		finishAction(action, ok, message);
	};
	m_chat->setTransport(std::move(transport));
}

bool FriendsController::live() const
{
	return m_live;
}

FriendsChat* FriendsController::chat() const
{
	return m_chat;
}

void FriendsController::followSession()
{
	if (!m_socket) {
		return;
	}
	const bool wanted = m_enabled && !m_session.token.isEmpty() && sessionUsable();
	if (!wanted) {
		m_socket->stop();
		return;
	}
	m_chat->setMe(m_session.uuid);
	/* The same token keeps the socket it has. */
	m_socket->start(FriendsSocket::socketUrl(serverUrl()), m_session.token);
}

void FriendsController::stopRealtime()
{
	if (m_socket) {
		m_socket->stop();
		m_chat->reset();
	}
}

void FriendsController::onSocketUp(bool up)
{
	if (up == m_live) {
		return;
	}
	m_live = up;
	m_chat->setLive(up);
	followPlay();
	emit changed();
	if (up) {
		poll();
	}
}

void FriendsController::onSocketFrame(const QJsonObject& frame)
{
	const auto kind = FriendsEvents::kindOf(frame);
	if (kind == FriendsEvents::Kind::Chat) {
		m_chat->handleFrame(frame);
	} else if (kind == FriendsEvents::Kind::Friend) {
		applyFriendEvent(frame);
	}
	if (FriendsEvents::forInvites(frame)) {
		applyInviteEvent(frame);
	}
}

void FriendsController::applyFriendEvent(const QJsonObject& frame)
{
	if (!m_loaded) {
		/* Nothing on screen to change yet: the first read brings it all. */
		poll();
		return;
	}
	FriendsApi::FriendsSnapshot next = m_snapshot;
	const auto outcome = FriendsEvents::apply(next, frame);
	if (outcome.changed) {
		applyLive(next);
	}
	if (outcome.poll) {
		poll();
	}
}

void FriendsController::onSocketSessionEnded()
{
	/* Logged out elsewhere, expired, or dropped for an eleventh sign-in: the token
	 * is dead, so the next request signs in again and the socket follows. */
	clearSession();
	emit changed();
	tick();
}

void FriendsController::onChatMessage(const FriendsChatApi::Message& message)
{
	if (m_chat->isViewing(message.from)) {
		return;
	}
	FriendsApi::FriendsSnapshot next = m_snapshot;
	if (m_loaded && FriendsEvents::addUnread(next, message.from)) {
		applyLive(next);
	}
	notifyMessage(message);
}

void FriendsController::onChatRead(const QString& uuid)
{
	FriendsApi::FriendsSnapshot next = m_snapshot;
	if (m_loaded && FriendsEvents::setUnread(next, uuid, 0)) {
		applyLive(next);
	}
}

void FriendsController::applyLive(const FriendsApi::FriendsSnapshot& snapshot)
{
	applySnapshot(snapshot);
}

FriendsApi::FriendsSnapshot FriendsController::chatSeen(
	const FriendsApi::FriendsSnapshot& snapshot) const
{
	FriendsApi::FriendsSnapshot seen = snapshot;
	for (auto& one : seen.friends) {
		/* A poll that left before the marker moved still counts them. */
		if (m_chat && m_chat->isViewing(one.uuid)) {
			one.unread = 0;
		}
	}
	return seen;
}
