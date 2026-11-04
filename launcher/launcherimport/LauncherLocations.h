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

#include <QList>
#include <QString>
#include <QStringList>

#include "LauncherImportTypes.h"

namespace LauncherImport
{
	/* The folders other launchers keep their data in are the same for every
	 * user, under a few base folders; this holds those bases. */
	struct SearchEnvironment {
		enum class System {
			MacOS,
			Windows,
			Linux
		};
		System system = System::Linux;
		QString home;
		/* %APPDATA% (Roaming) on Windows. */
		QString appData;
		/* %LOCALAPPDATA% on Windows. */
		QString localAppData;
		/* $XDG_DATA_HOME, else ~/.local/share, on Linux. */
		QString dataHome;

		/* This computer's. KITE_LAUNCHER_IMPORT_HOME replaces the home folder
		 * (and the bases under it), so a review run looks at a made-up one. */
		static SearchEnvironment current();
		/* The bases a system has under @p home, as on a default install. */
		static SearchEnvironment underHome(const QString& home, System system);
	};

	/* Where @p source keeps its data on @p env's system, most likely first.
	 * For CurseForge it is the Instances folder itself. Folders that do not
	 * exist are included; the scan checks. */
	QStringList candidateDirs(Source source, const SearchEnvironment& env);

	/* Every source, in the order the list shows them. */
	QList<Source> allSources();
} // namespace LauncherImport
