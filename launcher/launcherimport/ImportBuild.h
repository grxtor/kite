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

#include <QString>

#include <atomic>

#include "LauncherImportTypes.h"

/*
 * Turning one found instance into a Kite instance folder. Runs off the main
 * thread and touches nothing but @p stagingPath, which the instance list then
 * moves into place: the source is read (Copy) or only named (Link), never
 * written.
 */
namespace LauncherImport
{
	/* The instance.cfg keys an import writes. */
	inline const QString kLinkedGameDirKey = QStringLiteral("LinkedGameDir");
	inline const QString kImportedFromKey = QStringLiteral("ImportedFrom");

	struct BuildRequest {
		FoundInstance instance;
		Mode mode = Mode::Copy;
		/* The new instance's name and icon. */
		QString name;
		QString iconKey;
	};

	struct BuildResult {
		bool ok = false;
		bool cancelled = false;
		QString error;
		qint64 copiedBytes = 0;
		int skippedLinks = 0;
		/* Mods whose platform and project id were written for Kite. */
		int mappedMods = 0;
	};

	/* How far a build is, read from another thread for a progress bar:
	 * @p total is -1 until the copy is planned (and stays 0 for Link). */
	struct BuildProgress {
		std::atomic<qint64> total{-1};
		std::atomic<qint64> done{0};
	};

	/* Builds the instance in @p stagingPath: instance.cfg, mmc-pack.json and,
	 * for Copy, the game files under minecraft/ with their mods' origins. */
	BuildResult buildInstance(const BuildRequest& request,
							  const QString& stagingPath,
							  const std::atomic_bool& cancel,
							  BuildProgress* progress = nullptr);
} // namespace LauncherImport
