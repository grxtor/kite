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

#include <QDebug>
#include <QTemporaryDir>
#include <QTest>

#include <archive.h>
#include <archive_entry.h>

#include "ArchiveOpen.h"
#include "updater/meshupdater/ReleaseArchive.h"

/*! Unpacking a release archive. Archives are built here with libarchive so cases can cover entry
 * kinds that cannot be stored portably in git: hardlinks, symlinks and hostile paths. A Linux
 * portable release is a sharun bundle of hardlinks and symlinks, and an extractor that keeps only
 * regular files yields the right byte count but no launcher, no updater and no library by soname.
 * Link cases are split from the rest because links are refused on Windows (QFile::link() would
 * write a .lnk) and the executable bit does not exist there (isExecutable() looks at the suffix);
 * tst_PlainTreeExtractsEverywhere covers the link-free tree everywhere. */
class ReleaseArchiveTest : public QObject
{
	Q_OBJECT

  private:
	struct Entry {
		QString path;
		QByteArray contents;  //!< For a regular file.
		QString symlinkTo;    //!< Non-empty for a symlink.
		QString hardlinkTo;   //!< Non-empty for a hardlink.
		bool executable = false;
	};

	static Entry file(const QString& path, const QByteArray& contents,
					  bool executable = false)
	{
		Entry entry;
		entry.path = path;
		entry.contents = contents;
		entry.executable = executable;
		return entry;
	}

	static Entry symlink(const QString& path, const QString& target)
	{
		Entry entry;
		entry.path = path;
		entry.symlinkTo = target;
		return entry;
	}

	static Entry hardlink(const QString& path, const QString& target)
	{
		Entry entry;
		entry.path = path;
		entry.hardlinkTo = target;
		return entry;
	}

	/*! Writes \a entries into a .tar.gz, as the release workflow does (restricted pax). */
	static bool writeArchive(const QString& archivePath,
							 const QList<Entry>& entries)
	{
		archive* writer = archive_write_new();
		if (!writer)
			return false;

		archive_write_add_filter_gzip(writer);
		archive_write_set_format_pax_restricted(writer);

		// Through the same wrapper as the extractor, so non-ASCII archive paths can be created
		// here.
		if (MMCArchive::openForWriting(writer, archivePath) != ARCHIVE_OK) {
			qWarning().noquote()
				<< "could not open" << archivePath << "for writing:"
				<< archive_error_string(writer);
			archive_write_free(writer);
			return false;
		}

		bool ok = true;
		for (const Entry& entry : entries) {
			archive_entry* header = archive_entry_new();

			/* Use the _utf8 setters: the plain ones convert from the process locale, which on
			 * Windows is the ANSI code page and mangles "Ş" without failing, so the archive would
			 * not hold the names this file claims. */
			archive_entry_set_pathname_utf8(header,
											entry.path.toUtf8().constData());
			archive_entry_set_perm(header, entry.executable ? 0755 : 0644);

			if (!entry.symlinkTo.isEmpty()) {
				archive_entry_set_filetype(header, AE_IFLNK);
				archive_entry_set_symlink_utf8(
					header, entry.symlinkTo.toUtf8().constData());
				archive_entry_set_size(header, 0);
			} else if (!entry.hardlinkTo.isEmpty()) {
				archive_entry_set_filetype(header, AE_IFREG);
				archive_entry_set_hardlink_utf8(
					header, entry.hardlinkTo.toUtf8().constData());
				archive_entry_set_size(header, 0);
			} else {
				archive_entry_set_filetype(header, AE_IFREG);
				archive_entry_set_size(header, entry.contents.size());
			}

			if (archive_write_header(writer, header) != ARCHIVE_OK) {
				// Reported, not swallowed, so a failure says why.
				qWarning().noquote()
					<< "could not write a header for" << entry.path << ":"
					<< archive_error_string(writer);
				ok = false;
			} else if (entry.symlinkTo.isEmpty() &&
					   entry.hardlinkTo.isEmpty() &&
					   !entry.contents.isEmpty()) {
				const la_ssize_t written = archive_write_data(
					writer, entry.contents.constData(),
					static_cast<size_t>(entry.contents.size()));
				if (written != entry.contents.size()) {
					qWarning().noquote()
						<< "short write for" << entry.path << ":" << written
						<< "of" << entry.contents.size() << "bytes:"
						<< archive_error_string(writer);
					ok = false;
				}
			}

			archive_entry_free(header);
			if (!ok)
				break;
		}

		archive_write_close(writer);
		archive_write_free(writer);
		return ok;
	}

