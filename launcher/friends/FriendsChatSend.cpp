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
 * FriendsChat: sending (over the socket, or HTTP while it is down), what the
 * server answers, the frames it pushes, and reporting a message.
 */

#include "FriendsChat.h"

#include <algorithm>

using FriendsChatApi::idLess;

namespace
{
	const QByteArray kPost = "POST";
	constexpr int kReportedStatus = 204;
	/* A refusal without Retry-After waits this long. */
	constexpr int kDefaultRateLimitSeconds = 60;
	const QString kRateLimited = QStringLiteral("rate_limited");
} // namespace

bool FriendsChat::send(const QString& text)
{
	if (m_openUuid.isEmpty() || !FriendsChatApi::sendable(text) || rateLimitedUntilMs() > 0) {
		return false;
	}
	const int n = ++m_nextLocal;
	ChatEntry entry;
	entry.key = QStringLiteral("local-%1").arg(n);
	entry.ref = QStringLiteral("s%1").arg(n);
	entry.mine = true;
	/* What the server keeps, so the line does not change once it answers. */
	entry.text = FriendsChatApi::cleanText(text);
	entry.createdAt = QDateTime::currentDateTimeUtc();
	entry.state = ChatEntry::State::Sending;
	const QString key = entry.key;
	const QString uuid = m_openUuid;
	insertEntry(uuid, static_cast<int>(conversation(uuid).entries.size()), std::move(entry));
	deliver(uuid, key);
	return true;
}

void FriendsChat::retry(const QString& key)
{
	Conversation* conv = find(m_openUuid);
	const int at = conv ? indexOfKey(*conv, key) : -1;
	if (at < 0 || conv->entries.at(at).state != ChatEntry::State::Failed ||
		rateLimitedUntilMs() > 0) {
		return;
	}
	const QString text = conv->entries.at(at).text;
	removeEntry(m_openUuid, at);
	send(text);
}

void FriendsChat::discard(const QString& key)
{
	Conversation* conv = find(m_openUuid);
	const int at = conv ? indexOfKey(*conv, key) : -1;
	if (at >= 0 && conv->entries.at(at).state == ChatEntry::State::Failed) {
		removeEntry(m_openUuid, at);
	}
}

void FriendsChat::deliver(const QString& uuid, const QString& key)
{
	Conversation* conv = find(uuid);
	const int at = conv ? indexOfKey(*conv, key) : -1;
	if (at < 0) {
		return;
	}
	const ChatEntry& entry = conv->entries.at(at);
	const QString ref = entry.ref;
	if (m_transport.sendFrame(FriendsChatApi::sendFrame(ref, uuid, entry.text))) {
		/* A frame over the socket's limit is dropped without an answer. */
		QTimer::singleShot(kSendTimeoutMs, this,
						   [this, uuid, key, ref]() { failIfSending(uuid, key, ref); });
		return;
	}
	const int gen = m_gen;
	m_transport.request(kPost, FriendsChatApi::Path::messages(uuid),
						FriendsChatApi::sendBody(entry.text),
						[this, uuid, key, gen](const FriendsApi::Reply& reply) {
							onHttpSent(uuid, key, gen, reply);
						});
}

bool FriendsChat::handleFrame(const QJsonObject& frame)
{
	const QString type = frame.value("type").toString();
	const auto message = [&frame]() {
		return FriendsChatApi::parseMessage(frame.value("message").toObject());
	};
	if (type == QLatin1String("chat.message")) {
		if (const auto one = message()) {
			onIncoming(*one);
		}
	} else if (type == QLatin1String("chat.sent")) {
		if (const auto one = message()) {
			onSent(frame.value("ref").toString(), *one);
		}
	} else if (type == QLatin1String("chat.read")) {
		/* With a ref it answers our own marker; without, another socket moved it. */
		if (frame.value("ref").toString().isEmpty()) {
			emit readElsewhere(frame.value("with").toString().toLower());
		}
	} else if (type == QLatin1String("chat.deleted")) {
		markDeleted(frame.value("uuid").toString().toLower());
	} else if (type == QLatin1String("error")) {
		onFrameError(FriendsChatApi::parseFrameError(frame));
	} else {
		return false;
	}
	return true;
}

void FriendsChat::onIncoming(const FriendsChatApi::Message& message)
{
	const bool mine = message.from == m_me;
	const QString other = mine ? message.to : message.from;
	if (!addStored(other, message) || mine) {
		return;
	}
	emit messageReceived(message);
	if (isViewing(other)) {
		scheduleRead();
	}
}

void FriendsChat::onSent(const QString& ref, const FriendsChatApi::Message& message)
{
	QString uuid;
	QString key;
	if (findRef(ref, &uuid, &key)) {
		Conversation& conv = conversation(uuid);
		const int at = indexOfKey(conv, key);
		const bool newest = newestStoredId(conv).isEmpty() ||
							idLess(newestStoredId(conv), message.id);
		if (newest && indexOfKey(conv, message.id) < 0) {
			/* Where it already stands: kept in place, so the line does not jump. */
			ChatEntry& entry = conv.entries[at];
			entry.key = message.id;
			entry.id = message.id;
			entry.ref.clear();
			entry.text = message.text;
			entry.createdAt = message.createdAt;
			entry.state = ChatEntry::State::Sent;
			entryChanged(uuid, at);
			return;
		}
		removeEntry(uuid, at);
	}
	addStored(message.to, message);
}

