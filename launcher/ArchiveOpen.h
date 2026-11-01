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

#include <QDir>
#include <QString>
#include <QtGlobal>

#include <archive.h>

#include <string>

/* libarchive's archive_*_open_filename() take a `char*`, which on Windows is
 * decoded in the active ANSI code page, not UTF-8, so a path with e.g. "Ş"
 * fails to open. Windows uses the `_w` overloads; elsewhere the narrow form is
 * right (on POSIX the `_w` entry points convert through the C locale and
 * refuse non-ASCII names). Paths longer than MAX_PATH are not handled (they
 * need a `\\?\` prefix). */
namespace MMCArchive
{
#ifdef Q_OS_WIN32
	/* The wide path libarchive's `_w` entry points want; returned by value and
	 * kept alive by the caller for the open call, which copies it. */
	inline std::wstring widePath(const QString& path)
	{
		return QDir::toNativeSeparators(path).toStdWString();
	}
#endif

	// archive_read_open_filename(), with the path encoded as the platform
	// reads it. Returns what libarchive returns.
	inline int openForReading(struct archive* handle, const QString& path,
							  size_t blockSize)
	{
#ifdef Q_OS_WIN32
		const std::wstring wide = widePath(path);
		return archive_read_open_filename_w(handle, wide.c_str(), blockSize);
#else
		return archive_read_open_filename(handle, path.toUtf8().constData(),
										  blockSize);
#endif
	}

	// archive_write_open_filename(), likewise.
	inline int openForWriting(struct archive* handle, const QString& path)
	{
#ifdef Q_OS_WIN32
		const std::wstring wide = widePath(path);
		return archive_write_open_filename_w(handle, wide.c_str());
#else
		return archive_write_open_filename(handle, path.toUtf8().constData());
#endif
	}
} // namespace MMCArchive
