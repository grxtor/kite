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

#include <QCoreApplication>
#include <QFileInfo>
#include <QString>

#include "LauncherImportTypes.h"
#include "SafeFiles.h"

/* What the scanners share; not for use outside launcherimport/. */
namespace LauncherImport::ScanCommon
{
	inline QString unknownVersionProblem()
	{
		return QCoreApplication::translate(
			"LauncherImport", "Kite can't tell which Minecraft version it uses.");
	}
	inline QString missingFolderProblem()
	{
		return QCoreApplication::translate("LauncherImport",
										   "Its game folder is missing.");
	}

	/* Why Link is not offered for @p gameDir, or "" when it is: the folder
	 * must be a real one Kite may write to. */
	inline QString linkBlockerFor(const QString& gameDir)
	{
		const QFileInfo info(gameDir);
		if (!info.isDir()) {
			return missingFolderProblem();
		}
		if (isLink(info)) {
			return QCoreApplication::translate(
				"LauncherImport", "Its game folder is a link to another place.");
		}
		if (!info.isWritable()) {
			return QCoreApplication::translate(
				"LauncherImport", "Kite may not write to its game folder.");
		}
		return {};
	}

	/* A loader without a version cannot be installed; such an instance is
	 * listed but not offered. */
	inline void checkLoaders(FoundLauncher* launcher)
	{
		for (FoundInstance& instance : launcher->instances) {
			if (instance.importable() && !instance.loader.isEmpty() &&
				instance.loaderVersion.isEmpty()) {
				instance.problem = QCoreApplication::translate(
					"LauncherImport", "Kite can't tell which loader version it uses.");
			}
		}
	}

	/* @p dir's canonical path, so two routes to one folder compare equal. */
	inline QString canonical(const QString& dir)
	{
		const QString path = QFileInfo(dir).canonicalFilePath();
		return path.isEmpty() ? QFileInfo(dir).absoluteFilePath() : path;
	}

	inline QString instanceKey(Source source, const QString& where)
	{
		return sourceKey(source) + QLatin1Char(':') + where;
	}
} // namespace LauncherImport::ScanCommon
