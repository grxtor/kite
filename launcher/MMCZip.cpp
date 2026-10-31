/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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

#include "MMCZip.h"
#include "ArchiveOpen.h"
#include "FileSystem.h"

#include <archive.h>
#include <archive_entry.h>

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>

#include <memory>

struct ArchiveReadDeleter {
	void operator()(struct archive* a) const
	{
		archive_read_free(a);
	} // NOLINT(readability-identifier-naming)
};
struct ArchiveWriteDeleter {
	void operator()(struct archive* a) const
	{
		archive_write_free(a);
	} // NOLINT(readability-identifier-naming)
};
using ArchiveReadPtr = std::unique_ptr<
	struct archive,
	ArchiveReadDeleter>; // NOLINT(readability-identifier-naming)
using ArchiveWritePtr = std::unique_ptr<
	struct archive,
	ArchiveWriteDeleter>; // NOLINT(readability-identifier-naming)

/* The name of an entry, NFC-composed. libarchive decomposes UTF-8 names on
 * Apple platforms (to match HFS Plus), so one zip yields different strings on
 * macOS and elsewhere and lookups by name miss; NFC gives one form everywhere.
 * Uses the narrow archive_entry_pathname(), not the _utf8 accessor: writers
 * hand libarchive UTF-8 bytes through archive_entry_set_pathname(), stored
 * unconverted, and the _utf8 accessor would convert them out of the local
 * charset (on Windows the ANSI code page), which broke both Windows builds.
 * Change one end of this pair and the other must change. */
static QString entryPathName(struct archive_entry* entry)
{
	const char* name = archive_entry_pathname(entry);
	if (!name)
		return {};
	return QString::fromUtf8(name).normalized(QString::NormalizationForm_C);
}

static ArchiveReadPtr openZipForReading(const QString& path)
{
	ArchiveReadPtr a(archive_read_new());
	archive_read_support_format_zip(a.get());
	if (MMCArchive::openForReading(a.get(), path, 10240) != ARCHIVE_OK) {
		qWarning() << "Could not open archive:" << path
				   << archive_error_string(a.get());
		return nullptr;
	}
	return a;
}

// A zip already in memory (a jar bundled inside another). `data` must outlive
// the returned handle, which reads straight out of it.
static ArchiveReadPtr openZipFromMemory(const QByteArray& data)
{
	ArchiveReadPtr a(archive_read_new());
	archive_read_support_format_zip(a.get());
	if (archive_read_open_memory(a.get(), data.constData(), data.size()) !=
		ARCHIVE_OK) {
		qWarning() << "Could not open in-memory archive:"
				   << archive_error_string(a.get());
		return nullptr;
	}
	return a;
}

static ArchiveWritePtr createZipForWriting(const QString& path)
{
	ArchiveWritePtr a(archive_write_new());
	archive_write_set_format_zip(a.get());
	if (MMCArchive::openForWriting(a.get(), path) != ARCHIVE_OK) {
		qWarning() << "Could not create archive:" << path
				   << archive_error_string(a.get());
		return nullptr;
	}
	return a;
}

static bool writeFileToArchive(
	struct archive* aw,
	const QString& entryName, // NOLINT(readability-identifier-naming)
	const QByteArray& data)
{
	struct archive_entry* entry = archive_entry_new();
	archive_entry_set_pathname(entry, entryName.toUtf8().constData());
	archive_entry_set_size(entry, data.size());
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, 0644);

	if (archive_write_header(aw, entry) != ARCHIVE_OK) {
		archive_entry_free(entry);
		return false;
	}
	if (data.size() > 0) {
		archive_write_data(aw, data.constData(), data.size());
	}
	archive_entry_free(entry);
	return true;
}

