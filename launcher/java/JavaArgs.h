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

namespace JavaArgs
{
	/* Returns the warning text if @p jvmargs sets a memory option through
	 * flags meant to be set via the Memory group in Settings instead
	 * ("-Xmx", "-XX:PermSize=", ...), or an empty string if the args are
	 * fine. Pure logic, no user interface: the widget JavaCommon, the
	 * launch step and the QML Java settings all ask the same question. */
	QString memoryFlagWarning(const QString& jvmargs);

	/* The test behind memoryFlagWarning(): whether @p jvmargs sets heap or
	 * PermGen size by hand. */
	bool setsMemoryFlags(const QString& jvmargs);
} // namespace JavaArgs
