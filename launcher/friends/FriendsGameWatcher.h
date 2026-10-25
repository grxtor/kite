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

#include <QHash>
#include <QObject>
#include <QPointer>

#include <memory>
#include <vector>

#include "FriendsApi.h"

class BaseInstance;
class InstanceList;
class LaunchTask;

/*
 * What the launcher can say about the game right now: whether an instance
 * runs (the most recently started one, if several do), its name and
 * Minecraft version, and where in the world its log says the player is
 * (FriendsLogRules). Follows the instances of the list, including ones
 * added later, and says changed() when the answer does.
 *
 * A game counts once its launch is known (launchTaskChanged), and only for
 * the profile it is played as: a game played offline, as a demo or as another
 * account is nothing the signed-in account's friends are told about
 * (presence()).
 */
class FriendsGameWatcher : public QObject
{
	Q_OBJECT

  public:
	explicit FriendsGameWatcher(std::shared_ptr<InstanceList> instances,
								QObject* parent = nullptr);

	/// What friends of @p profileId (the profile Friends is signed in as)
	/// may be told: the latest running game when it is played as that
	/// profile, nothing otherwise.
	FriendsApi::Presence presence(const QString& profileId) const;

  signals:
	void changed();

  private:
	void watchAll();
	void watch(const std::shared_ptr<BaseInstance>& instance);
	void onRunningChanged(BaseInstance* instance, bool running);
	void onLaunchTask(BaseInstance* instance, LaunchTask* task);
	/// The launch of @p instance is known, played as @p profile ("" if not
	/// as an account's own identity): it counts as running if it does.
	void count(BaseInstance* instance, const QString& profile);
	/// The latest running game, whoever plays it.
	FriendsApi::Presence latest() const;
	void onLogLine(BaseInstance* instance, const QString& line);
	void forget(BaseInstance* instance);
	void removeRunning(BaseInstance* instance);
	void emitIfChanged();

  private:
	std::shared_ptr<InstanceList> m_instances;
	/// Instances connected to, with their world while they run.
	QHash<BaseInstance*, FriendsApi::World> m_worlds;
	/// The profile each instance with a known launch is played as, while it
	/// runs ("" when not as an account's own identity).
	QHash<BaseInstance*, QString> m_profiles;
	/// The running ones with a known launch, oldest first.
	std::vector<BaseInstance*> m_running;
	/// What changed() last said: the latest game and who plays it.
	FriendsApi::Presence m_last;
	QString m_lastProfile;
};
