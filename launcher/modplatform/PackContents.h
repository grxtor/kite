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
#include <QStringList>

/* What a modpack version put into an instance. An in-place update merges
 * over the old version but removes nothing, so a dropped mod would keep
 * loading. Each install therefore records the files it is responsible for
 * (not "everything in mods/", which may hold the user's own), and the next
 * update removes those it no longer ships. The list is a flat set of
 * paths relative to the game directory. */
namespace PackContents
{
	/* The list's location, beside instance.cfg rather than in the game
	 * directory, as launcher bookkeeping the game never sees. */
	QString listPath(const QString& instanceRoot);

	/* Records @p gameRelativePaths as the contents of the version just
	 * installed into @p instanceRoot. Paths are normalised, de-duplicated and
	 * sorted, so identical installs give identical files. */
	bool write(const QString& instanceRoot,
			   const QStringList& gameRelativePaths);

	/* Reads back a recorded list. False when nothing usable: no list (an
	 * install from before this was recorded, or by another launcher) or an
	 * unknown format. Unlike an empty list, no list means nothing may be
	 * deleted. */
	bool read(const QString& instanceRoot, QStringList& out);

	/* A path in canonical form: forward slashes, no redundant separators,
	 * relative to the game directory. Empty for absolute paths and paths
	 * climbing out with "..": they are dropped, not corrected, since the list
	 * feeds a delete loop. */
	QString normalizePath(const QString& path);

	/* The files @p oldPaths installed that @p newPaths does not, as normalised
	 * game-relative paths. Each entry brings its enabled/disabled counterpart
	 * along, under its install-time name. Nothing the new version ships is
	 * returned, even via that rule: a required-to-optional update renames
	 * "mods/x.jar" to "mods/x.jar.disabled". */
	QStringList staleEntries(const QStringList& oldPaths,
							 const QStringList& newPaths);
} // namespace PackContents
