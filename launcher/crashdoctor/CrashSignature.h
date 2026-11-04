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
#include <QSet>
#include <QString>
#include <QStringList>

#include "CrashEngine.h"
#include "CrashScrubber.h"

/*
 * The lines a crash report sends and the signature that groups them: the
 * exception and message, the root cause and its top frames, read from the crash
 * report, else the hs_err file, else the end of the log, then scrubbed
 * (CrashScrubber). The same crash on another computer gives the same lines, so
 * the same signature; the server computes it again from the lines. Pure; the
 * algorithm is written down in docs/crash-doctor.md ("Crash reports").
 */
namespace CrashDoctor
{
	struct ReportLines {
		QStringList lines;
		/// The mod the crash points at, when one of the installed ones does:
		/// "Suspected Mods" of the crash report, a frame's Forge module, or a
		/// frame's class in the catalog's class hints. "" otherwise.
		QString culprit;
	};

	/// Nothing (no lines) when the run left no error to send.
	ReportLines reportLines(const Evidence& evidence, const Personal& personal,
							const QList<ClassHint>& hints, const QSet<QString>& installed);

	/// SHA-256 of the UTF-8 lines joined by "\n", in lowercase hex.
	QString signatureOf(const QStringList& lines);

	inline constexpr int kSignatureFormat = 1;
	inline constexpr int kMaxReportFrames = 8;
	/// Message lines kept after an exception's first line.
	inline constexpr int kMaxMessageLines = 2;
	/// Error lines of the log kept when it has no stack trace.
	inline constexpr int kMaxFallbackLines = 5;
	/// Mods of a Forge or NeoForge mod loading failure whose messages are kept.
	inline constexpr int kMaxModSections = 3;
} // namespace CrashDoctor
