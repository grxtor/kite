/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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
#include <QtGlobal>

namespace Strings
{
	int naturalCompare(const QString& s1, const QString& s2,
					   Qt::CaseSensitivity cs);
	QString htmlListPatch(const QString& html);
	const char* logColor(QtMsgType type);
	const char* logColorReset();
	/// Dimmed: for the scaffolding of a log line (timestamp, source location).
	const char* logColorFaint();
	/// Emphasised: for the level letter and the category tag.
	const char* logColorBold();
} // namespace Strings
