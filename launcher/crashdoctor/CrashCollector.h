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

#include <QDateTime>
#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>

#include "CrashEngine.h"

class LaunchTask;
class MinecraftInstance;

/*
 * Gathers what the crash doctor reads after a run. snapshot() runs on the GUI
 * thread and only reads settings and the live log; analyse() runs on a worker
 * thread and reads files: the crash report and hs_err of the run, the tail of
 * logs/latest.log when there is no live log, and the mods folder's metadata.
 */
namespace CrashDoctor
{
	/// One enabled file of the mods folder.
	struct ModFile {
		QString path;
		QString version;
	};

	struct Snapshot {
		QString instanceId;
		QString gameRoot;
		QString modsDir;
		/// Files of this run are not older than this; invalid: no launch to
		/// go by (an analysis asked for later), latest.log's age decides.
		QDateTime since;
		QString liveLog;
		Facts facts;
		/// File names (ModMetadataIndex keys) Kite Performance installed.
		QSet<QString> performanceFiles;
	};

	/// The outcome of one analysis, kept until the next launch.
	struct Report {
		QString instanceId;
		QString gameRoot;
		QString modsDir;
		QDateTime analysedAt;
		QList<Cause> causes;
		QString crashReportPath;
		QString jvmErrorPath;
		std::optional<int> exitCode;
		/// Mod id (and provided ids) -> its enabled files, for the fixes.
		QHash<QString, QList<ModFile>> modFiles;
		/// Mod id -> the name its metadata gives, for the words on screen.
		QHash<QString, QString> modNames;
	};

	/// Reads @p instance's settings and @p task's log (null: none).
	Snapshot snapshot(MinecraftInstance& instance, LaunchTask* task);
	/// Worker-thread safe: files and the engine, nothing shared.
	Report analyse(const Catalog& catalog, const Snapshot& snapshot,
				   const QString& language);

	/// The exit Kite logged for the game ("Process exited with code 1.",
	/// "Process crashed with exitcode 9."): code and "normal" or "crash".
	std::optional<QPair<int, QString>> exitFromLog(const QString& log);
	/// The Java major CheckJava logged ("Java is version 17.0.15, ..."), or 0.
	int javaFromLog(const QString& log);

	inline constexpr qsizetype kMaxLogBytes = 2 * 1024 * 1024;
	inline constexpr qsizetype kMaxCrashReportBytes = 1024 * 1024;
	inline constexpr qsizetype kMaxJvmErrorBytes = 256 * 1024;
} // namespace CrashDoctor
