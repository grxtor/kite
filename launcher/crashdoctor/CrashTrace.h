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

/*
 * Reads the Java stack traces in a crash report or a log: each exception with
 * its message lines and frames, and its "Caused by:" causes ("Suppressed:"
 * blocks are left out). Pure; CrashSignature picks the lines it sends.
 */
namespace CrashDoctor::Trace
{
	/// One throwable: its first line, the message lines before its frames
	/// (at most 8 are read), and its frames (at most 32).
	struct Throwable {
		QString header;
		QStringList message;
		QStringList frames;
	};
	/// A trace: the exception, then each "Caused by:".
	using Chain = QList<Throwable>;

	/// The traces in @p text that have frames, in order.
	QList<Chain> read(const QString& text);
} // namespace CrashDoctor::Trace