static bool writeDiskEntry(struct archive* ar, const QString& absFilePath,
						   QString* error = nullptr)
{
	QFileInfo fi(absFilePath);
	QDir().mkpath(fi.absolutePath());

	if (absFilePath.endsWith('/')) {
		QDir().mkpath(absFilePath);
		return true;
	}

	QFile outFile(absFilePath);
	if (!outFile.open(QIODevice::WriteOnly)) {
		qWarning() << "Failed to open for writing:" << absFilePath;
		if (error) {
			*error = QStringLiteral("could not open '%1' for writing: %2")
						 .arg(absFilePath, outFile.errorString());
		}
		return false;
	}

	// Every bail-out leaves a truncated file behind, and a zero-byte native
	// library or mod jar would be loaded by the next launch, so clean up on
	// every failure path.
	const auto fail = [&outFile, &absFilePath](QString* out,
											   const QString& why) {
		outFile.close();
		if (!QFile::remove(absFilePath)) {
			qWarning() << "Could not remove partial file" << absFilePath;
		}
		if (out) {
			*out = why;
		}
		return false;
	};

	const void* buff;
	size_t size;
	la_int64_t offset;
	int readRet;
	while ((readRet = archive_read_data_block(ar, &buff, &size, &offset)) ==
		   ARCHIVE_OK) {
		qint64 written = outFile.write(static_cast<const char*>(buff), size);
		if (written != static_cast<qint64>(size)) {
			qWarning() << "Write error for" << absFilePath << ": wrote"
					   << written << "of" << size << "bytes";
			return fail(error,
						QStringLiteral("write error for '%1': %2")
							.arg(absFilePath, outFile.errorString()));
		}
	}

	if (readRet != ARCHIVE_EOF) {
		const char* archiveError = archive_error_string(ar);
		qWarning() << "Archive read error while extracting" << absFilePath
				   << ":" << archiveError;
		return fail(error, QString::fromUtf8(archiveError
												 ? archiveError
												 : "unknown archive error"));
	}

	outFile.close();
	return true;
}

bool MMCZip::compressDir(QString zipFile, QString dir,
						 FilterFunction excludeFilter,
						 ProgressFunction progress)
{
	auto aw = createZipForWriting(zipFile);
	if (!aw)
		return false;

	QDir directory(dir);
	if (!directory.exists())
		return false;

	// Counting up front costs one directory walk with no file reads, cheap
	// next to compressing, and turns a sweeping bar into a percentage. Only
	// paid when someone listens.
	qint64 total = 0;
	qint64 done = 0;
	if (progress) {
		QDirIterator counter(dir, QDir::Files | QDir::Hidden,
							 QDirIterator::Subdirectories);
		while (counter.hasNext()) {
			counter.next();
			if (excludeFilter &&
				excludeFilter(directory.relativeFilePath(counter.filePath())))
				continue;
			total++;
		}
		progress(0, total);
	}

	QDirIterator it(dir, QDir::Files | QDir::Hidden,
					QDirIterator::Subdirectories);
	bool success = true;
	while (it.hasNext()) {
		it.next();
		QString relPath = directory.relativeFilePath(it.filePath());
		if (excludeFilter && excludeFilter(relPath))
			continue;

		QFile file(it.filePath());
		if (!file.open(QIODevice::ReadOnly)) {
			success = false;
			break;
		}
		QByteArray data = file.readAll();
		file.close();

		if (!writeFileToArchive(aw.get(), relPath, data)) {
			success = false;
			break;
		}

		if (progress) {
			progress(++done, total);
		}
	}

	archive_write_close(aw.get());
	return success;
}

bool MMCZip::collectFileListRecursively(const QString& rootDir,
									   const QString& subDir,
									   QFileInfoList* files,
									   FilterFileFunction excludeFilter)
{
	QDir rootDirectory(rootDir);
	if (!rootDirectory.exists())
		return false;

	QDir directory = subDir.isNull() ? rootDirectory : QDir(subDir);
	if (!directory.exists())
		return false;

	// Subdirectories first, so the list is depth-first and stable rather than
	// in readdir order.
	const QFileInfoList dirs = directory.entryInfoList(
		QDir::AllDirs | QDir::NoDotAndDotDot | QDir::Hidden);
	for (const auto& dir : dirs) {
		/* A directory the user unchecked is not walked: the filter already
		 * says no to everything in it, and descending into a blocked
		 * `.minecraft/logs` is wasted work. */
		if (excludeFilter && excludeFilter(dir))
			continue;
		if (!collectFileListRecursively(rootDir, dir.filePath(), files,
										excludeFilter))
			return false;
	}

	const QFileInfoList entries =
		directory.entryInfoList(QDir::Files | QDir::Hidden | QDir::System);
	for (const auto& entry : entries) {
		if (excludeFilter && excludeFilter(entry)) {
			qDebug() << "Skipping file"
					 << rootDirectory.relativeFilePath(
							entry.absoluteFilePath());
			continue;
		}
		files->append(entry);
	}
	return true;
}

