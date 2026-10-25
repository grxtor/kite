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

#include "FriendsGameWatcher.h"

#include <algorithm>

#include "BaseInstance.h"
#include "InstanceList.h"
#include "FriendsLogRules.h"
#include "launch/LaunchTask.h"

void FriendsGameWatcher::removeRunning(BaseInstance* instance)
{
	m_running.erase(std::remove(m_running.begin(), m_running.end(), instance),
					m_running.end());
}

FriendsGameWatcher::FriendsGameWatcher(std::shared_ptr<InstanceList> instances,
									   QObject* parent)
	: QObject(parent), m_instances(std::move(instances))
{
	if (!m_instances) {
		return;
	}
	connect(m_instances.get(), &QAbstractItemModel::rowsInserted, this,
			&FriendsGameWatcher::watchAll);
	connect(m_instances.get(), &QAbstractItemModel::modelReset, this,
			&FriendsGameWatcher::watchAll);
	watchAll();
}

FriendsApi::Presence FriendsGameWatcher::presence(const QString& profileId) const
{
	if (m_running.empty()) {
		return {};
	}
	const QString played =
		FriendsApi::undashed(m_profiles.value(m_running.back()));
	if (played.isEmpty() || played != FriendsApi::undashed(profileId)) {
		return {};
	}
	return latest();
}

FriendsApi::Presence FriendsGameWatcher::latest() const
{
	FriendsApi::Presence presence;
	if (m_running.empty()) {
		return presence;
	}
	BaseInstance* instance = m_running.back();
	presence.playing = true;
	presence.instance = instance->name();
	presence.version = instance->gameVersion();
	presence.world = m_worlds.value(instance);
	return presence;
}

void FriendsGameWatcher::watchAll()
{
	for (int i = 0; i < m_instances->count(); ++i) {
		watch(m_instances->at(i));
	}
}

void FriendsGameWatcher::watch(const std::shared_ptr<BaseInstance>& instance)
{
	BaseInstance* raw = instance.get();
	if (!raw || m_worlds.contains(raw)) {
		return;
	}
	m_worlds.insert(raw, {});
	connect(raw, &BaseInstance::runningStatusChanged, this,
			[this, raw](bool running) { onRunningChanged(raw, running); });
	connect(raw, &BaseInstance::launchTaskChanged, this,
			[this, raw](shared_qobject_ptr<LaunchTask> task) {
				onLaunchTask(raw, task.get());
			});
	connect(raw, &QObject::destroyed, this, [this, raw]() { forget(raw); });
	/* A game already running when the watcher is made; its launch is known as
	 * far as the instance still holds it. */
	if (raw->isRunning()) {
		auto task = raw->getLaunchTask();
		if (task) {
			onLaunchTask(raw, task.get());
		} else {
			count(raw, QString());
		}
	}
}

void FriendsGameWatcher::onRunningChanged(BaseInstance* instance, bool running)
{
	removeRunning(instance);
	m_worlds[instance] = {};
	if (!running) {
		m_profiles.remove(instance);
	} else if (m_profiles.contains(instance)) {
		/* The launch was announced first. */
		m_running.push_back(instance);
	}
	/* Otherwise it counts once its launch is announced (create() of a
	 * LaunchTask sets the instance running before createLaunchTask() says
	 * who plays it; reporting in between would say "online" for a moment). */
	emitIfChanged();
}

void FriendsGameWatcher::count(BaseInstance* instance, const QString& profile)
{
	m_profiles[instance] = profile;
	if (instance->isRunning() &&
		std::find(m_running.begin(), m_running.end(), instance) ==
			m_running.end()) {
		m_running.push_back(instance);
	}
	emitIfChanged();
}

void FriendsGameWatcher::onLaunchTask(BaseInstance* instance, LaunchTask* task)
{
	if (!task) {
		return;
	}
	connect(task, &LaunchTask::logLineReceived, this,
			[this, instance](const QString& line) {
				onLogLine(instance, line);
			});
	count(instance, task->playerProfile());
}

void FriendsGameWatcher::onLogLine(BaseInstance* instance, const QString& line)
{
	const auto change = FriendsLogRules::fromLine(line);
	if (change.kind == FriendsLogRules::Change::Kind::None) {
		return;
	}
	auto it = m_worlds.find(instance);
	if (it == m_worlds.end()) {
		return;
	}
	*it = FriendsLogRules::apply(change, *it);
	emitIfChanged();
}

void FriendsGameWatcher::forget(BaseInstance* instance)
{
	removeRunning(instance);
	m_worlds.remove(instance);
	m_profiles.remove(instance);
	emitIfChanged();
}

void FriendsGameWatcher::emitIfChanged()
{
	const auto now = latest();
	const QString profile =
		m_running.empty() ? QString() : m_profiles.value(m_running.back());
	if (now == m_last && profile == m_lastProfile) {
		return;
	}
	m_last = now;
	m_lastProfile = profile;
	emit changed();
}
