/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include "Exception.h"
#include "pathmatcher/IPathMatcher.h"

#include <QDir>
#include <QFlags>

namespace FS
{

	class FileSystemException : public ::Exception
	{
	  public:
		FileSystemException(const QString& message) : Exception(message) {}
	};

	void write(const QString& filename, const QByteArray& data);

	QByteArray read(const QString& filename);

	bool updateTimestamp(const QString& filename);

	/** Creates all folders in the path; the last segment is treated as a file
	 * name and ignored. */
	bool ensureFilePathExists(QString filenamepath);

	/** Creates all folders in the path; the last segment is treated as a folder
	 * name and created. */
	bool ensureFolderPathExists(QString filenamepath);

	class copy
	{
	  public:
		copy(const QString& src, const QString& dst)
		{
			m_src = src;
			m_dst = dst;
		}
		copy& followSymlinks(const bool follow)
		{
			m_followSymlinks = follow;
			return *this;
		}
		copy& blacklist(const IPathMatcher* filter)
		{
			m_blacklist = filter;
			return *this;
		}
		bool operator()()
		{
			return operator()(QString());
		}

	  private:
		bool operator()(const QString& offset);

	  private:
		bool m_followSymlinks = true;
		const IPathMatcher* m_blacklist = nullptr;
		QDir m_src;
		QDir m_dst;
	};

	bool deletePath(QString path);

	/** Move everything in @p source on top of @p destination, replacing files
	 * that exist in both and leaving files only in @p destination alone: how a
	 * modpack update lands, keeping saves, screenshots, logs and config edits.
	 * Files the new version dropped are removed by the caller, the only party
	 * that can tell them from the user's. @p source is removed on success.
	 * Returns false on the first failure, having done part of the work; a
	 * partial merge cannot be rolled back. */
	bool overrideFolder(const QString& destination, const QString& source);

	/** Whether this platform has a trash we can trust to be reversible, so a
	 * dialog can promise "recoverable" or "permanent" and be right. */
	bool canTrash();

	/** Move a file or folder to the platform's trash. On success @p
	 * pathInTrash, when given, receives where the item landed (what makes undo
	 * possible). Returns false without touching anything when there is no
	 * reversible trash; the caller may fall back to deletePath() but must say
	 * so, since that cannot be undone. */
	bool trash(const QString& path, QString* pathInTrash = nullptr);

	QString PathCombine(const QString& path1, const QString& path2);
	QString PathCombine(const QString& path1, const QString& path2,
						const QString& path3);
	QString PathCombine(const QString& path1, const QString& path2,
						const QString& path3, const QString& path4);

	QString AbsolutePath(QString path);

	/** Resolve an executable: a single name, a relative path or an absolute
	 * path. @return absolute path, or a null string. */
	QString ResolveExecutable(QString path);

	/** Normalize path: paths inside the current directory become relative to
	 * it, other paths are made absolute. */
	QString NormalizePath(QString path);

	QString RemoveInvalidFilenameChars(QString string, QChar replaceWith = '-');

	QString DirNameFromString(QString string, QString inDir = ".");

	/// Checks if the a given Path contains "!"
	bool checkProblemticPathJava(QDir folder);

	QString getDesktopDir();

	QString getApplicationsDir();

	/** Write a shortcut that runs @p target with @p args, labelled @p name,
	 * with the image at @p iconPath. @p destination is the path without a
	 * suffix: each platform appends its own (.lnk on Windows, .desktop on
	 * Linux and the BSDs, a small .app bundle on macOS, the only form Finder
	 * shows with an icon and arguments). Returns the path written, or an empty
	 * string. */
	QString createShortcut(QString destination, QString target,
						   QStringList args, QString name, QString iconPath);
} // namespace FS
