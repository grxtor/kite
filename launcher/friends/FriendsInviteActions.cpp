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
 * FriendsController's invites to a server: the lists QML reads (from the
 * poll, or pushed by the socket through applyInviteEvent()), sending one,
 * dismissing one you got and taking back one you sent. An invite runs out
 * after ten minutes; a timer drops it then without waiting for the poll.
 */

#include "FriendsController.h"

#include <algorithm>

#include "FriendsInvites.h"
#include "minecraft/launch/MinecraftServerTarget.h"

namespace
{
	const QByteArray kPut = "PUT";
	const QByteArray kPost = "POST";
	const QByteArray kDelete = "DELETE";
	constexpr int kBadRequest = 400;
	constexpr int kNotFound = 404;
	constexpr int kConflict = 409;
	constexpr int kTooManyRequests = 429;
	constexpr int kSecondsPerMinute = 60;
	/* The timer fires this much after the expiry, so the invite is past it. */
	constexpr int kExpirySlackMs = 250;

	bool removeFrom(QList<FriendsApi::Invite>& list, const QString& uuid)
	{
		return list.removeIf([&](const auto& one) { return one.uuid == uuid; }) > 0;
	}
} // namespace

QVariantList FriendsController::invites() const
{
	return m_invites;
}

QVariantList FriendsController::outgoingInvites() const
{
	return m_outgoingInvites;
}

QVariantMap FriendsController::inviteTarget() const
{
	const FriendsApi::Presence presence = gamePresence();
	if (!m_enabled || !presence.playing ||
		presence.world.type != FriendsApi::World::Type::Server) {
		return {};
	}
	const QString host = FriendsApi::normalizeHost(presence.world.host);
	if (host.isEmpty() || FriendsApi::isLoopbackHost(host)) {
		return {};
	}
	const int port = presence.world.port > 0 ? presence.world.port
											 : FriendsInvites::kDefaultPort;
	return {{"host", host}, {"port", port}};
}

bool FriendsController::setInvites(const FriendsApi::InviteLists& lists)
{
	m_inviteData = lists;
	return publishInvites();
}

bool FriendsController::publishInvites()
{
	const QDateTime now = QDateTime::currentDateTimeUtc();
	m_inviteData.incoming = FriendsInvites::live(m_inviteData.incoming, now);
	m_inviteData.outgoing = FriendsInvites::live(m_inviteData.outgoing, now);
	QVariantList incoming = FriendsInvites::toList(m_inviteData.incoming);
	QVariantList outgoing = FriendsInvites::toList(m_inviteData.outgoing);
	attachFaces(incoming);
	attachFaces(outgoing);
	const bool same = incoming == m_invites && outgoing == m_outgoingInvites;
	m_invites = std::move(incoming);
	m_outgoingInvites = std::move(outgoing);

	const QDateTime next = FriendsInvites::nextExpiry(
		m_inviteData.incoming + m_inviteData.outgoing);
	if (next.isValid()) {
		m_inviteExpiryTimer.start(
			static_cast<int>(std::max<qint64>(0, now.msecsTo(next))) +
			kExpirySlackMs);
	} else {
		m_inviteExpiryTimer.stop();
	}
	return !same;
}

void FriendsController::onInviteExpiry()
{
	if (publishInvites()) {
		emit changed();
	}
}

void FriendsController::applyInviteEvent(const QJsonObject& event)
{
	if (!m_enabled) {
		return;
	}
	const QDateTime now = QDateTime::currentDateTimeUtc();
	const auto effect =
		FriendsInvites::applyEvent(m_inviteData.incoming, event, now);
	/* Someone who is no longer a friend can no longer be invited either. */
	const bool removedFriend =
		event.value("type").toString() == QLatin1String("friend.removed") &&
		removeFrom(m_inviteData.outgoing, event.value("uuid").toString());
	if (effect == FriendsInvites::EventEffect::Ignored && !removedFriend) {
		return;
	}
	if (effect == FriendsInvites::EventEffect::Added) {
		notifyInvites({m_inviteData.incoming.first()});
	}
	if (publishInvites()) {
		emit changed();
	}
}

