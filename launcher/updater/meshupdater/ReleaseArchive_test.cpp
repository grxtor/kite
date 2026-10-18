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

/*!
 * Unpacking a release archive.
 *
 * The archives are built here with libarchive rather than checked in, so that
 * a case can be written for entry kinds that are awkward to store in git and
 * impossible to store portably -- hardlinks, symlinks, and the malicious
 * paths that must be refused.
 *
 * The kinds are not academic. A Linux portable release is a sharun bundle:
 *
 *   bin/kite-crashreporter   the multi-call loader, a real file
 *   bin/kite                 hardlink to it
 *   bin/kite-updater         hardlink to it
 *   sharun                     hardlink to it
 *   shared/bin/kite          the real launcher, execed by the loader
 *   lib/libX11.so.6            symlink to libX11.so.6.4.0
 *
 * An extractor that keeps only regular files produces a tree with the right
 * number of bytes in it, no launcher, no updater, and no library the dynamic
 * linker can find by soname. That is what this file exists to prevent.
 *
 * Two of those properties do not exist on Windows, and the cases that cover
 * them are split rather than skipped wholesale, so that each platform is
 * still held to the contract it actually has:
 *
 *   - Links. ReleaseArchive refuses them on Windows instead of emulating
 *     them, because QFile::link() would write a .lnk shortcut that nothing
 *     opening the file would follow. The Windows artifacts are zips built
 *     from a directory tree and contain none, so a link-bearing archive
 *     there means something is wrong -- and refusing the archive is the
 *     documented answer. tst_LinkBearingArchiveIsRefusedOnWindows holds it
 *     to that; the POSIX cases hold the other platforms to keeping links.
 *
 *   - The executable bit. Windows has none: QFileInfo::isExecutable()
 *     answers from the file's suffix, so asserting it there tests Qt's
 *     name lookup and nothing of ours.
 *
 * What is left is the same on every platform -- a release with no links at
 * all, which is exactly what the Windows and macOS artifacts are -- and
 * tst_PlainTreeExtractsEverywhere covers it unconditionally.
 */
class ReleaseArchiveTest : public QObject
{
	Q_OBJECT

  private:
	//! One entry to write into a test archive.
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

	/*!
	 * Write \a entries into a .tar.gz at \a archivePath.
	 *
	 * Mirrors what the release workflow produces: a gzipped tar in the
	 * restricted-pax format libarchive defaults to.
	 */
	static bool writeArchive(const QString& archivePath,
							 const QList<Entry>& entries)
	{
		archive* writer = archive_write_new();
		if (!writer)
			return false;

		archive_write_add_filter_gzip(writer);
		archive_write_set_format_pax_restricted(writer);

		// Through the same wrapper the extractor uses: a case that puts the
		// archive under a non-ASCII path has to be able to create it there
		// before it can assert anything about reading it back.
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

			/* The _utf8 spellings, not the plain ones. The plain setters
			 * declare their argument to be a multi-byte string in the
			 * process's own locale, and libarchive converts from that when
			 * it writes a pax header, which stores names in UTF-8. Handing
			 * them UTF-8 bytes is therefore only correct where the locale
			 * says UTF-8 -- true on Linux and macOS, false on Windows,
			 * where the conversion runs through the active ANSI code page
			 * and turns "Ş" (C5 9E) into "Åž" without failing. The
			 * extractor then reads the name back with
			 * archive_entry_pathname_utf8() and gets the mangled form, so
			 * an archive written here did not contain what this file said
			 * it did -- on Windows only.
			 *
			 * Saying _utf8 states the encoding instead of leaving it to the
			 * host, which is also what the reader in ReleaseArchive.cpp
			 * asks for, so the two ends now agree by construction. */
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
				// Reported rather than swallowed: a bare "could not create
				// the archive" is what made the first Windows failure here
				// unreadable.
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

	/*!
	 * A path component made of letters no single-byte code page holds:
	 * Turkish, Cyrillic, CJK.
	 *
	 * Spelled as UTF-8 bytes rather than as the characters themselves
	 * because MSVC decodes a plain string literal in the build machine's
	 * ANSI code page unless it is given /utf-8, which this build does not
	 * pass. A case about mis-encoded paths must not itself depend on how
	 * the compiler guessed at its own source file.
	 */
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

	/*!
	 * A sharun bundle survives the round trip intact.
	 *
	 * The launcher and the updater exist only as hardlinks, and the library
	 * only resolves through a symlink, so this one case covers the whole
	 * failure that shipped: 763 files extracted, and an installation that
	 * could not start.
	 */
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

		// The bundle above: 7 regular files (the launch script, the loader,
		// two real binaries, one library, and the two marker files), then 3
		// hardlinks and 1 symlink.
		QCOMPARE(result.fileCount, 7);
		QCOMPARE(result.linkCount, 4);

		const QDir out(dest);

		// The entry points the launch script and the updater hand-off use.
		for (const QString& path :
			 {QStringLiteral("bin/kite"), QStringLiteral("bin/kite-updater"),
			  QStringLiteral("sharun")}) {
			const QFileInfo info(out.absoluteFilePath(path));
			QVERIFY2(info.exists(), qPrintable(path));
			QVERIFY2(info.isFile(), qPrintable(path));
			// A hardlink is indistinguishable from the file it links to, so
			// the content is the check: dropping the link and creating an
			// empty placeholder is the other way this went wrong.
			QCOMPARE(info.size(), qint64(6)); // "LOADER"
			QVERIFY2(info.isExecutable(), qPrintable(path));
		}

		// The soname link the dynamic linker actually looks for.
		const QFileInfo soname(out.absoluteFilePath("lib/libX11.so.6"));
		QVERIFY(soname.isSymLink());
		QCOMPARE(soname.symLinkTarget(),
				 out.absoluteFilePath("lib/libX11.so.6.4.0"));

		// And the real binaries behind the loader.
		QCOMPARE(QFileInfo(out.absoluteFilePath("shared/bin/kite")).size(),
				 qint64(8)); // "LAUNCHER"
#endif
	}

