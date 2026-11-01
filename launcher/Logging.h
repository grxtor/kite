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

/* Logging categories for the launcher's own output. A category tags the line
 * and gives users a filter, e.g. QT_LOGGING_RULES="kite.net.debug=false". To
 * add one: declare it here, define it in Logging.cpp as "kite.<subsystem>",
 * and use qCDebug(<name>). Only declare categories in use. */

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
	/** The log line layout as a Qt message pattern for qSetMessagePattern().
	 * @param coloured wrap the level letter in ANSI colour (console copy, not
	 * the file). @param withSourceLocation append "(function:line)"; per
	 * message, since Qt's own messages have no context and would end in
	 * "(unknown:0)". */
	QString messagePattern(bool coloured, bool withSourceLocation);

	/** Makes the console able to render ANSI colour; reports whether it now
	 * can. Call once, early, after the console is attached and the standard
	 * streams reopened (the answer depends on what stderr points at). A
	 * Windows console does not interpret escapes until
	 * ENABLE_VIRTUAL_TERMINAL_PROCESSING is set. */
	bool prepareConsoleColour();

	/// Whether the console copy of a log line may carry ANSI colour; false
	/// until prepareConsoleColour() says otherwise.
	bool consoleColourEnabled();
} // namespace Logging
