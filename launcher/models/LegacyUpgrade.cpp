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

#include "LegacyUpgrade.h"

#include <QObject>

#include "minecraft/legacy/LegacyInstance.h"
#include "minecraft/legacy/LegacyUpgradeTask.h"

namespace LegacyUpgrade
{
	bool isLegacy(const InstancePtr& instance)
	{
		return std::dynamic_pointer_cast<LegacyInstance>(instance) != nullptr;
	}

	QString upgradedName(const QString& name)
	{
		return QObject::tr("%1 (Migrated)").arg(name);
	}

	LegacyUpgradeTask* makeTask(const InstancePtr& instance,
								const QString& group)
	{
		if (!isLegacy(instance) || instance->isRunning()) {
			return nullptr;
		}
		auto* task = new LegacyUpgradeTask(instance);
		task->setName(upgradedName(instance->name()));
		task->setGroup(group);
		task->setIcon(instance->iconKey());
		return task;
	}
} // namespace LegacyUpgrade
