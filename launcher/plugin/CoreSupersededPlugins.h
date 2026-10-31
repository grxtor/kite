/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include <QLatin1String>
#include <QString>

/* Plugins that graduated into core. Their old .mmco files may still sit in a user's mmcmodules/ (or
 * a lagging distro package), and loading one would double up on what core does: two Backups tabs,
 * two pre-launch snapshots, two news dialogs. The loader refuses to initialise them and reports why
 * in Settings. Matching is case-insensitive on the declared name and the file name, so a
 * renamed .mmco cannot slip past. Add an entry in the same commit as the core implementation; keep
 * the detail string short, it is shown verbatim next to the module name. */

struct CoreSupersededPlugin {
	QLatin1String moduleName;
	QLatin1String detail;
};

/* Returns the matching entry, or nullptr when `name` is not a module core has taken over. */
inline const CoreSupersededPlugin*
findCoreSupersededPlugin(const QString& name)
{
	static const CoreSupersededPlugin entries[] = {
		{QLatin1String("Filelink"),
		 QLatin1String("The shortcut system is now part of Kite itself")},
		{QLatin1String("BackupSystem"),
		 QLatin1String("Instance backups are now part of Kite itself "
					   "(instance settings -> Backups)")},
		{QLatin1String("NewsViewer"),
		 QLatin1String("The news viewer is now part of Kite itself "
					   "(the news bar headline and \"More news\")")},
		{QLatin1String("PackPortal"),
		 QLatin1String("The pack portal is now part of Kite itself")},
		{QLatin1String("PackUpdater"),
		 QLatin1String("The pack updater is now part of Kite itself")},
		{QLatin1String("SkinManager"),
		 QLatin1String("The skin manager is now part of Kite itself")},
	};

	if (name.isEmpty())
		return nullptr;

	for (const auto& entry : entries) {
		if (name.compare(entry.moduleName, Qt::CaseInsensitive) == 0)
			return &entry;
	}
	return nullptr;
}
