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

void FriendsController::notifyChanges(
	const FriendsApi::FriendsSnapshot& snapshot, const QDateTime& now)
{
	/* The memory moves on even while notifications are off, so turning them
	 * on does not announce what changed in the meantime. */
	FriendsNotices::Diff diff =
		FriendsNotices::diff(m_noticeMemory, snapshot, now);
	m_noticeMemory = std::move(diff.memory);
	if (diff.notices.isEmpty() || !LAUNCHER ||
		!LAUNCHER->settings()->get(kNotifySetting).toBool()) {
		return;
	}
	for (const FriendsNotices::Notice& notice : diff.notices) {
		const bool online =
			notice.kind == FriendsNotices::Notice::Kind::Online;
		const QByteArray kind = online ? "online" : "request";
		const QByteArray uuid = notice.uuid.toUtf8();
		const QByteArray name = notice.name.toUtf8();
		const QByteArray title =
			(online ? tr("%1 is online") : tr("%1 sent you a friend request"))
				.arg(notice.name)
				.toUtf8();
		/* Never empty, so a desktop always has a message to show. */
		const QByteArray body =
			(online ? FriendsDisplay::statusText(notice.status, now)
					: tr("Accept or decline it in Friends."))
				.toUtf8();
		MMCOFriendsNotice payload{kind.constData(), uuid.constData(),
								  name.constData(), title.constData(),
								  body.constData()};
		LAUNCHER->dispatchPluginHook(MMCO_HOOK_FRIENDS_NOTICE, &payload);
	}
}
