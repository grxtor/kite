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
#include <QString>

/*
 * How the native crashes (a Windows status as the exit code, CrashExitCodes)
 * of one instance went lately, kept in its CrashDoctorNativeCrashes setting:
 * how many came in a row now, and how many came before the last launch that
 * worked. "It crashes, and starts on the third try" points at a graphics
 * driver or an overlay failing at start rather than at a mod, and the crash
 * window says so. Values, no settings: CrashDoctorService reads and writes it.
 */
namespace CrashDoctor
{
	struct NativeCrashStreak {
		/// Native crashes since the last launch that worked.
		int crashes = 0;
		/// The native crashes in a row that the last launch that worked ended,
		/// and when that launch ended.
		int crashesBeforeStart = 0;
		QDateTime startedAt;

		/// After a crash: one more in a row when it was @p native, else the
		/// row is broken.
		NativeCrashStreak afterCrash(bool native) const;
		/// After a launch that worked (the game ran and closed without a
		/// crash), ending at @p now.
		NativeCrashStreak afterCleanRun(const QDateTime& now) const;
		/// crashesBeforeStart while it is recent and several; else 0.
		int retried(const QDateTime& now) const;

		QString toJson() const;
		/// Empty or unreadable text is no crash.
		static NativeCrashStreak fromJson(const QString& json);
	};

	/// The instance setting it is kept in.
	inline constexpr char kNativeCrashesSetting[] = "CrashDoctorNativeCrashes";
	/// Fewer crashes before a start are bad luck, not a pattern.
	inline constexpr int kMinRetriedCrashes = 2;
	inline constexpr int kRetriedCrashesDays = 30;
} // namespace CrashDoctor
