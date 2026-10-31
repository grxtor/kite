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

#include <QString>
#include <QFileInfo>
#include <QFileInfoList>
#include <QDir>
#include <QSet>
#include <QDateTime>
#include <QHash>
#include "minecraft/mod/Mod.h"
#include <functional>
#include <optional>

struct archive;

namespace MMCZip
{
	using FilterFunction = std::function<bool(const QString&)>;

	/** Like FilterFunction but asked about a file: the export side already
	 * holds QFileInfo and the ignore models need more than the name (symlink,
	 * real location, size); rebuilding a QFileInfo from a path would stat()
	 * every file twice. */
	using FilterFileFunction = std::function<bool(const QFileInfo&)>;

	/**
	 * Merge two zip files, using a filter function.
	 * Reads entries from 'from' and writes them into the zip at 'intoPath'.
	 * 'contained' tracks already-added filenames to avoid duplicates.
	 */
	bool mergeZipFiles(const QString& intoPath, QFileInfo from,
					   QSet<QString>& contained,
					   const FilterFunction filter = nullptr);

	bool createModdedJar(QString sourceJarPath, QString targetJarPath,
						 const QList<Mod>& mods);

	/**
	 * Find a single file in archive by file name (not path).
	 * \return the path prefix where the file is
	 */
	QString findFolderOfFileInZip(const QString& zipPath, const QString& what,
								  const QString& root = QString(""));

	/**
	 * Find multiple files of the same name in archive by file name.
	 * If a file is found in a path, no deeper paths are searched.
	 * \return true if anything was found
	 */
	bool findFilesInZip(const QString& zipPath, const QString& what,
						QStringList& result, const QString& root = QString());

	/**
	 * Reports how far through a long running archive operation we are.
	 * Called with (files done, files in total) — total is 0 when nothing
	 * matched the filter.
	 */
	using ProgressFunction = std::function<void(qint64, qint64)>;

	/** Compress a directory into a zip, using a filter function to exclude
	 * entries. \param progress Optional. When given, the directory is walked
	 * once up front to count the entries to be written, so the callback can
	 * report a real percentage. It runs on the calling thread, once before the
	 * first file and once per file written. */
	bool compressDir(QString zipFile, QString dir,
					 FilterFunction excludeFilter,
					 ProgressFunction progress = nullptr);

	/** Walks `rootDir` and collects every file into `files`, skipping what
	 * `excludeFilter` rejects; the counterpart of compressDir() for callers
	 * that must know what they will write first. \param subDir Recursion
	 * state; pass a null QString at the top. \return false when a directory
	 * could not be read, meaning the list is incomplete. */
	bool collectFileListRecursively(const QString& rootDir,
									const QString& subDir,
									QFileInfoList* files,
									FilterFileFunction excludeFilter);

	/** A zip written one entry at a time. Unlike compressDir(), the caller
	 * chooses the order (a generated manifest beside files from disk) and can
	 * stop midway. File contents are streamed in fixed-size blocks, as worlds
	 * and resource packs can be gigabytes. Not copyable: two owners would
	 * close the handle twice. */
	class ZipWriter
	{
	  public:
		explicit ZipWriter(QString path);
		~ZipWriter();

		ZipWriter(const ZipWriter&) = delete;
		ZipWriter& operator=(const ZipWriter&) = delete;

		bool open();

		/**
		 * Finish the archive. Safe to call twice; the destructor calls it
		 * for the paths that bail out early.
		 */
		bool close();

		bool addFile(const QString& sourcePath, const QString& entryName);

		bool addFile(const QString& entryName, const QByteArray& data);

		QString errorString() const
		{
			return m_error;
		}

	  private:
		QString m_path;
		struct archive* m_archive = nullptr;
		QString m_error;
	};

	/** What a long-running extraction reports and how it is asked to stop.
	 * Every member is optional. `progress` is called once before the first
	 * entry and once per entry written; it costs one extra pass over the entry
	 * list to get a total. `isCancelled` is polled between entries: when true
	 * the extraction stops, removes what it wrote and returns nullopt, as for
	 * a damaged archive; the caller knows which happened because it owns the
	 * flag. */
	struct ExtractReporting {
		ProgressFunction progress = nullptr;
		std::function<bool()> isCancelled = nullptr;
		std::function<void(const QString&)> entryStarted = nullptr;
	};

	std::optional<QStringList> extractSubDir(const QString& zipPath,
												const QString& subdir,
												const QString& target);

	/**
	 * Extract a subdirectory from an archive, reporting progress and
	 * honouring cancellation. See ExtractReporting.
	 */
	std::optional<QStringList> extractSubDir(
		const QString& zipPath, const QString& subdir, const QString& target,
		const ExtractReporting& reporting);

	/** Extract a single file relative to the zip root. \param error If
	 * non-null, receives a human readable reason; libarchive's diagnostics
	 * ("Unsupported ZIP compression method") are otherwise only in qWarning
	 * output. */
	bool extractRelFile(const QString& zipPath, const QString& file,
						const QString& target, QString* error = nullptr);

	/** Extract a whole archive into \p dir (the current directory if empty).
	 * \return the full paths extracted, empty on failure. */
	std::optional<QStringList> extractDir(QString fileCompressed,
											 QString dir);

	/** Extract \p subdir of an archive into \p dir (the current directory if
	 * empty). \return the full paths extracted, empty on failure. */
	std::optional<QStringList> extractDir(QString fileCompressed,
											 QString subdir, QString dir);

	bool extractFile(QString fileCompressed, QString file, QString dir);

	/**
	 * Read a file's contents from inside a zip archive.
	 * \return the file data, or empty QByteArray on failure
	 */
	QByteArray readFileFromZip(const QString& zipPath,
							   const QString& entryName);

	/** Read several files from a zip archive in one pass; readFileFromZip()
	 * rescans from the start every time. \return the contents of each entry
	 * that exists, by the name asked for. Missing names are absent; an empty
	 * file is present with empty contents. */
	QHash<QString, QByteArray> readFilesFromZip(const QString& zipPath,
												const QStringList& entryNames);

	QHash<QString, QByteArray> readFilesFromZipData(
		const QByteArray& zipData, const QStringList& entryNames);

	bool entryExists(const QString& zipPath, const QString& entryName);

	QStringList listEntries(const QString& zipPath);

	/**
	 * List entries under a specific directory in a zip archive.
	 * \param type QDir::Files, QDir::Dirs, or both
	 */
	QStringList listEntries(const QString& zipPath, const QString& dirPath,
							QDir::Filters type = QDir::Files | QDir::Dirs);

	QDateTime getEntryModTime(const QString& zipPath, const QString& entryName);
} // namespace MMCZip