MMCZip::ZipWriter::ZipWriter(QString path) : m_path(std::move(path)) {}

MMCZip::ZipWriter::~ZipWriter()
{
	(void)close();
}

bool MMCZip::ZipWriter::open()
{
	if (m_archive) {
		return true;
	}
	if (m_path.isEmpty()) {
		m_error = QStringLiteral("no output path was given");
		return false;
	}

	m_archive = archive_write_new();
	if (!m_archive) {
		m_error = QStringLiteral("could not allocate an archive writer");
		return false;
	}
	archive_write_set_format_zip(m_archive);
	/* Names go in as UTF-8 and are flagged as such, so a pack with non-Latin
	 * file names survives other tools. */
	if (archive_write_set_options(m_archive, "hdrcharset=UTF-8") !=
		ARCHIVE_OK) {
		qWarning() << "Could not set archive options for" << m_path << ":"
				   << archive_error_string(m_archive);
	}

	if (MMCArchive::openForWriting(m_archive, m_path) != ARCHIVE_OK) {
		m_error = QString::fromUtf8(archive_error_string(m_archive));
		qWarning() << "Could not create archive:" << m_path << m_error;
		archive_write_free(m_archive);
		m_archive = nullptr;
		return false;
	}
	return true;
}

bool MMCZip::ZipWriter::close()
{
	if (!m_archive) {
		return true;
	}
	bool success = true;
	if (archive_write_close(m_archive) != ARCHIVE_OK) {
		m_error = QString::fromUtf8(archive_error_string(m_archive));
		qWarning() << "Could not close archive" << m_path << m_error;
		success = false;
	}
	archive_write_free(m_archive);
	m_archive = nullptr;
	return success;
}

bool MMCZip::ZipWriter::addFile(const QString& entryName,
								const QByteArray& data)
{
	if (!m_archive) {
		m_error = QStringLiteral("archive is not open");
		return false;
	}
	if (!writeFileToArchive(m_archive, entryName, data)) {
		m_error = QString::fromUtf8(archive_error_string(m_archive));
		return false;
	}
	return true;
}

bool MMCZip::ZipWriter::addFile(const QString& sourcePath,
								const QString& entryName)
{
	if (!m_archive) {
		m_error = QStringLiteral("archive is not open");
		return false;
	}

	QFile file(sourcePath);
	if (!file.open(QIODevice::ReadOnly)) {
		m_error = QStringLiteral("could not read '%1': %2")
					  .arg(sourcePath, file.errorString());
		return false;
	}

	const QFileInfo info(sourcePath);
	const qint64 size = file.size();

	struct archive_entry* entry = archive_entry_new();
	archive_entry_set_pathname(entry, entryName.toUtf8().constData());
	archive_entry_set_size(entry, size);
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_perm(entry, info.isExecutable() ? 0755 : 0644);
	/* Kept so an exported instance shows when its files were last touched;
	 * entries all dated "now" make every diff against the original look like a
	 * change. */
	const QDateTime modified = info.lastModified();
	if (modified.isValid()) {
		archive_entry_set_mtime(entry, modified.toSecsSinceEpoch(), 0);
	}

	if (archive_write_header(m_archive, entry) != ARCHIVE_OK) {
		m_error = QString::fromUtf8(archive_error_string(m_archive));
		archive_entry_free(entry);
		return false;
	}
	archive_entry_free(entry);

	/* Streamed, not read whole: worlds and resource packs are often too big to
	 * hold in memory. */
	QByteArray buffer(64 * 1024, Qt::Uninitialized);
	qint64 written = 0;
	while (written < size) {
		const qint64 read = file.read(buffer.data(), buffer.size());
		if (read < 0) {
			m_error = QStringLiteral("could not read '%1': %2")
						  .arg(sourcePath, file.errorString());
			return false;
		}
		if (read == 0) {
			/* The file shrank while being read. The header promised `size`
			 * bytes, so the entry cannot be completed; failing beats writing
			 * an archive libarchive considers malformed. */
			m_error = QStringLiteral("'%1' changed size while being read")
						  .arg(sourcePath);
			return false;
		}
		if (archive_write_data(m_archive, buffer.constData(), read) < 0) {
			m_error = QString::fromUtf8(archive_error_string(m_archive));
			return false;
		}
		written += read;
	}

	return true;
}