	/*! A path component of letters no single-byte code page holds (Turkish, Cyrillic, CJK), spelled
	 * as UTF-8 bytes because MSVC decodes plain literals in the ANSI code page without /utf-8. */
	static QString awkwardName()
	{
		return QString::fromUtf8(
			"\xC5\x9E"                                          // Ş U+015E
			"afak-"                                             //
			"\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"  // Привет
			"-"                                                 //
			"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");            // 日本語
	}

	//! The shape of a real Linux portable release, in miniature.
	static QList<Entry> sharunBundle()
	{
		return {
			file(QStringLiteral("Kite"), "#!/usr/bin/env bash\n", true),
			file(QStringLiteral("bin/kite-crashreporter"), "LOADER", true),
			hardlink(QStringLiteral("bin/kite"),
					 QStringLiteral("bin/kite-crashreporter")),
			hardlink(QStringLiteral("bin/kite-updater"),
					 QStringLiteral("bin/kite-crashreporter")),
			hardlink(QStringLiteral("sharun"),
					 QStringLiteral("bin/kite-crashreporter")),
			file(QStringLiteral("shared/bin/kite"), "LAUNCHER", true),
			file(QStringLiteral("shared/bin/kite-updater"), "UPDATER", true),
			file(QStringLiteral("lib/libX11.so.6.4.0"), "XLIB"),
			symlink(QStringLiteral("lib/libX11.so.6"),
					QStringLiteral("libX11.so.6.4.0")),
			file(QStringLiteral("manifest.txt"), "bin/kite\n"),
			file(QStringLiteral("portable.txt"), ""),
		};
	}

  private slots:

	/*! A sharun bundle survives the round trip intact. The launcher and updater exist only as
	 * hardlinks and the library only resolves through a symlink, so one case covers the shipped
	 * failure: files extracted, installation unable to start. */
	void tst_SharunBundleKeepsItsLinks()
	{
#if defined(Q_OS_WIN32)
		QSKIP("Windows cannot create POSIX links and ReleaseArchive refuses "
			  "to fake them; see tst_LinkBearingArchiveIsRefusedOnWindows.");
#else
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QString archivePath =
			work.filePath(QStringLiteral("release.tar.gz"));
		QVERIFY(writeArchive(archivePath, sharunBundle()));

		const QString dest = work.filePath(QStringLiteral("out"));
		const ReleaseArchive::Result result =
			ReleaseArchive::extract(archivePath, dest);

		QVERIFY2(result.ok, qPrintable(result.error));

		// 7 regular files (launch script, loader, two real binaries, one library, two markers),
		// then 3 hardlinks and 1 symlink.
		QCOMPARE(result.fileCount, 7);
		QCOMPARE(result.linkCount, 4);

		const QDir out(dest);

		for (const QString& path :
			 {QStringLiteral("bin/kite"), QStringLiteral("bin/kite-updater"),
			  QStringLiteral("sharun")}) {
			const QFileInfo info(out.absoluteFilePath(path));
			QVERIFY2(info.exists(), qPrintable(path));
			QVERIFY2(info.isFile(), qPrintable(path));
			// A hardlink looks like the file it links to, so check the content: an empty
			// placeholder is the other failure.
			QCOMPARE(info.size(), qint64(6)); // "LOADER"
			QVERIFY2(info.isExecutable(), qPrintable(path));
		}

		const QFileInfo soname(out.absoluteFilePath("lib/libX11.so.6"));
		QVERIFY(soname.isSymLink());
		QCOMPARE(soname.symLinkTarget(),
				 out.absoluteFilePath("lib/libX11.so.6.4.0"));

		QCOMPARE(QFileInfo(out.absoluteFilePath("shared/bin/kite")).size(),
				 qint64(8)); // "LAUNCHER"
#endif
	}