	/*!
	 * On Windows, an archive that contains links is refused outright.
	 *
	 * The alternative -- extracting what can be extracted and leaving the
	 * links out -- is the failure this whole file exists to prevent, just
	 * arrived at politely: a tree with the right byte count, no launcher and
	 * no updater. Since no Windows artifact contains a link, an archive that
	 * does is not one to install part of.
	 */
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
		// The wording comes from createLink() in ReleaseArchive.cpp.
		QVERIFY2(result.error.contains(QStringLiteral("not supported")),
				 qPrintable(result.error));
		QCOMPARE(result.linkCount, 0);

		// Nothing may be left standing in for a link. An empty placeholder
		// where bin/kite belongs is an installation that does not start.
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

	/*!
	 * The archive is downloaded, so it is not trusted. Each of these must
	 * abort the whole extraction rather than be sanitised and let through:
	 * an archive that tries to escape its directory is not one to install
	 * "most of".
	 */
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

		// Nothing may have landed outside the destination.
		QVERIFY(!QFileInfo::exists(work.filePath(QStringLiteral("escaped.txt"))));
	}

	/*!
	 * A hardlink to something the archive has not stored yet is refused.
	 *
	 * tar always writes the target first, so this only happens in a damaged
	 * or hand-made archive -- and a launcher that is a dangling link is not a
	 * launcher.
	 */
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

	//! The executable bit survives; a launcher that is not executable is not
	//! an installation.
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

	/*!
	 * A release with no links in it, which is what the Windows and macOS
	 * artifacts are, extracts on every platform.
	 *
	 * Shaped like a Windows package -- executables and libraries at the top
	 * level, plugins and jars in subdirectories -- because that is the tree
	 * an update actually installs there, and because everything else in this
	 * file that touches the normal path stops at the first link and so never
	 * runs on Windows at all.
	 */
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
			// Paths are reported relative to the destination, with '/'
			// separators on every platform, and are what the update log
			// shows when a release turns out to be missing something.
			QVERIFY2(result.paths.contains(entry.path),
					 qPrintable(entry.path));
			expectedBytes += entry.contents.size();
		}
		QCOMPARE(result.byteCount, expectedBytes);

		// The two the update itself goes looking for by name afterwards.
		QVERIFY(QFileInfo::exists(
			out.absoluteFilePath(QStringLiteral("kite.exe"))));
		QVERIFY(QFileInfo::exists(
			out.absoluteFilePath(QStringLiteral("kite-updater.exe"))));
	}

	/*!
	 * A release is unpacked from, and into, a path that is not ASCII.
	 *
	 * This is the one case in the file that is about the file name rather
	 * than the contents. archive_read_open_filename() and its write
	 * counterpart take a `char*`, and on Windows libarchive hands that to
	 * the narrow CRT entry points, which decode it in the active ANSI code
	 * page -- so passing UTF-8 bytes, as this code used to, is the one
	 * encoding guaranteed to be wrong there. Every archive Kite opens
	 * lives under the user's data directory, which lives under the user
	 * profile, so for a user named Şafak the answer was "No such file or
	 * directory" for every zip they ever touched, update included.
	 *
	 * Running on Linux this passes either way, since the byte path is
	 * already UTF-8 -- but it is still the case that fails if someone
	 * reaches for the narrow call again, and it is the case a Windows run
	 * has to go through. The nesting is deliberate: the awkward name is in
	 * the directory holding the archive, in the archive's own file name, in
	 * an entry inside it, and in the destination, because those are four
	 * different code paths and only the first two are libarchive's.
	 */
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

		// Failing here is already the bug, from the writing end.
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

	/*!
	 * Extract a real published release and account for every entry in it.
	 *
	 * Opt-in, because it needs a 100-plus-megabyte archive that has no
	 * business in the repository:
	 *
	 *     KITE_TEST_RELEASE_ARCHIVE=/path/to/Kite-Linux-Qt6-Portable-v10.0.0.tar.gz \
	 *         ./ReleaseArchive_test
	 *
	 * The archive is read a second time to build the expectation, rather than
	 * hardcoding counts, so this stays true for any release: every entry the
	 * archive declares must exist on disk afterwards, as the same kind of
	 * thing. That is exactly the property that failed in the field -- 763
	 * files arrived and 97 links did not.
	 */
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

		// Walk the archive again for the expectation.
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

			// symlinkTarget-aware: a QFileInfo on a symlink answers about its
			// target, so ask about the link itself first.
			QVERIFY2(info.isSymLink() || info.exists(), qPrintable(path));

			if (isSymlink) {
				++links;
				QVERIFY2(info.isSymLink(), qPrintable(path));
			} else if (isHardlink) {
				++links;
				QVERIFY2(info.isFile(), qPrintable(path));
				// The link must carry the target's content, not be an empty
				// placeholder -- which is what a naive extractor produces.
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

		// The two entry points the update depends on: the launch script runs
		// bin/kite, and the hand-off runs bin/kite-updater. Both are
		// hardlinks to the sharun loader in a real bundle.
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

	//! A release whose files sit at the top level is returned unchanged --
	//! which is what our own tarballs look like.
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