bool MMCZip::mergeZipFiles(const QString& intoPath, QFileInfo from,
						   QSet<QString>& contained,
						   const FilterFunction filter)
{
	auto ar = openZipForReading(from.filePath());
	if (!ar)
		return false;

	struct EntryData {
		QString name;
		QByteArray data;
	};
	QList<EntryData> newEntries;

	struct archive_entry* entry;
	while (archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
		QString filename = entryPathName(entry);
		if (filter && !filter(filename)) {
			qDebug() << "Skipping file " << filename << " from "
					 << from.fileName() << " - filtered";
			archive_read_data_skip(ar.get());
			continue;
		}
		if (contained.contains(filename)) {
			qDebug() << "Skipping already contained file " << filename
					 << " from " << from.fileName();
			archive_read_data_skip(ar.get());
			continue;
		}
		contained.insert(filename);

		la_int64_t entrySize = archive_entry_size(entry);
		QByteArray data;
		if (entrySize > 0) {
			data.resize(entrySize);
			la_ssize_t bytesRead =
				archive_read_data(ar.get(), data.data(), entrySize);
			if (bytesRead < 0) {
				qCritical() << "Failed to read " << filename << " from "
							<< from.fileName();
				return false;
			}
			data.resize(bytesRead);
		}
		newEntries.append({filename, data});
	}

	// libarchive cannot append: read the existing entries and write them all
	// again.
	QList<EntryData> existingEntries;
	if (QFile::exists(intoPath)) {
		auto existingAr = openZipForReading(intoPath);
		if (existingAr) {
			while (archive_read_next_header(existingAr.get(), &entry) ==
				   ARCHIVE_OK) {
				QString name = entryPathName(entry);
				la_int64_t sz = archive_entry_size(entry);
				QByteArray data;
				if (sz > 0) {
					data.resize(sz);
					archive_read_data(existingAr.get(), data.data(), sz);
				}
				existingEntries.append({name, data});
			}
		}
	}

	auto aw = createZipForWriting(intoPath);
	if (!aw)
		return false;

	for (const auto& e : existingEntries) {
		if (!writeFileToArchive(aw.get(), e.name, e.data)) {
			qCritical() << "Failed to write existing entry " << e.name;
			return false;
		}
	}

	for (const auto& e : newEntries) {
		if (!writeFileToArchive(aw.get(), e.name, e.data)) {
			qCritical() << "Failed to write " << e.name << " into the jar";
			return false;
		}
	}

	archive_write_close(aw.get());
	return true;
}

