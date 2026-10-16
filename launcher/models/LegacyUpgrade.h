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

#include <QString>

#include "BaseInstance.h"

class LegacyUpgradeTask;

/*
 * Upgrading an old-format (Legacy) instance to the current instance format -
 * the widget-free half of LegacyUpgradePage, which also draws the
 * explanation and runs the task behind a modal ProgressDialog.
 *
 * The upgrade copies the instance into a new one ("<name> (Migrated)", same
 * group and icon); the original stays untouched. The caller wraps the task
 * with InstanceList::wrapInstanceTask() so the copy lands in the library,
 * and watches it (see InstanceDetails::upgradeLegacy()).
 */
namespace LegacyUpgrade
{
	/// Whether @p instance is an old-format (Legacy) instance.
	bool isLegacy(const InstancePtr& instance);

	/// The name the upgraded copy gets.
	QString upgradedName(const QString& name);

	/// The upgrade task for @p instance, already named, in @p group and with
	/// the instance's icon. Null if @p instance is not a legacy instance or
	/// is running (the old page was not shown for a running instance either:
	/// the files would be changing under the copy).
	LegacyUpgradeTask* makeTask(const InstancePtr& instance,
								const QString& group);
} // namespace LegacyUpgrade
