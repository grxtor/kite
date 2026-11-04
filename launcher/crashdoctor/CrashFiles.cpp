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

#include "CrashFiles.h"

#include <QDir>
#include <QFileInfo>

namespace
{
	const QString kCrashReportsFolder = QStringLiteral("crash-reports");
	const QString kCrashReportPattern = QStringLiteral("crash-*.txt");
	const QString kJvmErrorPattern = QStringLiteral("hs_err_pid*.log");
	// File systems keep coarser times than the clock (FAT: 2 s), so a file
	// written right after the start can look older than it.
	constexpr int kClockSlackSecs = 2;

	QString newestSince(const QDir& dir, const QString& pattern,
						const QDateTime& since)
	{
		if (!dir.exists()) {
			return QString();
		}
		const QFileInfoList files =
			dir.entryInfoList({pattern}, QDir::Files, QDir::Time);
		if (files.isEmpty()) {
			return QString();
		}
		// QDir::Time sorts the newest first.
		const QFileInfo& newest = files.first();
		return newest.lastModified() >= since.addSecs(-kClockSlackSecs)
				   ? newest.absoluteFilePath()
				   : QString();
	}
} // namespace

namespace CrashFiles
{
	QString newestCrashReport(const QString& gameRoot, const QDateTime& since)
	{
		return newestSince(QDir(gameRoot).filePath(kCrashReportsFolder),
						   kCrashReportPattern, since);
	}

	QString newestJvmErrorReport(const QString& gameRoot,
								 const QDateTime& since)
	{
		return newestSince(QDir(gameRoot), kJvmErrorPattern, since);
	}
} // namespace CrashFiles
