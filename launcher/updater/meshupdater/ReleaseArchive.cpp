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

#include "ReleaseArchive.h"

#include "ArchiveOpen.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cerrno>
#include <cstring>
#include <memory>

#if !defined(Q_OS_WIN32)
#include <unistd.h>
#endif

namespace
{

	//! 64 KiB at a time: large enough to be cheap, small enough to not matter.
	constexpr qint64 kCopyBlockSize = 64 * 1024;

	using ArchiveReadPtr =
		std::unique_ptr<archive, decltype(&archive_read_free)>;

	QString archiveError(archive* handle, const QString& fallback)
	{
		if (const char* message = archive_error_string(handle);
			message && *message)
			return QString::fromUtf8(message);
		return fallback;
	}

	/*!
	 * Resolve an archive entry's path against \a destDir, refusing anything
	 * that escapes it.
	 *
	 * QDir::cleanPath collapses "a/../../b" for us; comparing the cleaned
	 * absolute result against the destination prefix is then enough to catch
	 * both "../" traversal and absolute paths. Returns an empty string when
	 * the entry must be rejected.
	 */
	QString safeDestination(const QDir& destDir, const QString& entryPath)
	{
		if (entryPath.isEmpty())
			return {};

		// Windows-style separators appear in archives built on Windows, and a
		// backslash is a perfectly ordinary character in a POSIX file name --
		// so normalise before any of the checks below, not after.
		QString normalised = entryPath;
		normalised.replace(QLatin1Char('\\'), QLatin1Char('/'));

		if (normalised.startsWith(QLatin1Char('/')) ||
			normalised.contains(QLatin1Char(':'))) {
			// Absolute POSIX path, or a Windows drive letter.
			return {};
		}

		const QString base = QDir::cleanPath(destDir.absolutePath());
		const QString candidate =
			QDir::cleanPath(base + QLatin1Char('/') + normalised);

		if (candidate == base)
			return {};
		if (!candidate.startsWith(base + QLatin1Char('/')))
			return {};

		return candidate;
	}

	//! Whichever of the two spellings libarchive filled in.
	const char* entryString(const char* utf8Value, const char* fallbackValue)
	{
		if (utf8Value && *utf8Value)
			return utf8Value;
		return fallbackValue;
	}

	/*!
	 * Where a symlink would land, refusing anything outside \a destDir.
	 *
	 * A symlink's target is relative to the directory the link sits in, not
	 * to the archive root, so it is resolved from there. Nothing is written
	 * through the link during extraction, but the install pass that follows
	 * copies files -- and a link pointing out of the tree would send those
	 * writes anywhere on the disk.
	 */
	bool symlinkTargetStaysInside(const QDir& destDir, const QString& linkPath,
								  const QString& linkTarget)
	{
		if (linkTarget.isEmpty())
			return false;
		if (linkTarget.startsWith(QLatin1Char('/')))
			return false; // absolute, so not ours to point at

		const QString base = QDir::cleanPath(destDir.absolutePath());
		const QString candidate = QDir::cleanPath(
			QFileInfo(linkPath).absolutePath() + QLatin1Char('/') + linkTarget);

		return candidate == base ||
			   candidate.startsWith(base + QLatin1Char('/'));
	}

	bool createLink(const QString& linkPath, const QString& target,
					bool hard, QString* error)
	{
#if defined(Q_OS_WIN32)
		// Our Windows artifacts are zips built from a directory tree, so they
		// contain neither kind. Refusing beats emulating: QFile::link() would
		// write a .lnk shortcut, which is not a link as far as anything that
		// opens the file is concerned.
		Q_UNUSED(linkPath)
		Q_UNUSED(target)
		Q_UNUSED(hard)
		*error = QStringLiteral("archive links are not supported on Windows");
		return false;
#else
		// A leftover from an earlier attempt would make the call fail with
		// EEXIST; the caller has already decided this tree is ours to write.
		QFile::remove(linkPath);

		const int result = hard
							   ? ::link(QFile::encodeName(target).constData(),
										QFile::encodeName(linkPath).constData())
							   : ::symlink(QFile::encodeName(target).constData(),
										   QFile::encodeName(linkPath).constData());
		if (result != 0) {
			*error = QStringLiteral("could not create %1 -> %2: %3")
						 .arg(QDir::toNativeSeparators(linkPath), target,
							  QString::fromLocal8Bit(strerror(errno)));
			return false;
		}
		return true;
#endif
	}