	/*! On Windows an archive containing links is refused outright: no Windows artifact has any, and
	 * installing part of one is the failure this file exists to prevent. */
	void tst_LinkBearingArchiveIsRefusedOnWindows()
	{
#if !defined(Q_OS_WIN32)
		QSKIP("only Windows refuses links; the other platforms keep them, "
			  "which tst_SharunBundleKeepsItsLinks covers.");
#else
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QString archivePath =
			work.filePath(QStringLiteral("release.tar.gz"));
		QVERIFY(writeArchive(archivePath, sharunBundle()));

		const QString dest = work.filePath(QStringLiteral("out"));
		const ReleaseArchive::Result result =
			ReleaseArchive::extract(archivePath, dest);

		QVERIFY2(!result.ok, "a link-bearing archive was accepted on Windows");
		QVERIFY2(result.error.contains(QStringLiteral("not supported")),
				 qPrintable(result.error));
		QCOMPARE(result.linkCount, 0);

		// Nothing may stand in for a link: an empty placeholder for bin/kite is an installation
		// that does not start.
		const QDir out(dest);
		QVERIFY(!QFileInfo::exists(
			out.absoluteFilePath(QStringLiteral("bin/kite"))));
#endif
	}

	//! Links are reported, so a release that lost them is visible in the log.
	void tst_LinksAreListedAmongThePaths()
	{
#if defined(Q_OS_WIN32)
		QSKIP("there are no links to list on Windows; see "
			  "tst_LinkBearingArchiveIsRefusedOnWindows.");
#else
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QString archivePath = work.filePath(QStringLiteral("r.tar.gz"));
		QVERIFY(writeArchive(archivePath, sharunBundle()));

		const ReleaseArchive::Result result = ReleaseArchive::extract(
			archivePath, work.filePath(QStringLiteral("out")));

		QVERIFY2(result.ok, qPrintable(result.error));
		QVERIFY(result.paths.contains(QStringLiteral("bin/kite")));
		QVERIFY(result.paths.contains(QStringLiteral("lib/libX11.so.6")));
		QCOMPARE(result.paths.size(), result.fileCount + result.linkCount);
#endif
	}

	// -- Hostile archives --------------------------------------------------

	/*! The archive is downloaded, so untrusted: each case must abort the whole extraction, not be
	 * sanitised and let through. */
	void tst_HostileEntries_data()
	{
		QTest::addColumn<QString>("path");
		QTest::addColumn<QString>("symlinkTo");
		QTest::addColumn<QString>("hardlinkTo");

		QTest::newRow("path climbs out") << "../escaped.txt" << "" << "";
		QTest::newRow("path climbs out deeper")
			<< "bin/../../escaped.txt" << "" << "";
		QTest::newRow("absolute path") << "/etc/passwd" << "" << "";
		QTest::newRow("symlink out of the tree")
			<< "bin/evil" << "../../../../etc/passwd" << "";
		QTest::newRow("absolute symlink") << "bin/evil" << "/etc/passwd" << "";
		QTest::newRow("hardlink out of the tree")
			<< "bin/evil" << "" << "../../../../etc/passwd";
	}

	void tst_HostileEntries()
	{
		QFETCH(QString, path);
		QFETCH(QString, symlinkTo);
		QFETCH(QString, hardlinkTo);

		QTemporaryDir work;
		QVERIFY(work.isValid());

		QList<Entry> entries = {
			file(QStringLiteral("bin/honest"), "OK"),
		};
		if (!symlinkTo.isEmpty())
			entries.append(symlink(path, symlinkTo));
		else if (!hardlinkTo.isEmpty())
			entries.append(hardlink(path, hardlinkTo));
		else
			entries.append(file(path, "EVIL"));

		const QString archivePath = work.filePath(QStringLiteral("evil.tar.gz"));
		QVERIFY(writeArchive(archivePath, entries));

		const QString dest = work.filePath(QStringLiteral("out"));
		const ReleaseArchive::Result result =
			ReleaseArchive::extract(archivePath, dest);

		QVERIFY2(!result.ok, "a hostile archive was accepted");
		QVERIFY(!result.error.isEmpty());

		QVERIFY(!QFileInfo::exists(work.filePath(QStringLiteral("escaped.txt"))));
	}

