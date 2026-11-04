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
#include "CrashScrubber.h"
#include "CrashSignature.h"

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

	/// An enabled mod as its metadata names it, for a crash report.
	struct ModEntry {
		QString id;
		QString version;
	};

	/// Files to read instead of looking for the newest ones (a crash read
	/// again later, CrashFollowUp).
	struct RunFiles {
		QString crashReport;
		QString jvmError;
		QString log;

		bool isEmpty() const
		{
			return crashReport.isEmpty() && jvmError.isEmpty() && log.isEmpty();
		}
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
		/// What the crash report's lines must not carry.
		Personal personal;
		QString loaderVersion;
		/// Set: read these and nothing else (no live log).
		RunFiles files;
		/// The rule of the last fix tried on this instance, and when.
		QString lastFixRule;
		QDateTime lastFixAt;
		/// NativeCrashStreak::retried() of the instance before this run.
		int retriedNativeCrashes = 0;
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

		// For a crash report (CrashPayload) and reading it again later.
		Facts facts;
		QString loaderVersion;
		/// The enabled mods by their own id, sorted.
		QList<ModEntry> mods;
		/// The files read; log is logs/latest.log, also when the live log was.
		RunFiles files;
		/// When the launch began; invalid for a crash read from its files.
		QDateTime launchedAt;
		ReportLines lines;
		/// signatureOf(lines.lines), "" without lines.
		QString signature;
		/// The fix Kite offered last time for this crash was tried and the game
		/// crashed the same way again (CrashDoctorService).
		bool fixDidNotHelp = false;
		/// Found by rules that came after the crash (CrashFollowUp): the time
		/// of the crash, invalid otherwise.
		QDateTime solvedLaterFor;
		/// This crash is a native one and the instance crashed like this this
		/// many times lately before a later try started (0: not so).
		int retriedNativeCrashes = 0;
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
	/// The processes whose hs_err file would be this run's, each once: the JVM
	/// that says it crashed ("pid=1234, tid=…", "hs_err_pid1234.log"), then the
	/// one Kite started ("Minecraft process ID: 1234"), which is the game itself
	/// only when it runs in Kite's own launcher JVM.
	QList<qint64> jvmPidsFromLog(const QString& log);
	/// Whether Iris or Oculus is installed (@p installed) and its settings in
	/// <gameRoot>/config name a shader pack without turning shaders off.
	bool shaderPackOn(const QString& gameRoot, const QSet<QString>& installed);

	inline constexpr qsizetype kMaxLogBytes = 2 * 1024 * 1024;
	inline constexpr qsizetype kMaxCrashReportBytes = 1024 * 1024;
	inline constexpr qsizetype kMaxJvmErrorBytes = 256 * 1024;
} // namespace CrashDoctor