bool MMCZip::createModdedJar(QString sourceJarPath, QString targetJarPath,
							 const QList<Mod>& mods)
{
	QSet<QString> addedFiles;

	struct EntryData {
		QString name;
		QByteArray data;
	};
	QList<EntryData> allEntries;

	QListIterator<Mod> i(mods);
	i.toBack();
	while (i.hasPrevious()) {
		const Mod& mod = i.previous();
		if (!mod.enabled())
			continue;

		if (mod.type() == Mod::MOD_ZIPFILE) {
			auto ar = openZipForReading(mod.filename().absoluteFilePath());
			if (!ar) {
				QFile::remove(targetJarPath);
				qCritical() << "Failed to add" << mod.filename().fileName()
							<< "to the jar.";
				return false;
			}
			struct archive_entry* entry;
			while (archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
				QString filename = entryPathName(entry);
				if (addedFiles.contains(filename)) {
					archive_read_data_skip(ar.get());
					continue;
				}
				addedFiles.insert(filename);
				la_int64_t sz = archive_entry_size(entry);
				QByteArray data;
				if (sz > 0) {
					data.resize(sz);
					archive_read_data(ar.get(), data.data(), sz);
				}
				allEntries.append({filename, data});
			}
		} else if (mod.type() == Mod::MOD_SINGLEFILE) {
			auto filename = mod.filename();
			QFile file(filename.absoluteFilePath());
			if (!file.open(QIODevice::ReadOnly)) {
				QFile::remove(targetJarPath);
				qCritical()
					<< "Failed to add" << filename.fileName() << "to the jar.";
				return false;
			}
			allEntries.append({filename.fileName(), file.readAll()});
			addedFiles.insert(filename.fileName());
		} else if (mod.type() == Mod::MOD_FOLDER) {
			auto filename = mod.filename();
			QDir modRoot(filename.absoluteFilePath());
			QDirIterator it(modRoot.absolutePath(), QDir::Files | QDir::Hidden,
							QDirIterator::Subdirectories);
			while (it.hasNext()) {
				it.next();
				QString relPath = modRoot.relativeFilePath(it.filePath());
				if (addedFiles.contains(relPath)) {
					continue;
				}
				QFile f(it.filePath());
				if (!f.open(QIODevice::ReadOnly)) {
					QFile::remove(targetJarPath);
					qCritical() << "Failed to add" << relPath << "of folder mod"
								<< filename.fileName() << "to the jar.";
					return false;
				}
				addedFiles.insert(relPath);
				allEntries.append({relPath, f.readAll()});
			}
			qDebug() << "Adding folder " << filename.fileName() << " from "
					 << filename.absoluteFilePath();
		} else {
			QFile::remove(targetJarPath);
			qCritical() << "Failed to add unknown mod type"
						<< mod.filename().fileName() << "to the jar.";
			return false;
		}
	}

	auto ar = openZipForReading(sourceJarPath);
	if (!ar) {
		QFile::remove(targetJarPath);
		qCritical() << "Failed to insert minecraft.jar contents.";
		return false;
	}
	struct archive_entry* entry;
	while (archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
		QString filename = entryPathName(entry);
		if (filename.contains("META-INF")) {
			archive_read_data_skip(ar.get());
			continue;
		}
		if (addedFiles.contains(filename)) {
			archive_read_data_skip(ar.get());
			continue;
		}
		la_int64_t sz = archive_entry_size(entry);
		QByteArray data;
		if (sz > 0) {
			data.resize(sz);
			archive_read_data(ar.get(), data.data(), sz);
		}
		allEntries.append({filename, data});
	}

	auto aw = createZipForWriting(targetJarPath);
	if (!aw) {
		QFile::remove(targetJarPath);
		qCritical() << "Failed to open the minecraft.jar for modding";
		return false;
	}
	for (const auto& e : allEntries) {
		if (!writeFileToArchive(aw.get(), e.name, e.data)) {
			archive_write_close(aw.get());
			(void)QFile::remove(targetJarPath);
			qCritical() << "Failed to finalize minecraft.jar!";
			return false;
		}
	}
	archive_write_close(aw.get());
	return true;
}

QString MMCZip::findFolderOfFileInZip(const QString& zipPath,
									  const QString& what, const QString& root)
{
	auto entries = listEntries(zipPath, root, QDir::Files);
	for (const auto& fileName : entries) {
		if (fileName == what)
			return root;
	}
	auto dirs = listEntries(zipPath, root, QDir::Dirs);
	for (const auto& dirName : dirs) {
		QString result = findFolderOfFileInZip(zipPath, what, root + dirName);
		if (!result.isEmpty())
			return result;
	}
	return QString();
}

bool MMCZip::findFilesInZip(const QString& zipPath, const QString& what,
							QStringList& result, const QString& root)
{
	auto entries = listEntries(zipPath, root, QDir::Files);
	for (const auto& fileName : entries) {
		if (fileName == what) {
			result.append(root);
			return true;
		}
	}
	auto dirs = listEntries(zipPath, root, QDir::Dirs);
	for (const auto& dirName : dirs) {
		(void)findFilesInZip(zipPath, what, result, root + dirName);
	}
	return !result.isEmpty();
}

std::optional<QStringList> MMCZip::extractSubDir(const QString& zipPath,
													const QString& subdir,
													const QString& target)
{
	return MMCZip::extractSubDir(zipPath, subdir, target, ExtractReporting{});
}