	/*! A hardlink to something the archive has not stored yet is refused (tar writes targets first,
	 * so this means a damaged or hand-made archive). */
	void tst_HardlinkToAMissingTargetIsRefused()
	{
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QList<Entry> entries = {
			hardlink(QStringLiteral("bin/kite"),
					 QStringLiteral("bin/never-stored")),
		};

		const QString archivePath = work.filePath(QStringLiteral("r.tar.gz"));
		QVERIFY(writeArchive(archivePath, entries));

		const ReleaseArchive::Result result = ReleaseArchive::extract(
			archivePath, work.filePath(QStringLiteral("out")));

		QVERIFY(!result.ok);
		QVERIFY(result.error.contains(QStringLiteral("not stored yet")));
	}

	// -- Ordinary behaviour ------------------------------------------------

	//! The executable bit survives.
	void tst_ExecutableBitIsKept()
	{
#if defined(Q_OS_WIN32)
		QSKIP("Windows has no executable bit: QFileInfo::isExecutable() "
			  "answers from the file's suffix, so this would assert nothing "
			  "about the extractor. tst_PlainTreeExtractsEverywhere covers "
			  "the same archive there.");
#else
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QList<Entry> entries = {
			file(QStringLiteral("bin/runnable"), "X", true),
			file(QStringLiteral("share/plain.txt"), "X", false),
		};

		const QString archivePath = work.filePath(QStringLiteral("r.tar.gz"));
		QVERIFY(writeArchive(archivePath, entries));

		const QString dest = work.filePath(QStringLiteral("out"));
		QVERIFY(ReleaseArchive::extract(archivePath, dest).ok);

		QVERIFY(QFileInfo(QDir(dest).absoluteFilePath("bin/runnable"))
					.isExecutable());
		QVERIFY(!QFileInfo(QDir(dest).absoluteFilePath("share/plain.txt"))
					 .isExecutable());
#endif
	}

	/*! A release with no links, like the Windows and macOS artifacts, extracts on every platform.
	 * Shaped like a Windows package (executables and libraries at the top, plugins and jars in
	 * subdirectories); the other normal-path cases stop at the first link and never run on Windows. */
	void tst_PlainTreeExtractsEverywhere()
	{
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QList<Entry> entries = {
			file(QStringLiteral("kite.exe"), "LAUNCHER", true),
			file(QStringLiteral("kite-updater.exe"), "UPDATER", true),
			file(QStringLiteral("Qt6Core.dll"), "QTCORE"),
			file(QStringLiteral("platforms/qwindows.dll"), "PLUGIN"),
			file(QStringLiteral("jars/NewLaunch.jar"), "JAR"),
			file(QStringLiteral("manifest.txt"), "kite.exe\n"),
		};

		const QString archivePath =
			work.filePath(QStringLiteral("plain.tar.gz"));
		QVERIFY(writeArchive(archivePath, entries));

		const QString dest = work.filePath(QStringLiteral("out"));
		const ReleaseArchive::Result result =
			ReleaseArchive::extract(archivePath, dest);

		QVERIFY2(result.ok, qPrintable(result.error));
		QCOMPARE(result.fileCount, int(entries.size()));
		QCOMPARE(result.linkCount, 0);
		QCOMPARE(int(result.paths.size()), int(entries.size()));

		const QDir out(dest);
		qint64 expectedBytes = 0;
		for (const Entry& entry : entries) {
			const QFileInfo info(out.absoluteFilePath(entry.path));
			QVERIFY2(info.isFile(), qPrintable(entry.path));
			QCOMPARE(info.size(), qint64(entry.contents.size()));
			// Paths are reported relative to the destination, with '/' on every platform.
			QVERIFY2(result.paths.contains(entry.path),
					 qPrintable(entry.path));
			expectedBytes += entry.contents.size();
		}
		QCOMPARE(result.byteCount, expectedBytes);

		QVERIFY(QFileInfo::exists(
			out.absoluteFilePath(QStringLiteral("kite.exe"))));
		QVERIFY(QFileInfo::exists(
			out.absoluteFilePath(QStringLiteral("kite-updater.exe"))));
	}

