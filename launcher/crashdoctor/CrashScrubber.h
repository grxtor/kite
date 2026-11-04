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

#include <QRegularExpression>
#include <QString>
#include <QStringList>

/*
 * Takes what is personal or belongs to one run out of the error lines a crash
 * report sends (CrashSignature), so the same crash on two computers reads the
 * same and nothing in it points at a person: home folders and other paths,
 * user and player names, UUIDs, addresses, hex numbers, times, and the
 * numbers of generated classes and lambdas. Pure; the steps are in
 * docs/crash-doctor.md ("Crash reports").
 */
namespace CrashDoctor
{
	/// What only this computer knows: the home folder and the names of the
	/// system user and the Minecraft accounts.
	class Personal
	{
	  public:
		Personal() = default;
		Personal(const QStringList& homes, const QStringList& names);

		const QStringList& homes() const
		{
			return m_homes;
		}
		/// Whole words, any case; invalid when there is no name to look for.
		const QRegularExpression& names() const
		{
			return m_names;
		}

	  private:
		QStringList m_homes;
		QRegularExpression m_names;
	};

	/// An exception message, a log line or an hs_err line, scrubbed: numbers
	/// become "<n>", host names "<host>", and the rest as scrubFrame().
	QString scrubMessage(const QString& line, const Personal& personal);
	/// A stack frame ("\tat TRANSFORMER/create@6.0/a.B.c(B.java:12) ~[x.jar:?]")
	/// as "at a.B.c(B.java)": no module prefix, line number or jar, and
	/// generated names without their numbers. @p module gets the mod the
	/// Forge module prefix names ("create"), or "".
	QString scrubFrame(const QString& frame, const Personal& personal,
					   QString* module = nullptr);

	/// Shorter names than this are not looked for: they would hit words.
	inline constexpr int kMinPersonalLength = 3;
	/// The longest line a report carries, in UTF-16 code units.
	inline constexpr int kMaxReportLineLength = 300;
} // namespace CrashDoctor
