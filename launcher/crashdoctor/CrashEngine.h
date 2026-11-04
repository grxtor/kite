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

#include <QHash>
#include <QList>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <optional>

#include "CrashRules.h"

/*
 * Matches a catalog against what one crashed run left behind. Pure: no files,
 * no settings, no network, so every corpus case is a unit test
 * (CrashEngine_test). The steps, in order, are in docs/crash-doctor.md.
 */
namespace CrashDoctor
{
	/// What the run left: the live log (Kite's lines, the JVM's and the
	/// game's), the newest crash report and hs_err file of the run.
	struct Evidence {
		QString log;
		QString crashReport;
		QString jvmError;
	};

	/// The instance and the computer, worked out by Kite (never by a rule).
	struct Facts {
		QString minecraft;
		/// "vanilla", "fabric", "quilt", "forge" or "neoforge".
		QString loader;
		/// "macos", "windows" or "linux".
		QString os;
		/// "arm64" or "x86_64".
		QString arch;
		int javaNow = 0;
		int javaRecommended = 0;
		int memoryMb = 0;
		int systemMemoryMb = 0;
		bool customJvmArgs = false;
		std::optional<int> exitCode;
		/// "normal" or "crash" (killed by a signal); empty when unknown.
		QString exitStatus;
		/// Ids of the enabled mods, provided ids included.
		QSet<QString> installed;
		/// Ids of the mods Kite Performance put in.
		QSet<QString> performanceMods;
		/// A shader mod (Iris, Oculus) is installed and its settings have a
		/// shader pack on: turning shaders off would change something.
		bool shadersOn = false;

		bool modded() const;
		/// Memory to offer after an out-of-memory crash.
		int suggestedMemoryMb() const;
		/// Memory to offer when the computer could not give what was asked.
		int lowerMemoryMb() const;
		/// Whether suggestedMemoryMb() is more than the game had.
		bool memoryRoom() const;
	};

	struct ResolvedFix {
		QString action;
		/// Filled-in strings and string lists, checked against the action's
		/// parameter types.
		QVariantMap params;
		QString label;
	};

	/// One match of a rule: its typed captures and the log line it came from.
	struct Finding {
		QVariantMap captures;
		QString line;
		Source source = Source::Log;
	};

	/// One cause, ready to show: a rule and every instance of it.
	struct Cause {
		QString ruleId;
		int score = 0;
		/// The rule is a fallback (Rule::fallback): Kite saw it, but can't say why.
		bool fallback = false;
		QList<Finding> findings;
		QString title;
		QString explanation;
		/// The first is the main button; the others go in the menu.
		QList<ResolvedFix> fixes;
	};

	/// The causes, best first. Empty when nothing fits: the caller says so
	/// honestly. @p language "tr..." picks Turkish, anything else English.
	QList<Cause> analyse(const Catalog& catalog, const Evidence& evidence,
						 const Facts& facts, const QString& language);

	/// CRLF to LF, no ANSI escapes, no Minecraft "§x" formatting codes.
	QString normalize(const QString& text);

	/// The first cause that is no fallback, or null: Kite could not say why.
	const Cause* firstExplained(const QList<Cause>& causes);

	inline constexpr int kLogTailLines = 400;
	inline constexpr int kMaxFindingsPerRule = 10;
	inline constexpr int kEvidenceSourceBonus = 3;
	inline constexpr int kMaxCountBonus = 3;
	inline constexpr int kMaxLineLength = 300;
} // namespace CrashDoctor