	/*! A release is unpacked from, and into, a non-ASCII path. archive_read_open_filename() takes a
	 * `char*` that Windows libarchive decodes in the ANSI code page, so UTF-8 bytes break for a
	 * user named Şafak. Passes on Linux either way, but fails if the narrow call comes back. The
	 * awkward name sits in the directory holding the archive, the archive's file name, an entry
	 * inside it and the destination: four different code paths. */
	void tst_NonAsciiPathsAreOpened()
	{
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QString awkward = awkwardName();

		// Stands in for C:\Users\Şafak\AppData\Roaming\Kite\update.
		const QString nest = work.filePath(awkward);
		QVERIFY(QDir().mkpath(nest));

		const QString archivePath =
			QDir(nest).filePath(awkward + QStringLiteral(".tar.gz"));
		const QList<Entry> entries = {
			file(QStringLiteral("kite.exe"), "LAUNCHER", true),
			file(QStringLiteral("Qt6Core.dll"), "QTCORE"),
			file(awkward + QStringLiteral("/inside.txt"), "ENTRY"),
		};

		QVERIFY2(writeArchive(archivePath, entries),
				 "could not create an archive at a non-ASCII path");
		QVERIFY(QFileInfo::exists(archivePath));

		const QString dest =
			QDir(nest).filePath(QStringLiteral("out-") + awkward);
		const ReleaseArchive::Result result =
			ReleaseArchive::extract(archivePath, dest);

		QVERIFY2(result.ok, qPrintable(result.error));
		QCOMPARE(result.fileCount, int(entries.size()));
		QCOMPARE(result.linkCount, 0);

		const QDir out(dest);
		for (const Entry& entry : entries) {
			const QFileInfo info(out.absoluteFilePath(entry.path));
			QVERIFY2(info.isFile(), qPrintable(entry.path));
			QCOMPARE(info.size(), qint64(entry.contents.size()));
			QVERIFY2(result.paths.contains(entry.path),
					 qPrintable(entry.path));
		}
	}

	void tst_MissingArchiveIsReportedNotCrashed()
	{
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const ReleaseArchive::Result result = ReleaseArchive::extract(
			work.filePath(QStringLiteral("nope.tar.gz")),
			work.filePath(QStringLiteral("out")));

		QVERIFY(!result.ok);
		QVERIFY(!result.error.isEmpty());
	}

	void tst_GarbageIsReportedNotCrashed()
	{
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QString archivePath = work.filePath(QStringLiteral("junk.tar.gz"));
		QFile junk(archivePath);
		QVERIFY(junk.open(QIODevice::WriteOnly));
		junk.write(QByteArray(4096, '\x17'));
		junk.close();

		const ReleaseArchive::Result result = ReleaseArchive::extract(
			archivePath, work.filePath(QStringLiteral("out")));

		QVERIFY(!result.ok);
		QVERIFY(!result.error.isEmpty());
	}

