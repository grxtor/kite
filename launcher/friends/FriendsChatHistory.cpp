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
 * FriendsChat: reading a conversation's history (the newest page, older
 * pages, the catch-up after the socket was down) and merging every message
 * that arrives into it, in id order with ours that are not stored yet last.
 */

#include "FriendsChat.h"

using FriendsChatApi::idLess;

namespace
{
	const QByteArray kGet = "GET";
} // namespace

void FriendsChat::loadNewest(const QString& uuid)
{
	Conversation& conv = conversation(uuid);
	conv.loading = true;
	conv.loadError.clear();
	emit stateChanged();
	const int gen = m_gen;
	m_transport.request(kGet,
						FriendsChatApi::Path::page(uuid, QString(), QString(),
												   FriendsChatApi::kPageLimit),
						QByteArray(), [this, uuid, gen](const FriendsApi::Reply& reply) {
							onNewest(uuid, gen, reply);
						});
}

void FriendsChat::onNewest(const QString& uuid, int gen,
						   const FriendsApi::Reply& reply)
{
	Conversation* conv = gen == m_gen ? find(uuid) : nullptr;
	if (!conv) {
		return;
	}
	conv->loading = false;
	const auto page = reply.ok() ? FriendsChatApi::parsePage(reply.body) : std::nullopt;
	if (!page) {
		conv->loadError = loadErrorText(reply);
		emit stateChanged();
		return;
	}
	for (const auto& message : page->messages) {
		addStored(uuid, message);
	}
	/* Read before, then again: whether older messages exist is this page's to say. */
	if (!conv->loaded) {
		conv->hasOlder = page->more;
	}
	conv->loaded = true;
	emit stateChanged();
	scheduleRead();
}

void FriendsChat::loadOlder()
{
	Conversation* conv = find(m_openUuid);
	if (!conv || !conv->loaded || !conv->hasOlder || conv->loadingOlder) {
		return;
	}
	QString oldest;
	for (const ChatEntry& entry : conv->entries) {
		if (entry.stored()) {
			oldest = entry.id;
			break;
		}
	}
	if (oldest.isEmpty()) {
		return;
	}
	conv->loadingOlder = true;
	emit stateChanged();
	const QString uuid = m_openUuid;
	const int gen = m_gen;
	m_transport.request(
		kGet, FriendsChatApi::Path::page(uuid, oldest, QString(), FriendsChatApi::kPageLimit),
		QByteArray(), [this, uuid, gen](const FriendsApi::Reply& reply) {
			Conversation* conv = gen == m_gen ? find(uuid) : nullptr;
			if (!conv) {
				return;
			}
			conv->loadingOlder = false;
			if (const auto page = reply.ok() ? FriendsChatApi::parsePage(reply.body)
											 : std::nullopt) {
				for (const auto& message : page->messages) {
					addStored(uuid, message);
				}
				conv->hasOlder = page->more;
			}
			emit stateChanged();
		});
}

void FriendsChat::catchUp()
{
	const Conversation* conv = openConversation();
	if (!conv || conv->loading) {
		return;
	}
	if (!conv->loaded) {
		loadNewest(m_openUuid);
		return;
	}
	catchUpPage(m_openUuid, m_gen, 0);
}

void FriendsChat::catchUpPage(const QString& uuid, int gen, int page)
{
	Conversation* conv = gen == m_gen ? find(uuid) : nullptr;
	if (!conv) {
		return;
	}
	const QString after = newestStoredId(*conv);
	if (after.isEmpty() || page >= kMaxCatchUpPages) {
		/* Nothing to go on from, or too far behind: the newest page is enough. */
		loadNewest(uuid);
		return;
	}
	m_transport.request(
		kGet, FriendsChatApi::Path::page(uuid, QString(), after, FriendsChatApi::kMaxPageLimit),
		QByteArray(), [this, uuid, gen, page](const FriendsApi::Reply& reply) {
			const auto got = gen == m_gen && reply.ok()
								 ? FriendsChatApi::parsePage(reply.body)
								 : std::nullopt;
			if (!got || !find(uuid)) {
				return;
			}
			for (const auto& message : got->messages) {
				addStored(uuid, message);
			}
			if (got->more) {
				catchUpPage(uuid, gen, page + 1);
			}
			scheduleRead();
		});
}

