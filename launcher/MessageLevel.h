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

#include <QString>

/**
 * @brief the MessageLevel Enum
 * defines what level a log message is
 */
namespace MessageLevel
{
	enum Enum {
		Unknown, /**< No idea what this is or where it came from */
		StdOut,	 /**< Undetermined stderr messages */
		StdErr,	 /**< Undetermined stdout messages */
		Kite,	 /**< Kite Messages */
		Debug,	 /**< Debug Messages */
		Info,	 /**< Info Messages */
		Message, /**< Standard Messages */
		Warning, /**< Warnings */
		Error,	 /**< Errors */
		Fatal,	 /**< Fatal Errors */
	};
	MessageLevel::Enum getLevel(const QString& levelName);

	/* Get message level from a line. Line is modified if it was successful. */
	MessageLevel::Enum fromLine(QString& line);
} // namespace MessageLevel
