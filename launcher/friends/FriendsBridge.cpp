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
 * FriendsController's side of the Kite mod bridge (modbridge/): the cache as
 * the mod is told it, its refresh, the faces it may load and the worlds it
 * reports.
 */

#include "FriendsController.h"

#include "FriendsFaces.h"
#include "FriendsGameWatcher.h"

namespace
{
	/* The mod asks to refresh as often as its player presses a button. */
	constexpr int kMinBridgeRefreshGapMs = 10 * 1000;
} // namespace

ModBridgeJson::FriendsView FriendsController::bridgeView()
{
	ModBridgeJson::FriendsView view;
	view.enabled = m_enabled;
	view.state = ModBridgeJson::friendsState(state(), m_needsSignIn);
	/* Ready before the first list arrived would read as "no friends". */
	if (view.state == QLatin1String("ready") && !m_loaded) {
		view.state = QStringLiteral("connecting");
	}
	view.meName = m_session.name;
	view.meUuid = m_session.uuid;
	view.incomingCount = static_cast<int>(m_incoming.size());
	view.friends = m_friends;
	/* The statuses are worded afresh at every poll, so a rev that followed
	 * the poll would change every minute: it follows what the mod is told. */
	const QByteArray digest = ModBridgeJson::friendsBody(view);
	if (digest != m_bridgeDigest) {
		m_bridgeDigest = digest;
		++m_bridgeRev;
	}
	view.rev = m_bridgeRev;
	view.updatedAtMs = updatedAtMs();
	return view;
}

void FriendsController::bridgeRefresh()
{
	if (!m_enabled || (m_lastBridgeRefresh.isValid() &&
					   m_lastBridgeRefresh.elapsed() < kMinBridgeRefreshGapMs)) {
		return;
	}
	m_lastBridgeRefresh.restart();
	tick();
}

QString FriendsController::bridgeFacePath(const QString& uuid) const
{
	const QString wanted = FriendsApi::undashed(uuid);
	for (const QVariant& one : m_friends) {
		const QString friendUuid = one.toMap().value("uuid").toString();
		if (FriendsApi::undashed(friendUuid) == wanted) {
			return m_faces->cachedFacePath(friendUuid);
		}
	}
	return QString();
}

void FriendsController::reportWorld(BaseInstance* instance,
									const FriendsApi::World& world)
{
	if (m_watcher) {
		m_watcher->setReportedWorld(instance, world);
	}
}