void FriendsChat::onHttpSent(const QString& uuid, const QString& key, int gen,
							 const FriendsApi::Reply& reply)
{
	if (gen != m_gen) {
		return;
	}
	if (const auto message = reply.ok() ? FriendsChatApi::parseMessageBody(reply.body)
										: std::nullopt) {
		Conversation* conv = find(uuid);
		const int at = conv ? indexOfKey(*conv, key) : -1;
		if (at >= 0) {
			removeEntry(uuid, at);
		}
		addStored(uuid, *message);
		return;
	}
	if (reply.status == 429) {
		noteRateLimit(reply.retryAfterSeconds);
		fail(uuid, key, sendErrorText(kRateLimited));
		return;
	}
	if (!reply.failureText.isEmpty()) {
		fail(uuid, key, reply.failureText);
		return;
	}
	fail(uuid, key, reply.networkFailed()
						? tr("Not sent. Check your connection.")
						: sendErrorText(FriendsApi::parseError(reply.body).code));
}

void FriendsChat::onFrameError(const FriendsChatApi::FrameError& error)
{
	if (error.code == kRateLimited) {
		noteRateLimit(error.retryAfterSeconds);
	}
	QString uuid;
	QString key;
	if (findRef(error.ref, &uuid, &key)) {
		fail(uuid, key, sendErrorText(error.code));
	}
}

void FriendsChat::fail(const QString& uuid, const QString& key, const QString& why)
{
	Conversation* conv = find(uuid);
	const int at = conv ? indexOfKey(*conv, key) : -1;
	if (at < 0) {
		return;
	}
	ChatEntry& entry = conv->entries[at];
	entry.state = ChatEntry::State::Failed;
	entry.ref.clear();
	entry.error = why;
	entryChanged(uuid, at);
}

void FriendsChat::failIfSending(const QString& uuid, const QString& key, const QString& ref)
{
	Conversation* conv = find(uuid);
	const int at = conv ? indexOfKey(*conv, key) : -1;
	if (at >= 0 && conv->entries.at(at).state == ChatEntry::State::Sending &&
		conv->entries.at(at).ref == ref) {
		fail(uuid, key, tr("Not sent. Check your connection."));
	}
}

bool FriendsChat::findRef(const QString& ref, QString* uuid, QString* key) const
{
	if (ref.isEmpty()) {
		return false;
	}
	for (const auto& [owner, conv] : m_conversations) {
		for (const ChatEntry& entry : conv.entries) {
			if (!entry.stored() && entry.ref == ref) {
				*uuid = owner;
				*key = entry.key;
				return true;
			}
		}
	}
	return false;
}

void FriendsChat::noteRateLimit(int retryAfterSeconds)
{
	const int seconds = retryAfterSeconds > 0 ? retryAfterSeconds : kDefaultRateLimitSeconds;
	m_rateLimitedUntil = QDateTime::currentDateTimeUtc().addSecs(seconds);
	emit stateChanged();
	/* rateLimitedUntilMs goes back to 0 then; QML hears of it. */
	QTimer::singleShot(seconds * 1000, this, [this]() { emit stateChanged(); });
}

QString FriendsChat::sendErrorText(const QString& code) const
{
	if (code == kRateLimited) {
		return tr("Not sent: you are sending messages too fast.");
	}
	if (code == FriendsApi::Code::NotFound) {
		return tr("Not sent: you can only message your friends.");
	}
	if (code == FriendsApi::Code::BadRequest) {
		return tr("Not sent: the message is empty or too long.");
	}
	return tr("Not sent. Try again.");
}

void FriendsChat::report(const QString& messageId, const QString& reason)
{
	const auto why = FriendsChatApi::reasonOf(reason);
	if (m_openUuid.isEmpty() || !why || messageId.isEmpty()) {
		m_transport.finished(QStringLiteral("report"), false,
							 tr("This message can't be reported."));
		return;
	}
	m_transport.request(kPost, FriendsChatApi::Path::report(m_openUuid, messageId),
						FriendsChatApi::reportBody(*why),
						[this](const FriendsApi::Reply& reply) { reportAnswered(reply); });
}

void FriendsChat::reportAnswered(const FriendsApi::Reply& reply)
{
	QString message;
	if (reply.status == kReportedStatus) {
		message = tr("Reported. Kite keeps a copy of this message for its moderators.");
	} else if (reply.status == 404) {
		message = tr("This message can no longer be reported.");
	} else if (reply.status == 429) {
		message = tr("You have reported too many messages today.");
	} else if (!reply.failureText.isEmpty()) {
		message = reply.failureText;
	} else {
		message = tr("The report could not be sent; try again.");
	}
	m_transport.finished(QStringLiteral("report"), reply.status == kReportedStatus, message);
}
