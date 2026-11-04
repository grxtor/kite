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

#include <QDateTime>
#include <QList>
#include <QString>

// Where a game run leaves its evidence, found by time so files from earlier
// runs are not blamed on this one. Both answer the newest file whose last change
// is not before @p since, or "" if there is none.
namespace CrashFiles
{
	/// <gameRoot>/crash-reports/crash-*.txt. Forge and NeoForge write one
	/// for a mod loading error and then let the game exit with code 0.
	QString newestCrashReport(const QString& gameRoot, const QDateTime& since);
	/// hs_err_pid<pid>.log, the JVM's own fatal error report. The JVM writes it
	/// to its working directory, the game root, or to the temporary folder when
	/// it cannot. The report of one of @p pids first (the JVM that crashed,
	/// named in the log), in either folder; else the newest in the game root.
	QString newestJvmErrorReport(const QString& gameRoot, const QDateTime& since,
								 const QList<qint64>& pids = {});
} // namespace CrashFiles