bool FriendsChat::addStored(const QString& uuid, const FriendsChatApi::Message& message)
{
	Conversation& conv = conversation(uuid);
	const bool mine = message.from == m_me;
	const int known = indexOfKey(conv, message.id);
	if (known >= 0) {
		/* Seen already; it may have been deleted since. */
		if (message.deleted && !conv.entries.at(known).deleted) {
			conv.entries[known].deleted = true;
			conv.entries[known].text.clear();
			entryChanged(uuid, known);
		}
		return false;
	}
	int at = 0;
	bool replaced = false;
	for (int i = static_cast<int>(conv.entries.size()) - 1; i >= 0; --i) {
		const ChatEntry& entry = conv.entries.at(i);
		/* Ours on its way with the same words is this one: its answer was lost with
		 * the socket, or it went out from another of our sockets. */
		if (mine && !replaced && !entry.stored() && entry.text == message.text) {
			removeEntry(uuid, i);
			replaced = true;
			continue;
		}
		if (entry.stored() && !idLess(message.id, entry.id)) {
			at = i + 1;
			break;
		}
	}
	ChatEntry entry;
	entry.key = message.id;
	entry.id = message.id;
	entry.mine = mine;
	entry.text = message.text;
	entry.createdAt = message.createdAt;
	entry.deleted = message.deleted;
	insertEntry(uuid, at, std::move(entry));
	if (!mine && idLess(conv.theirNewest, message.id)) {
		conv.theirNewest = message.id;
	}
	return true;
}

void FriendsChat::insertEntry(const QString& uuid, int at, ChatEntry entry)
{
	const bool shown = uuid == m_openUuid;
	if (shown) {
		m_model->beginInsert(at);
	}
	conversation(uuid).entries.insert(at, std::move(entry));
	if (shown) {
		m_model->endInsert(at);
	}
}

void FriendsChat::removeEntry(const QString& uuid, int at)
{
	const bool shown = uuid == m_openUuid;
	if (shown) {
		m_model->beginRemove(at);
	}
	conversation(uuid).entries.removeAt(at);
	if (shown) {
		m_model->endRemove(at);
	}
}

void FriendsChat::entryChanged(const QString& uuid, int at)
{
	if (uuid == m_openUuid) {
		m_model->changed(at);
	}
}

int FriendsChat::indexOfKey(const Conversation& conv, const QString& key) const
{
	for (int i = 0; i < conv.entries.size(); ++i) {
		if (conv.entries.at(i).key == key) {
			return i;
		}
	}
	return -1;
}

QString FriendsChat::newestStoredId(const Conversation& conv) const
{
	for (int i = static_cast<int>(conv.entries.size()) - 1; i >= 0; --i) {
		if (conv.entries.at(i).stored()) {
			return conv.entries.at(i).id;
		}
	}
	return QString();
}

void FriendsChat::markDeleted(const QString& uuid)
{
	Conversation* conv = find(uuid);
	for (int i = 0; conv && i < conv->entries.size(); ++i) {
		ChatEntry& entry = conv->entries[i];
		if (!entry.mine && !entry.deleted) {
			entry.deleted = true;
			entry.text.clear();
			entryChanged(uuid, i);
		}
	}
}

QString FriendsChat::loadErrorText(const FriendsApi::Reply& reply) const
{
	if (!reply.failureText.isEmpty()) {
		return reply.failureText;
	}
	if (reply.networkFailed()) {
		return tr("Kite's friends server can't be reached.");
	}
	if (reply.status == 429) {
		return tr("Too many requests; try again in a minute.");
	}
	return tr("These messages could not be loaded.");
}