	bool writeEntry(archive* reader, const QString& path, qint64* bytesWritten,
					QString* error)
	{
		QFile out(path);
		if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			*error = QStringLiteral("could not write %1: %2")
						 .arg(QDir::toNativeSeparators(path),
							  out.errorString());
			return false;
		}

		for (;;) {
			const void* block = nullptr;
			size_t size = 0;
			la_int64_t offset = 0;

			const int status =
				archive_read_data_block(reader, &block, &size, &offset);
			if (status == ARCHIVE_EOF)
				break;
			if (status < ARCHIVE_WARN) {
				*error = QStringLiteral("could not read %1 from the archive: %2")
							 .arg(QDir::toNativeSeparators(path),
								  archiveError(reader, QStringLiteral("unknown error")));
				return false;
			}

			// A sparse entry reports gaps as jumps in the offset; seek so the
			// hole is preserved rather than shifting the rest of the file.
			if (!out.seek(offset)) {
				*error = QStringLiteral("could not seek in %1: %2")
							 .arg(QDir::toNativeSeparators(path),
								  out.errorString());
				return false;
			}

			const char* cursor = static_cast<const char*>(block);
			qint64 remaining = static_cast<qint64>(size);
			while (remaining > 0) {
				const qint64 chunk = qMin(remaining, kCopyBlockSize);
				const qint64 written = out.write(cursor, chunk);
				if (written < 0) {
					*error = QStringLiteral("could not write %1: %2")
								 .arg(QDir::toNativeSeparators(path),
									  out.errorString());
					return false;
				}
				cursor += written;
				remaining -= written;
				*bytesWritten += written;
			}
		}

		if (!out.flush()) {
			*error = QStringLiteral("could not flush %1: %2")
						 .arg(QDir::toNativeSeparators(path), out.errorString());
			return false;
		}
		out.close();
		return true;
	}

} // namespace