std::optional<QStringList>
MMCZip::extractSubDir(const QString& zipPath, const QString& subdir,
					  const QString& target,
					  const ExtractReporting& reporting)
{
	QDir directory(target);
	QStringList extracted;

	qDebug() << "Extracting subdir" << subdir << "from" << zipPath << "to"
			 << target;

	/* One extra pass over the entry list, with no file data read, turns the
	 * bar into a percentage. Only paid when somebody is watching. */
	qint64 total = 0;
	qint64 done = 0;
	if (reporting.progress) {
		for (const QString& name : listEntries(zipPath)) {
			if (name.startsWith(subdir))
				total++;
		}
		reporting.progress(0, total);
	}

	auto ar = openZipForReading(zipPath);
	if (!ar) {
		// check if this is a minimum size empty zip file...
		QFileInfo fileInfo(zipPath);
		if (fileInfo.size() == 22) {
			return extracted;
		}
		qWarning() << "Failed to open archive:" << zipPath;
		return std::nullopt;
	}

	struct archive_entry* entry;
	bool hasEntries = false;
	int readRet;
	while ((readRet = archive_read_next_header(ar.get(), &entry)) ==
		   ARCHIVE_OK) {
		/* Between entries, not inside one: a half-written file is worse than
		 * one more file, and the wait is bounded by the largest entry. */
		if (reporting.isCancelled && reporting.isCancelled()) {
			qDebug() << "Extraction of" << zipPath << "cancelled";
			for (const auto& f : extracted)
				(void)QFile::remove(f);
			return std::nullopt;
		}

		hasEntries = true;
		QString name = entryPathName(entry);
		if (!name.startsWith(subdir)) {
			archive_read_data_skip(ar.get());
			continue;
		}
		QString relName = name.mid(subdir.size());
		QString absFilePath = directory.absoluteFilePath(relName);
		// Refuse "../" and absolute entry names: they would write outside the
		// destination (zip-slip).
		if (!relName.isEmpty()) {
			const QString base = QDir::cleanPath(directory.absolutePath());
			const QString candidate = QDir::cleanPath(absFilePath);
			if (candidate != base &&
				!candidate.startsWith(base + QLatin1Char('/'))) {
				qWarning() << "Archive entry escapes the destination:" << name;
				for (const auto& f : extracted)
					(void)QFile::remove(f);
				return std::nullopt;
			}
		}
		if (relName.isEmpty()) {
			absFilePath += "/";
		}

		if (reporting.entryStarted) {
			reporting.entryStarted(relName);
		}

		if (!writeDiskEntry(ar.get(), absFilePath)) {
			qWarning() << "Failed to extract file" << name << "to"
					   << absFilePath;
			for (const auto& f : extracted)
				(void)QFile::remove(f);
			return std::nullopt;
		}
		extracted.append(absFilePath);
		if (reporting.progress) {
			reporting.progress(++done, total);
		}
		qDebug() << "Extracted file" << relName;
	}

	if (readRet != ARCHIVE_EOF) {
		qWarning() << "Archive read failed for:" << zipPath
				   << "Error:" << archive_error_string(ar.get());
		for (const auto& f : extracted)
			(void)QFile::remove(f);
		return std::nullopt;
	}

	if (!hasEntries) {
		qDebug() << "Extracting empty archives seems odd...";
	}
	return extracted;
}

bool MMCZip::extractRelFile(const QString& zipPath, const QString& file,
							const QString& target, QString* error)
{
	auto ar = openZipForReading(zipPath);
	if (!ar) {
		if (error) {
			*error = QStringLiteral("could not open archive '%1'").arg(zipPath);
		}
		return false;
	}

	struct archive_entry* entry;
	while (archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
		QString name = entryPathName(entry);
		if (name == file) {
			return writeDiskEntry(ar.get(), target, error);
		}
		archive_read_data_skip(ar.get());
	}
	if (error) {
		*error = QStringLiteral("'%1' not found in archive '%2'")
					 .arg(file, zipPath);
	}
	return false;
}

std::optional<QStringList> MMCZip::extractDir(QString fileCompressed,
												 QString dir)
{
	return MMCZip::extractSubDir(fileCompressed, "", dir);
}

std::optional<QStringList> MMCZip::extractDir(QString fileCompressed,
												 QString subdir, QString dir)
{
	return MMCZip::extractSubDir(fileCompressed, subdir, dir);
}

bool MMCZip::extractFile(QString fileCompressed, QString file, QString target)
{
	// check if this is a minimum size empty zip file...
	QFileInfo fileInfo(fileCompressed);
	if (fileInfo.size() == 22) {
		return true;
	}
	return MMCZip::extractRelFile(fileCompressed, file, target);
}

