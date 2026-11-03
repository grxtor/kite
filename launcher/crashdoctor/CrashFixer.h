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

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QVariantMap>
#include <optional>

#include "CrashCollector.h"
#include "CrashEngine.h"

class SettingsObject;

/*
 * Carries out the crash doctor's Fixer actions (CrashActions::Runner::Fixer):
 * instance settings the next launch reads, and files of the instance renamed
 * aside (never deleted). Each returns what undoes it; the last one is kept in
 * the instance's CrashDoctorUndo setting so Undo works after a restart too.
 */
namespace CrashDoctor
{
	struct Undo {
		QString action;
		/// Instance setting -> its value before the fix.
		QVariantMap settings;
		/// Launcher setting -> its value before the fix.
		QVariantMap launcherSettings;
		/// Renames to reverse: (where the file is now, where it was).
		QList<QPair<QString, QString>> renames;
		/// File -> its content before the fix; a null array: it did not exist.
		QHash<QString, QByteArray> contents;
		/// The game's Graphics API setting before the fix, if it was changed.
		QString graphicsBackend;

		QString toJson() const;
		static std::optional<Undo> fromJson(const QString& json);
	};

	/// The files @p fix would change, as a person reads them (file names), for
	/// the confirmation; empty when it changes no file.
	QStringList filesTouched(const ResolvedFix& fix, const Report& report);

	/// The instance a fix changes: its settings and folders, and the
	/// launcher's own settings (the Windows GPU choice; null leaves them alone).
	struct Target {
		SettingsObject& settings;
		QString gameRoot;
		QString modsDir;
		SettingsObject* launcherSettings = nullptr;
	};

	/// Applies @p fix. Nothing, with @p error, when it cannot: unknown action,
	/// a file that is gone or outside the instance.
	std::optional<Undo> applyFix(const Target& target, const ResolvedFix& fix,
								 const Report& report, QString* error);
	/// Puts back what @p undo recorded; false (with @p error) if a file moved
	/// on in the meantime, the rest is still put back.
	bool undoFix(const Target& target, const Undo& undo, QString* error);

	/// What a backup copy of @p path is called: <path>.kite-backup-<time>.
	QString backupPath(const QString& path, const QDateTime& when);

	inline constexpr int kMinMemoryMb = 1024;
	inline constexpr int kKeepForSystemMb = 2048;
} // namespace CrashDoctor
