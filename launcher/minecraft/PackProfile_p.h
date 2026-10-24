/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
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

#include "Component.h"
#include "RuntimeContext.h"
#include <map>
#include <QTimer>
#include <QList>
#include <QMap>

class MinecraftInstance;
using ComponentContainer = QList<ComponentPtr>;
using ComponentIndex = QMap<QString, ComponentPtr>;

struct PackProfileData {
	// the instance this belongs to
	MinecraftInstance* m_instance;

	// the launch profile (volatile, temporary thing created on demand)
	std::shared_ptr<LaunchProfile> m_profile;
	// what m_profile was made for; another Java may need other libraries
	RuntimeContext m_profileContext;

	// version information migrated from instance.cfg file. Single use on
	// migration!
	std::map<QString, QString> m_oldConfigVersions;
	QString getOldConfigVersion(const QString& uid) const
	{
		const auto iter = m_oldConfigVersions.find(uid);
		if (iter != m_oldConfigVersions.cend()) {
			return (*iter).second;
		}
		return QString();
	}

	// persistent list of components and related machinery
	ComponentContainer components;
	ComponentIndex componentIndex;
	bool dirty = false;
	QTimer m_saveTimer;
	Task::Ptr m_updateTask;
	bool loaded = false;
	bool interactionDisabled = true;
};
