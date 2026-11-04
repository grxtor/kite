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

#include <atomic>

/*
 * What a copy takes from another launcher's game folder, and the copy itself.
 *
 * Only the player's things come over, named in copiedFolders()/copiedFiles():
 * worlds, mods, configs, resource and shader packs, the options and the
 * server list. Everything else stays: logs, caches, the launcher's own files
 * and above all its accounts (the official launcher keeps its sign-in in the
 * same folder as the worlds).
 *
 * The source is only ever read. A link inside it is followed when it ends up
 * inside the game folder and skipped otherwise, so a copy cannot reach out to
 * the rest of the disk; a link back to a folder it is in is not walked again.
 */
namespace LauncherImport
{
	/* The top-level folders and files a copy takes. */
	QStringList copiedFolders();
	QStringList copiedFiles();

	struct CopyPlan {
		struct File {
			QString source;	  /* absolute, links resolved */
			QString relative; /* to the game folder, '/'-separated */
			qint64 size = 0;
		};
		QList<File> files;
		/* Every folder to make, parents first, so empty ones come too. */
		QStringList folders;
		qint64 bytes = 0;
		/* Links that pointed out of the game folder and were left out. */
		int skippedLinks = 0;
		bool cancelled = false;
		/* More entries than a copy lists; nothing should be copied. */
		bool tooLarge = false;
	};

	/* Lists what copying @p gameDir takes, less the top-level folders named
	 * in @p leaveOut (Duplicate without worlds leaves out "saves"). Stops early
	 * (cancelled) when @p cancel turns true. */
	CopyPlan planCopy(const QString& gameDir,
					  const std::atomic_bool* cancel = nullptr,
					  const QStringList& leaveOut = {});

	/* Copies @p plan into @p destination, adding each byte written to
	 * @p copied. False with @p error when a file cannot be read or written,
	 * or when @p cancel turns true (error empty). */
	bool runCopy(const CopyPlan& plan, const QString& destination,
				 const std::atomic_bool& cancel, std::atomic<qint64>* copied,
				 QString* error);
} // namespace LauncherImport