ReleaseArchive::Result ReleaseArchive::extract(const QString& archivePath,
											   const QString& destDir)
{
	Result result;

	if (!QFileInfo::exists(archivePath)) {
		result.error = QStringLiteral("%1 does not exist")
						   .arg(QDir::toNativeSeparators(archivePath));
		return result;
	}

	QDir destination(destDir);
	if (!destination.exists() && !QDir().mkpath(destination.absolutePath())) {
		result.error = QStringLiteral("could not create %1")
						   .arg(QDir::toNativeSeparators(destDir));
		return result;
	}

	ArchiveReadPtr reader(archive_read_new(), &archive_read_free);
	if (!reader) {
		result.error = QStringLiteral("could not allocate an archive reader");
		return result;
	}

	// Let libarchive work out what the container is. Release artifacts are
	// .zip on Windows and macOS and .tar.gz on Linux, and having one code
	// path for both means neither can rot unnoticed.
	archive_read_support_filter_all(reader.get());
	archive_read_support_format_all(reader.get());

	if (MMCArchive::openForReading(reader.get(), archivePath,
								   kCopyBlockSize) != ARCHIVE_OK) {
		result.error =
			QStringLiteral("could not open %1: %2")
				.arg(QDir::toNativeSeparators(archivePath),
					 archiveError(reader.get(),
								  QStringLiteral("unrecognised archive")));
		return result;
	}

	for (;;) {
		archive_entry* entry = nullptr;
		const int status = archive_read_next_header(reader.get(), &entry);
		if (status == ARCHIVE_EOF)
			break;
		if (status < ARCHIVE_WARN) {
			result.error =
				QStringLiteral("could not read the archive: %1")
					.arg(archiveError(reader.get(),
									  QStringLiteral("unknown error")));
			return result;
		}

		const QString entryPath =
			QString::fromUtf8(archive_entry_pathname_utf8(entry)
								  ? archive_entry_pathname_utf8(entry)
								  : archive_entry_pathname(entry));

		const QString target = safeDestination(destination, entryPath);
		if (target.isEmpty()) {
			// Refuse the whole archive. A release that contains a path
			// escaping its own directory is either broken or hostile, and
			// installing "most of" it is not a sane middle ground.
			result.error =
				QStringLiteral("the archive contains an unsafe path: %1")
					.arg(entryPath);
			return result;
		}

		// auto, not mode_t: libarchive supplies its own integer type for this
		// and mode_t does not exist in the MSVC headers at all.
		const auto entryType = archive_entry_filetype(entry);

		if (entryType == AE_IFDIR) {
			if (!QDir().mkpath(target)) {
				result.error = QStringLiteral("could not create %1")
								   .arg(QDir::toNativeSeparators(target));
				return result;
			}
			continue;
		}

		// Everything below writes into the entry's own directory, so make it
		// before deciding what kind of thing goes in it.
		if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
			result.error =
				QStringLiteral("could not create %1")
					.arg(QDir::toNativeSeparators(QFileInfo(target).absolutePath()));
			return result;
		}

		// A hardlink entry carries no data of its own -- it names another
		// entry, which tar always stores first. This is not an exotic case:
		// the Linux portable bundle is a sharun bundle, where bin/kite,
		// bin/kite-updater and sharun are all hardlinks to a single
		// multi-call loader.
		if (const char* hardlink =
				entryString(archive_entry_hardlink_utf8(entry),
							archive_entry_hardlink(entry));
			hardlink && *hardlink) {
			const QString linkedEntry = QString::fromUtf8(hardlink);
			const QString linkedPath = safeDestination(destination, linkedEntry);
			if (linkedPath.isEmpty()) {
				result.error =
					QStringLiteral("the archive hardlinks %1 to an unsafe "
								   "path: %2")
						.arg(entryPath, linkedEntry);
				return result;
			}
			if (!QFileInfo::exists(linkedPath)) {
				// Out-of-order archive. Failing is right: a launcher that is
				// a dangling hardlink is not a launcher.
				result.error = QStringLiteral("the archive hardlinks %1 to %2, "
											  "which it has not stored yet")
								   .arg(entryPath, linkedEntry);
				return result;
			}

			if (!createLink(target, linkedPath, true, &result.error))
				return result;

			++result.linkCount;
			result.paths.append(destination.relativeFilePath(target));
			continue;
		}

		if (entryType == AE_IFLNK) {
			const QString linkTarget = QString::fromUtf8(
				entryString(archive_entry_symlink_utf8(entry),
							archive_entry_symlink(entry)));

			if (!symlinkTargetStaysInside(destination, target, linkTarget)) {
				result.error =
					QStringLiteral("the archive symlinks %1 outside the "
								   "release: %2")
						.arg(entryPath, linkTarget);
				return result;
			}

			if (!createLink(target, linkTarget, false, &result.error))
				return result;

			++result.linkCount;
			result.paths.append(destination.relativeFilePath(target));
			continue;
		}

		if (entryType != AE_IFREG) {
			// Devices, fifos, sockets. Nothing we publish contains any, and
			// creating them from a downloaded file is not something an
			// updater should be able to do.
			qWarning() << "Skipping special archive entry:" << entryPath;
			continue;
		}

		if (!QDir().mkpath(QFileInfo(target).absolutePath())) {
			result.error =
				QStringLiteral("could not create %1")
					.arg(QDir::toNativeSeparators(QFileInfo(target).absolutePath()));
			return result;
		}

		if (!writeEntry(reader.get(), target, &result.byteCount,
						&result.error)) {
			return result;
		}

		// Carry the executable bit over: on Linux and macOS the launcher and
		// its helpers come out of the archive with it set, and a launcher
		// that is not executable is an update that bricked the install.
		if (const auto mode = archive_entry_perm(entry); (mode & 0111) != 0) {
			QFile::setPermissions(target,
								  QFile::permissions(target) |
									  QFileDevice::ExeOwner |
									  QFileDevice::ExeGroup |
									  QFileDevice::ExeOther);
		}

		++result.fileCount;
		result.paths.append(destination.relativeFilePath(target));
	}

	result.ok = true;
	return result;
}

QString ReleaseArchive::descendIntoSingleRoot(const QString& dir)
{
	const QDir root(dir);
	const QFileInfoList entries =
		root.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);

	if (entries.size() == 1 && entries.first().isDir())
		return entries.first().absoluteFilePath();

	return root.absolutePath();
}