	/*! Extracts a real published release and accounts for every entry. Opt-in, it needs a 100+ MB
	 * archive: KITE_TEST_RELEASE_ARCHIVE=/path/to/Kite-Linux-Qt6-Portable-v10.0.0.tar.gz
	 * ./ReleaseArchive_test. The archive is read a second time to build the expectation, so it
	 * holds for any release: every declared entry must exist afterwards as the same kind of thing. */
	void tst_RealReleaseArchiveIsFullyAccountedFor()
	{
		const QString archivePath =
			qEnvironmentVariable("KITE_TEST_RELEASE_ARCHIVE");
		if (archivePath.isEmpty())
			QSKIP("set KITE_TEST_RELEASE_ARCHIVE to a release tarball");
		QVERIFY2(QFileInfo::exists(archivePath), qPrintable(archivePath));

		QTemporaryDir work;
		QVERIFY(work.isValid());
		const QString dest = work.filePath(QStringLiteral("out"));

		const ReleaseArchive::Result result =
			ReleaseArchive::extract(archivePath, dest);
		QVERIFY2(result.ok, qPrintable(result.error));

		archive* reader = archive_read_new();
		QVERIFY(reader);
		archive_read_support_filter_all(reader);
		archive_read_support_format_all(reader);
		QCOMPARE(MMCArchive::openForReading(reader, archivePath, 64 * 1024),
				 ARCHIVE_OK);

		const QDir out(dest);
		int files = 0;
		int links = 0;
		int dirs = 0;

		archive_entry* entry = nullptr;
		while (archive_read_next_header(reader, &entry) == ARCHIVE_OK) {
			const QString path = QString::fromUtf8(
				archive_entry_pathname_utf8(entry)
					? archive_entry_pathname_utf8(entry)
					: archive_entry_pathname(entry));
			const QString onDisk = out.absoluteFilePath(path);
			const QFileInfo info(onDisk);

			const char* hardlink = archive_entry_hardlink(entry);
			const bool isHardlink = hardlink && *hardlink;
			const bool isSymlink = archive_entry_filetype(entry) == AE_IFLNK;
			const bool isDir = archive_entry_filetype(entry) == AE_IFDIR;

			if (isDir) {
				++dirs;
				QVERIFY2(info.isDir(), qPrintable(path));
				continue;
			}

			// A QFileInfo on a symlink describes its target, so ask about the link itself first.
			QVERIFY2(info.isSymLink() || info.exists(), qPrintable(path));

			if (isSymlink) {
				++links;
				QVERIFY2(info.isSymLink(), qPrintable(path));
			} else if (isHardlink) {
				++links;
				QVERIFY2(info.isFile(), qPrintable(path));
				// The link must carry the target's content, not be an empty placeholder.
				const QFileInfo target(
					out.absoluteFilePath(QString::fromUtf8(hardlink)));
				QVERIFY2(target.isFile(), qPrintable(path));
				QCOMPARE(info.size(), target.size());
				QVERIFY2(info.size() > 0, qPrintable(path));
			} else {
				++files;
				QVERIFY2(info.isFile(), qPrintable(path));
				QCOMPARE(info.size(), qint64(archive_entry_size(entry)));
			}
		}
		archive_read_free(reader);

		qDebug() << "archive declared" << files << "files," << links
				 << "links," << dirs << "directories";
		QCOMPARE(result.fileCount, files);
		QCOMPARE(result.linkCount, links);

		// The two entry points the update depends on (hardlinks to the sharun loader in a real
		// bundle): bin/kite for the launch script, bin/kite-updater for the hand-off.
		QVERIFY(QFileInfo(out.absoluteFilePath("bin/kite")).isExecutable());
		QVERIFY(QFileInfo(out.absoluteFilePath("bin/kite-updater"))
					.isExecutable());
	}

	// -- descendIntoSingleRoot --------------------------------------------

	void tst_SingleRootIsDescendedInto()
	{
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QString dest = work.filePath(QStringLiteral("out"));
		QVERIFY(QDir().mkpath(dest + QStringLiteral("/Kite-1.2.3/bin")));

		QCOMPARE(ReleaseArchive::descendIntoSingleRoot(dest),
				 QDir(dest).absoluteFilePath(QStringLiteral("Kite-1.2.3")));
	}

	//! A release whose files sit at the top level is returned unchanged, as our own tarballs are.
	void tst_FlatTreeIsReturnedUnchanged()
	{
		QTemporaryDir work;
		QVERIFY(work.isValid());

		const QString dest = work.filePath(QStringLiteral("out"));
		QVERIFY(QDir().mkpath(dest + QStringLiteral("/bin")));
		QVERIFY(QDir().mkpath(dest + QStringLiteral("/lib")));

		QCOMPARE(ReleaseArchive::descendIntoSingleRoot(dest),
				 QDir(dest).absolutePath());
	}
};

QTEST_GUILESS_MAIN(ReleaseArchiveTest)

#include "ReleaseArchive_test.moc"