static QByteArray readEntryData(struct archive* ar, struct archive_entry* entry)
{
	la_int64_t sz = archive_entry_size(entry);
	if (sz <= 0) {
		// Size may be unknown; read in chunks
		QByteArray result;
		char buf[8192];
		la_ssize_t r;
		while ((r = archive_read_data(ar, buf, sizeof(buf))) > 0)
			result.append(buf, r);
		return result;
	}
	QByteArray data(sz, Qt::Uninitialized);
	archive_read_data(ar, data.data(), sz);
	return data;
}

// One pass, stopping once every wanted entry has been read. The first entry of
// a name wins.
static QHash<QString, QByteArray> readEntries(struct archive* ar,
											  const QStringList& entryNames)
{
	const QSet<QString> wanted(entryNames.begin(), entryNames.end());
	QHash<QString, QByteArray> found;
	struct archive_entry* entry;
	while (found.size() < wanted.size() &&
		   archive_read_next_header(ar, &entry) == ARCHIVE_OK) {
		const QString name = entryPathName(entry);
		if (wanted.contains(name) && !found.contains(name)) {
			found.insert(name, readEntryData(ar, entry));
		} else {
			archive_read_data_skip(ar);
		}
	}
	return found;
}

QByteArray MMCZip::readFileFromZip(const QString& zipPath,
								   const QString& entryName)
{
	auto ar = openZipForReading(zipPath);
	if (!ar)
		return {};

	return readEntries(ar.get(), {entryName}).value(entryName);
}

QHash<QString, QByteArray> MMCZip::readFilesFromZip(
	const QString& zipPath, const QStringList& entryNames)
{
	auto ar = openZipForReading(zipPath);
	if (!ar)
		return {};

	return readEntries(ar.get(), entryNames);
}

QHash<QString, QByteArray> MMCZip::readFilesFromZipData(
	const QByteArray& zipData, const QStringList& entryNames)
{
	if (zipData.isEmpty())
		return {};
	auto ar = openZipFromMemory(zipData);
	if (!ar)
		return {};

	return readEntries(ar.get(), entryNames);
}

bool MMCZip::entryExists(const QString& zipPath, const QString& entryName)
{
	auto ar = openZipForReading(zipPath);
	if (!ar)
		return false;

	struct archive_entry* entry;
	while (archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
		QString name = entryPathName(entry);
		if (name == entryName || name == entryName + "/")
			return true;
		archive_read_data_skip(ar.get());
	}
	return false;
}

QStringList MMCZip::listEntries(const QString& zipPath)
{
	QStringList result;
	auto ar = openZipForReading(zipPath);
	if (!ar)
		return result;

	struct archive_entry* entry;
	while (archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
		result.append(entryPathName(entry));
		archive_read_data_skip(ar.get());
	}
	return result;
}

QStringList MMCZip::listEntries(const QString& zipPath, const QString& dirPath,
								QDir::Filters type)
{
	QStringList result;
	auto ar = openZipForReading(zipPath);
	if (!ar)
		return result;

	QSet<QString> seen;
	struct archive_entry* entry;
	while (archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
		QString name = entryPathName(entry);

		if (!name.startsWith(dirPath)) {
			archive_read_data_skip(ar.get());
			continue;
		}

		QString relative = name.mid(dirPath.size());
		while (relative.startsWith('/'))
			relative = relative.mid(1);

		if (relative.isEmpty()) {
			archive_read_data_skip(ar.get());
			continue;
		}

		int slashIdx = relative.indexOf('/');
		if (slashIdx < 0) {
			if (type & QDir::Files) {
				if (!seen.contains(relative)) {
					seen.insert(relative);
					result.append(relative);
				}
			}
		} else {
			if (type & QDir::Dirs) {
				QString dirName =
					relative.left(slashIdx + 1);
				if (!seen.contains(dirName)) {
					seen.insert(dirName);
					result.append(dirName);
				}
			}
		}
		archive_read_data_skip(ar.get());
	}
	return result;
}

QDateTime MMCZip::getEntryModTime(const QString& zipPath,
								  const QString& entryName)
{
	auto ar = openZipForReading(zipPath);
	if (!ar)
		return {};

	struct archive_entry* entry;
	while (archive_read_next_header(ar.get(), &entry) == ARCHIVE_OK) {
		QString name = entryPathName(entry);
		if (name == entryName) {
			time_t mtime = archive_entry_mtime(entry);
			return QDateTime::fromSecsSinceEpoch(mtime);
		}
		archive_read_data_skip(ar.get());
	}
	return {};
}
