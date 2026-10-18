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

/*!
 * Opening an archive by file name, on a path that is not necessarily ASCII.
 *
 * archive_read_open_filename() and archive_write_open_filename() take a
 * `char*`, and on Windows libarchive hands that to the narrow CRT/Win32
 * entry points, which decode it in the process's active ANSI code page --
 * not UTF-8. So `path.toUtf8()` is the one encoding those functions are
 * guaranteed to get wrong there: a perfectly ordinary
 *
 *     C:\Users\Şafak\AppData\Roaming\Kite\instances\...
 *
 * arrives mangled and the open fails with "No such file or directory",
 * for no reason other than the letter in the user's name. Every archive
 * Kite touches lives under the data directory, which lives under the
 * user profile, so this is not an edge case for the people it hits -- it
 * is every zip they ever open: instance import and export, modded jar
 * creation, and the update they are being offered.
 *
 * libarchive's own answer is the `_w` overloads, which go through
 * _wfopen()/CreateFileW(). Windows gets those; everywhere else the byte
 * path is already UTF-8 (or whatever QFile::encodeName would produce for
 * it) and the narrow form is correct as it stands.
 *
 * The Q_OS_WIN32 guard is load-bearing, not tidiness. On POSIX libarchive
 * implements the `_w` entry points by converting the wide name back to a
 * multi-byte string through the C locale, which is not the same thing as
 * UTF-8 and is not even set up in most of the environments this runs in:
 * measured on Linux, archive_write_open_filename_w() refuses a non-ASCII
 * name outright with "Can't convert '/tmp/?afak-?.zip' to MBS" while the
 * narrow call with the same path in UTF-8 bytes succeeds. Reaching for the
 * wide form on both platforms would have traded a Windows bug for a Linux
 * one.
 *
 * Both open-by-name directions are wrapped, not just the read side: an
 * export that cannot create its output is the same bug seen from the
 * other end.
 *
 * Still not handled: paths longer than MAX_PATH, which need a `\\?\`
 * prefix that libarchive does not add for us.
 */
namespace MMCArchive
{
#ifdef Q_OS_WIN32
	/*!
	 * The wide path libarchive's `_w` entry points want.
	 *
	 * Returned by value and kept alive by the caller for the duration of
	 * the open call -- these functions copy the name they are given.
	 */
	inline std::wstring widePath(const QString& path)
	{
		return QDir::toNativeSeparators(path).toStdWString();
	}
#endif

	//! archive_read_open_filename(), with the path encoded as the platform
	//! actually reads it. Returns what libarchive returns.
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

	//! archive_write_open_filename(), likewise.
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
