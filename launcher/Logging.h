/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
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

#include <QLoggingCategory>
#include <QString>

/*
 * Logging categories for the launcher's own output.
 *
 * A category does two things for us: it tags the line in the log, so a reader
 * can tell our noise apart from Qt's, and it gives users a filter they can
 * aim, for example
 *
 *     QT_LOGGING_RULES="kite.net.debug=false"
 *
 * to silence one subsystem without losing its warnings.
 *
 * To add one: declare it here, define it in Logging.cpp with the matching
 * "kite.<subsystem>" string, then use qCDebug(<name>) instead of qDebug()
 * in that subsystem. Uncategorised qDebug() keeps working and keeps printing
 * exactly as it did - Qt reports its category as "default", which the log
 * layout treats as "no category" and omits.
 *
 * NOTE: only declare categories that are actually in use. An unused category
 * is a promise about where messages come from that nothing keeps.
 */

/// Downloads, HTTP caching, network jobs. And more.
Q_DECLARE_LOGGING_CATEGORY(netLog)
Q_DECLARE_LOGGING_CATEGORY(mojangLog)
Q_DECLARE_LOGGING_CATEGORY(metaLog)
Q_DECLARE_LOGGING_CATEGORY(archiveLog)
Q_DECLARE_LOGGING_CATEGORY(backupLog)
Q_DECLARE_LOGGING_CATEGORY(iconsLog)
Q_DECLARE_LOGGING_CATEGORY(javaLog)
Q_DECLARE_LOGGING_CATEGORY(launchLog)
Q_DECLARE_LOGGING_CATEGORY(newsLog)
Q_DECLARE_LOGGING_CATEGORY(pluginsLog)
Q_DECLARE_LOGGING_CATEGORY(minecraftauthLog)
Q_DECLARE_LOGGING_CATEGORY(minecraftgoptionsLog)
Q_DECLARE_LOGGING_CATEGORY(minecraftlaunchLog)

namespace Logging
{
	/**
	 * @brief the launcher's log line layout, as a Qt message pattern
	 *
	 * Handed to qSetMessagePattern() so that Qt's own formatter does the
	 * work; that is what makes %{category} and a readable %{function}
	 * available at all.
	 *
	 * @param coloured whether to wrap the level letter in ANSI colour, which
	 *                 is wanted for the console copy but not for the log file
	 * @param withSourceLocation whether to append "(function:line)"
	 *
	 * withSourceLocation is a per-message decision, not a per-build one: Qt's
	 * own libraries are built without QT_MESSAGELOGCONTEXT, so anything
	 * raised inside Qt arrives with an empty context and must be rendered
	 * without the suffix, or every such line ends in a useless "(unknown:0)".
	 * The caller decides by looking at QMessageLogContext::line.
	 */
	QString messagePattern(bool coloured, bool withSourceLocation);

	/**
	 * @brief make the console able to render ANSI colour, and report whether
	 *        it now can
	 *
	 * Call once, early, and after the console has been attached and the
	 * standard streams reopened - the answer depends on what stderr actually
	 * points at.
	 *
	 * This is not a formality on Windows: a console does not interpret escape
	 * sequences until someone sets ENABLE_VIRTUAL_TERMINAL_PROCESSING on it,
	 * and the classic console host starts with that bit clear. Printing
	 * colour anyway is how a log line ends up reading "<ESC>[32mD:<ESC>[0m".
	 */
	bool prepareConsoleColour();

	/// Whether the console copy of a log line may carry ANSI colour. False
	/// until prepareConsoleColour() has said otherwise.
	bool consoleColourEnabled();
} // namespace Logging