QString FriendsController::inviteErrorText(const FriendsApi::Reply& reply,
										   const QString& name) const
{
	if (reply.networkFailed() || !reply.failureText.isEmpty()) {
		return failureText(reply);
	}
	const QString code = FriendsApi::parseError(reply.body).code;
	if (reply.status == kConflict && code == FriendsApi::Code::InvitesOff) {
		return tr("%1 doesn't take invites to servers").arg(name);
	}
	if (reply.status == kConflict &&
		code == FriendsApi::Code::AddressNotShared) {
		return tr("This server is reached by an IP address or is on your home "
				  "network. To invite friends to it, turn on \"Show servers I "
				  "join by IP address\" in Settings › Friends.");
	}
	if (reply.status == kNotFound) {
		return tr("%1 is not your friend").arg(name);
	}
	if (reply.status == kBadRequest) {
		return tr("That is not a server address friends can join");
	}
	if (reply.status == kTooManyRequests &&
		reply.retryAfterSeconds > kSecondsPerMinute) {
		return tr("You sent many invites; try again in %1 min")
			.arg((reply.retryAfterSeconds + kSecondsPerMinute - 1) /
				 kSecondsPerMinute);
	}
	if (reply.status == kTooManyRequests) {
		return tr("You sent many invites; try again in a minute");
	}
	return failureText(reply);
}

void FriendsController::sendInvite(const QString& uuid, const QString& address)
{
	if (!canAct("sendInvite", &uuid)) {
		return;
	}
	const MinecraftServerTarget target =
		MinecraftServerTarget::parse(address.trimmed(), false);
	const QString host = FriendsApi::normalizeHost(target.address);
	if (host.isEmpty()) {
		finishAction("sendInvite", false,
					 tr("That is not a server address friends can join"));
		return;
	}
	if (FriendsApi::isLoopbackHost(host)) {
		finishAction("sendInvite", false,
					 tr("That server runs on your own computer; friends "
						"can't join it"));
		return;
	}
	const QString name = nameOf(uuid);
	act("sendInvite", kPut, FriendsApi::Path::invite(uuid),
		FriendsApi::inviteBody(host, target.port),
		[this, name, uuid](const FriendsApi::Reply& reply, bool* ok) {
			const auto invite = reply.ok() ? FriendsInvites::parseBody(reply.body)
										   : std::nullopt;
			if (!invite) {
				return reply.ok() ? failureText(reply)
								  : inviteErrorText(reply, name);
			}
			*ok = true;
			removeFrom(m_inviteData.outgoing, uuid);
			m_inviteData.outgoing.prepend(*invite);
			if (publishInvites()) {
				emit changed();
			}
			return tr("Invited %1 to %2").arg(invite->name, invite->host);
		},
		false);
}

void FriendsController::dismissInvite(const QString& uuid)
{
	if (!canAct("dismissInvite", &uuid)) {
		return;
	}
	act("dismissInvite", kPost, FriendsApi::Path::dismissInvite(uuid),
		QByteArray(),
		[this, uuid](const FriendsApi::Reply& reply, bool* ok) {
			/* Gone already (ran out, or taken back) is what was asked for. */
			*ok = reply.ok() || reply.status == kNotFound;
			if (!*ok) {
				return failureText(reply);
			}
			if (removeFrom(m_inviteData.incoming, uuid) && publishInvites()) {
				emit changed();
			}
			return QString();
		},
		false);
}

void FriendsController::cancelInvite(const QString& uuid)
{
	if (!canAct("cancelInvite", &uuid)) {
		return;
	}
	const QString name = nameOf(uuid);
	act("cancelInvite", kDelete, FriendsApi::Path::invite(uuid), QByteArray(),
		[this, uuid, name](const FriendsApi::Reply& reply, bool* ok) {
			*ok = reply.ok() || reply.status == kNotFound;
			if (!*ok) {
				return failureText(reply);
			}
			if (removeFrom(m_inviteData.outgoing, uuid) && publishInvites()) {
				emit changed();
			}
			return tr("Invite to %1 taken back").arg(name);
		},
		false);
}

void FriendsController::send(const QByteArray& verb, const QString& path,
							 const QByteArray& body, FriendsHttp::Done done)
{
	if (!m_enabled) {
		FriendsApi::Reply off;
		off.failureText = tr("Friends is turned off");
		done(off);
		return;
	}
	request(verb, path, body, std::move(done));
}
