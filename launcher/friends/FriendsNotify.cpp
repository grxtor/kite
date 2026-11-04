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

#include "FriendsController.h"

#include "FriendsDisplay.h"
#include "core/LauncherContext.h"
#include "plugin/PluginHooks.h"
#include "settings/SettingsObject.h"

namespace
{
	QByteArray kindName(FriendsNotices::Notice::Kind kind)
	{
		switch (kind) {
			case FriendsNotices::Notice::Kind::Online:
				return "online";
			case FriendsNotices::Notice::Kind::Request:
				return "request";
			case FriendsNotices::Notice::Kind::Invite:
				break;
		}
		return "invite";
	}
} // namespace

void FriendsController::notifyChanges(
	const FriendsApi::FriendsSnapshot& snapshot, const QDateTime& now)
{
	/* The memory moves on even while notifications are off, so turning them
	 * on does not announce what changed in the meantime. */
	FriendsNotices::Diff diff =
		FriendsNotices::diff(m_noticeMemory, snapshot, now);
	m_noticeMemory = std::move(diff.memory);
	dispatchNotices(diff.notices, now);
}

void FriendsController::notifyInvites(const QList<FriendsApi::Invite>& incoming)
{
	QList<FriendsNotices::Notice> notices;
	for (const FriendsApi::Invite& invite : incoming) {
		if (auto notice = FriendsNotices::invite(m_noticeMemory, invite)) {
			notices.append(*notice);
		}
	}
	dispatchNotices(notices, QDateTime::currentDateTimeUtc());
}

void FriendsController::dispatchNotices(
	const QList<FriendsNotices::Notice>& notices, const QDateTime& now)
{
	if (notices.isEmpty() || !LAUNCHER ||
		!LAUNCHER->settings()->get(kNotifySetting).toBool()) {
		return;
	}
	using Kind = FriendsNotices::Notice::Kind;
	for (const FriendsNotices::Notice& notice : notices) {
		const QByteArray kind = kindName(notice.kind);
		const QByteArray uuid = notice.uuid.toUtf8();
		const QByteArray name = notice.name.toUtf8();
		QString title;
		QString body;
		switch (notice.kind) {
			case Kind::Online:
				title = tr("%1 is online").arg(notice.name);
				body = FriendsDisplay::statusText(notice.status, now);
				break;
			case Kind::Request:
				title = tr("%1 sent you a friend request").arg(notice.name);
				body = tr("Accept or decline it in Friends.");
				break;
			case Kind::Invite:
				title = tr("%1 invited you to %2")
							.arg(notice.name, notice.host);
				body = tr("Join from Friends within 10 minutes.");
				break;
		}
		/* Never empty, so a desktop always has a message to show. */
		const QByteArray titleBytes = title.toUtf8();
		const QByteArray bodyBytes = body.toUtf8();
		MMCOFriendsNotice payload{kind.constData(), uuid.constData(),
								  name.constData(), titleBytes.constData(),
								  bodyBytes.constData()};
		LAUNCHER->dispatchPluginHook(MMCO_HOOK_FRIENDS_NOTICE, &payload);
	}
}

void FriendsController::notifyMessage(const FriendsChatApi::Message& message)
{
	if (message.deleted || message.text.isEmpty() || !LAUNCHER ||
		!LAUNCHER->settings()->get(kMessageNotifySetting).toBool()) {
		return;
	}
	/* A notification shows a line or two; the rest waits in the chat. */
	constexpr int kBodyChars = 140;
	QString text = message.text;
	if (text.size() > kBodyChars) {
		text = text.left(kBodyChars - 1) + QChar(0x2026);
	}
	const QByteArray kind = "message";
	const QByteArray uuid = message.from.toUtf8();
	const QByteArray name = nameOf(message.from).toUtf8();
	const QByteArray title = name;
	const QByteArray body = text.toUtf8();
	MMCOFriendsNotice payload{kind.constData(), uuid.constData(), name.constData(),
							  title.constData(), body.constData()};
	LAUNCHER->dispatchPluginHook(MMCO_HOOK_FRIENDS_NOTICE, &payload);
}
