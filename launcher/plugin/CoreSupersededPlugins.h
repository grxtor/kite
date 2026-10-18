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

/*
 * ─── Plugins that graduated into core ─────────────────────────────────
 *
 * Every module listed here once shipped as an in-tree .mmco plugin and
 * has since been absorbed into the launcher binary. Their old .mmco
 * files may still be sitting in a user's mmcmodules/ directory (or in a
 * distro package that has not caught up yet), and loading one now would
 * double up on everything core already does: two "Backups" tabs in the
 * instance dialog, two pre-launch snapshots per launch, two news
 * dialogs fighting over the same toolbar button.
 *
 * So the loader refuses to initialise them and reports the reason in
 * the plugins dialog instead of silently ignoring the file.
 *
 * Matching is case-insensitive and checks both the module's declared
 * name (MMCO_DEFINE_MODULE) and its file name, because a renamed .mmco
 * would otherwise slip past.
 *
 * ── Adding an entry ──
 * When a plugin graduates, append it here in the same commit that adds
 * its core implementation. Keep the detail string short: it is shown
 * verbatim in the plugins dialog next to the module name.
 */

struct CoreSupersededPlugin {
	QLatin1String moduleName;
	QLatin1String detail;
};

/*
 * Returns the matching entry, or nullptr when `name` is not a module
 * that core has taken over.
 */
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
