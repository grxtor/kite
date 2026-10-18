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

#include "MessageLevel.h"

MessageLevel::Enum MessageLevel::getLevel(const QString& levelName)
{
	if (levelName == "Kite")
		return MessageLevel::Kite;
	else if (levelName == "Debug")
		return MessageLevel::Debug;
	else if (levelName == "Info")
		return MessageLevel::Info;
	else if (levelName == "Message")
		return MessageLevel::Message;
	else if (levelName == "Warning")
		return MessageLevel::Warning;
	else if (levelName == "Error")
		return MessageLevel::Error;
	else if (levelName == "Fatal")
		return MessageLevel::Fatal;
	// Skip PrePost, it's not exposed to !![]!
	// Also skip StdErr and StdOut
	else
		return MessageLevel::Unknown;
}

MessageLevel::Enum MessageLevel::fromLine(QString& line)
{
	// Level prefix
	int endmark = line.indexOf("]!");
	if (line.startsWith("!![") && endmark != -1) {
		auto level = MessageLevel::getLevel(line.left(endmark).mid(3));
		line = line.mid(endmark + 2);
		return level;
	}
	return MessageLevel::Unknown;
}
