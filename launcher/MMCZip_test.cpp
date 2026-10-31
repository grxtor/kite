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

#include <QTest>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "MMCZip.h"
#include "minecraft/mod/Mod.h"

namespace
{
	bool writeFile(const QString& path, const QByteArray& content)
	{
		QFileInfo info(path);
		if (!QDir().mkpath(info.absolutePath())) {
			return false;
		}
		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write(content) == content.size();
	}

	// CRC-32 as zip wants it, spelled out so the test does not depend on the
	// linked compression library.
	quint32 crc32Of(const QByteArray& data)
	{
		static quint32 table[256];
		static bool ready = false;
		if (!ready) {
			for (quint32 i = 0; i < 256; i++) {
				quint32 c = i;
				for (int k = 0; k < 8; k++) {
					c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
				}
				table[i] = c;
			}
			ready = true;
		}
		quint32 c = 0xFFFFFFFFu;
		for (char byte : data) {
			c = table[(c ^ static_cast<quint8>(byte)) & 0xFF] ^ (c >> 8);
		}
		return c ^ 0xFFFFFFFFu;
	}

	void appendLE16(QByteArray& out, quint16 value)
	{
		out.append(static_cast<char>(value & 0xFF));
		out.append(static_cast<char>((value >> 8) & 0xFF));
	}

	void appendLE32(QByteArray& out, quint32 value)
	{
		out.append(static_cast<char>(value & 0xFF));
		out.append(static_cast<char>((value >> 8) & 0xFF));
		out.append(static_cast<char>((value >> 16) & 0xFF));
		out.append(static_cast<char>((value >> 24) & 0xFF));
	}

	/* Writes a zip the way Info-ZIP, 7-Zip and modpack pipelines do: stored
	 * entries with the CRC and sizes already in the local header, then a
	 * central directory. compressDir cannot stand in: libarchive's writer puts
	 * the real CRC in a trailing data descriptor, which makes truncation loud. */
	bool writeStoredZip(const QString& path,
						const QList<QPair<QString, QByteArray>>& entries)
	{
		QByteArray zip;
		struct Placed {
			QByteArray name;
			quint32 crc;
			quint32 size;
			quint32 offset;
		};
		QList<Placed> placed;

		for (const auto& entry : entries) {
			Placed p;
			p.name = entry.first.toUtf8();
			p.crc = crc32Of(entry.second);
			p.size = static_cast<quint32>(entry.second.size());
			p.offset = static_cast<quint32>(zip.size());
			placed.append(p);

			zip.append("PK\x03\x04", 4);
			appendLE16(zip, 20); // version needed
			appendLE16(zip, 0);	 // flags: sizes known here, no descriptor
			appendLE16(zip, 0);	 // method: stored
			appendLE16(zip, 0);	 // time
			appendLE16(zip, 0);	 // date
			appendLE32(zip, p.crc);
			appendLE32(zip, p.size);
			appendLE32(zip, p.size);
			appendLE16(zip, static_cast<quint16>(p.name.size()));
			appendLE16(zip, 0); // extra length
			zip.append(p.name);
			zip.append(entry.second);
		}

		const quint32 centralOffset = static_cast<quint32>(zip.size());
		for (const auto& p : placed) {
			zip.append("PK\x01\x02", 4);
			appendLE16(zip, 20); // version made by
			appendLE16(zip, 20); // version needed
			appendLE16(zip, 0);	 // flags
			appendLE16(zip, 0);	 // method
			appendLE16(zip, 0);	 // time
			appendLE16(zip, 0);	 // date
			appendLE32(zip, p.crc);
			appendLE32(zip, p.size);
			appendLE32(zip, p.size);
			appendLE16(zip, static_cast<quint16>(p.name.size()));
			appendLE16(zip, 0); // extra length
			appendLE16(zip, 0); // comment length
			appendLE16(zip, 0); // disk number start
			appendLE16(zip, 0); // internal attributes
			appendLE32(zip, 0); // external attributes
			appendLE32(zip, p.offset);
			zip.append(p.name);
		}
		const quint32 centralSize =
			static_cast<quint32>(zip.size()) - centralOffset;

		zip.append("PK\x05\x06", 4);
		appendLE16(zip, 0); // disk number
		appendLE16(zip, 0); // disk with central directory
		appendLE16(zip, static_cast<quint16>(placed.size()));
		appendLE16(zip, static_cast<quint16>(placed.size()));
		appendLE32(zip, centralSize);
		appendLE32(zip, centralOffset);
		appendLE16(zip, 0); // comment length

		QFile file(path);
		if (!file.open(QIODevice::WriteOnly)) {
			return false;
		}
		return file.write(zip) == zip.size();
	}

	bool truncateFile(const QString& path, qint64 size)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadWrite)) {
			return false;
		}
		return file.resize(size);
	}

	qint64 localHeaderOffset(const QString& path, int nth)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return -1;
		}
		const QByteArray content = file.readAll();
		const QByteArray signature("PK\x03\x04", 4);
		int from = 0;
		for (int found = 0; found < nth; found++) {
			const int at = content.indexOf(signature, from);
			if (at < 0) {
				return -1;
			}
			if (found + 1 == nth) {
				return at;
			}
			from = at + 1;
		}
		return -1;
	}

	int countFilesUnder(const QString& dir)
	{
		int count = 0;
		QDirIterator it(dir, QDir::Files, QDirIterator::Subdirectories);
		while (it.hasNext()) {
			it.next();
			count++;
		}
		return count;
	}

	int countEntries(const QStringList& entries, const QString& name)
	{
		int count = 0;
		for (const auto& entry : entries) {
			if (entry == name) {
				count++;
			}
		}
		return count;
	}

} // namespace

class MMCZipTest : public QObject
{
	Q_OBJECT
  private slots:

	void test_CreateModdedJar_ZipMod()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		auto gameDir = root.absoluteFilePath("game");
		QVERIFY(writeFile(gameDir + "/net/minecraft/Foo.class", "vanilla-foo"));
		QVERIFY(writeFile(gameDir + "/net/minecraft/Bar.class", "vanilla-bar"));
		QVERIFY(writeFile(gameDir + "/META-INF/MANIFEST.MF", "manifest"));
		auto sourceJar = root.absoluteFilePath("minecraft.jar");
		QVERIFY(MMCZip::compressDir(sourceJar, gameDir, nullptr));

		auto modDir = root.absoluteFilePath("modsrc");
		QVERIFY(writeFile(modDir + "/net/minecraft/Foo.class", "modded-foo"));
		auto modZip = root.absoluteFilePath("jarmod.zip");
		QVERIFY(MMCZip::compressDir(modZip, modDir, nullptr));

		QList<Mod> mods;
		mods.append(Mod(QFileInfo(modZip)));
		QCOMPARE(mods[0].type(), Mod::MOD_ZIPFILE);

		auto targetJar = root.absoluteFilePath("modded.jar");
		QVERIFY(MMCZip::createModdedJar(sourceJar, targetJar, mods));

		auto entries = MMCZip::listEntries(targetJar);
		QCOMPARE(countEntries(entries, "net/minecraft/Foo.class"), 1);
		QCOMPARE(countEntries(entries, "net/minecraft/Bar.class"), 1);
		for (const auto& entry : entries) {
			QVERIFY2(!entry.contains("META-INF"),
					 qPrintable("unexpected entry: " + entry));
		}
		QCOMPARE(MMCZip::readFileFromZip(targetJar, "net/minecraft/Foo.class"),
				 QByteArray("modded-foo"));
		QCOMPARE(MMCZip::readFileFromZip(targetJar, "net/minecraft/Bar.class"),
				 QByteArray("vanilla-bar"));
	}

	void test_CreateModdedJar_FolderMod()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		auto gameDir = root.absoluteFilePath("game");
		QVERIFY(writeFile(gameDir + "/net/minecraft/Foo.class", "vanilla-foo"));
		auto sourceJar = root.absoluteFilePath("minecraft.jar");
		QVERIFY(MMCZip::compressDir(sourceJar, gameDir, nullptr));

		auto modFolder = root.absoluteFilePath("jarmods/mymod");
		QVERIFY(writeFile(modFolder + "/net/minecraft/Foo.class",
						  "folder-modded-foo"));
		QVERIFY(writeFile(modFolder + "/mymod.txt", "hello"));

		QList<Mod> mods;
		mods.append(Mod(QFileInfo(modFolder)));
		QCOMPARE(mods[0].type(), Mod::MOD_FOLDER);

		auto targetJar = root.absoluteFilePath("modded.jar");
		QVERIFY(MMCZip::createModdedJar(sourceJar, targetJar, mods));

		auto entries = MMCZip::listEntries(targetJar);
		QCOMPARE(countEntries(entries, "net/minecraft/Foo.class"), 1);
		QCOMPARE(countEntries(entries, "mymod.txt"), 1);
		QVERIFY2(!entries.contains("mymod/net/minecraft/Foo.class"),
				 "folder jar mods must not be prefixed with the folder name");
		QCOMPARE(MMCZip::readFileFromZip(targetJar, "net/minecraft/Foo.class"),
				 QByteArray("folder-modded-foo"));
	}

	void test_CreateModdedJar_MissingSourceJarFails()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		auto modDir = root.absoluteFilePath("modsrc");
		QVERIFY(writeFile(modDir + "/net/minecraft/Foo.class", "modded-foo"));
		auto modZip = root.absoluteFilePath("jarmod.zip");
		QVERIFY(MMCZip::compressDir(modZip, modDir, nullptr));

		QList<Mod> mods;
		mods.append(Mod(QFileInfo(modZip)));

		auto targetJar = root.absoluteFilePath("modded.jar");
		QVERIFY(!MMCZip::createModdedJar(root.absoluteFilePath("nope.jar"),
										 targetJar, mods));
		QVERIFY(!QFile::exists(targetJar));
	}

	void test_ExtractSubDir_CompleteArchive()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		auto packDir = root.absoluteFilePath("pack");
		QVERIFY(writeFile(packDir + "/mods/a.jar", QByteArray(4000, 'a')));
		QVERIFY(writeFile(packDir + "/mods/b.jar", QByteArray(4000, 'b')));
		QVERIFY(writeFile(packDir + "/mods/c.jar", QByteArray(4000, 'c')));
		auto zip = root.absoluteFilePath("modpack.zip");
		QVERIFY(MMCZip::compressDir(zip, packDir, nullptr));

		auto target = root.absoluteFilePath("out");
		auto extracted = MMCZip::extractSubDir(zip, QString(""), target);
		QVERIFY(extracted.has_value());
		QCOMPARE(countFilesUnder(target), 3);
	}

	void test_ExtractSubDir_CompleteStoredArchive()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		auto zip = root.absoluteFilePath("modpack.zip");
		QVERIFY(writeStoredZip(zip, {{"mods/a.jar", QByteArray(4000, 'a')},
									 {"mods/b.jar", QByteArray(4000, 'b')},
									 {"mods/c.jar", QByteArray(4000, 'c')}}));

		auto target = root.absoluteFilePath("out");
		auto extracted = MMCZip::extractSubDir(zip, QString(""), target);
		QVERIFY(extracted.has_value());
		QCOMPARE(countFilesUnder(target), 3);
		QCOMPARE(MMCZip::readFileFromZip(zip, "mods/c.jar"),
				 QByteArray(4000, 'c'));
	}

	void test_ReadFilesFromZip_OnePassFromFileAndMemory()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		auto zip = root.absoluteFilePath("jar.zip");
		QVERIFY(writeStoredZip(zip, {{"a.txt", "alpha"},
									 {"dir/b.txt", "bravo"},
									 {"empty.txt", ""},
									 {"c.txt", "charlie"}}));
		const QStringList wanted{"dir/b.txt", "a.txt", "empty.txt",
								 "missing.txt"};

		const auto fromFile = MMCZip::readFilesFromZip(zip, wanted);
		QCOMPARE(fromFile.size(), 3);
		QCOMPARE(fromFile.value("a.txt"), QByteArray("alpha"));
		QCOMPARE(fromFile.value("dir/b.txt"), QByteArray("bravo"));
		QVERIFY(fromFile.contains("empty.txt"));
		QVERIFY(fromFile.value("empty.txt").isEmpty());
		QVERIFY(!fromFile.contains("missing.txt"));
		QVERIFY(!fromFile.contains("c.txt"));

		QFile file(zip);
		QVERIFY(file.open(QIODevice::ReadOnly));
		const QByteArray bytes = file.readAll();
		QCOMPARE(MMCZip::readFilesFromZipData(bytes, wanted), fromFile);

		QVERIFY(MMCZip::readFilesFromZip(root.absoluteFilePath("nope.zip"),
										 wanted)
					.isEmpty());
		QVERIFY(MMCZip::readFilesFromZipData(QByteArray(), wanted).isEmpty());
		QVERIFY(MMCZip::readFilesFromZipData(QByteArray("not a zip"), wanted)
					.isEmpty());
		QVERIFY(MMCZip::readFilesFromZip(zip, {}).isEmpty());
	}

	// A download that stopped on an entry boundary still looks like a readable
	// zip to a streaming reader, which walks local headers until the bytes run
	// out. Extraction would report success and the instance would quietly miss
	// mods.
	void test_ExtractSubDir_TruncatedOnEntryBoundaryIsRejected()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		auto zip = root.absoluteFilePath("modpack.zip");
		QVERIFY(writeStoredZip(zip, {{"mods/a.jar", QByteArray(4000, 'a')},
									 {"mods/b.jar", QByteArray(4000, 'b')},
									 {"mods/c.jar", QByteArray(4000, 'c')}}));

		const qint64 thirdEntry = localHeaderOffset(zip, 3);
		QVERIFY(thirdEntry > 0);
		QVERIFY(truncateFile(zip, thirdEntry));

		auto target = root.absoluteFilePath("out");
		auto extracted = MMCZip::extractSubDir(zip, QString(""), target);
		QVERIFY2(!extracted.has_value(),
				 "a truncated archive must not extract as if it were whole");
		QCOMPARE(countFilesUnder(target), 0);
	}

	void test_ExtractSubDir_StoredTruncatedMidEntryIsRejected()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		auto zip = root.absoluteFilePath("modpack.zip");
		QVERIFY(writeStoredZip(zip, {{"mods/a.jar", QByteArray(4000, 'a')},
									 {"mods/b.jar", QByteArray(4000, 'b')},
									 {"mods/c.jar", QByteArray(4000, 'c')}}));

		const qint64 second = localHeaderOffset(zip, 2);
		const qint64 third = localHeaderOffset(zip, 3);
		QVERIFY(second > 0);
		QVERIFY(third > second);
		QVERIFY(truncateFile(zip, second + (third - second) / 2));

		auto target = root.absoluteFilePath("out");
		auto extracted = MMCZip::extractSubDir(zip, QString(""), target);
		QVERIFY(!extracted.has_value());
		QCOMPARE(countFilesUnder(target), 0);
	}

	// Cut in the middle of an entry's compressed data, where a streaming
	// reader gets as far as a CRC mismatch. Nothing half-written may be left
	// behind for the game to load.
	void test_ExtractSubDir_TruncatedMidEntryIsRejected()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		auto packDir = root.absoluteFilePath("pack");
		QVERIFY(writeFile(packDir + "/mods/a.jar", QByteArray(4000, 'a')));
		QVERIFY(writeFile(packDir + "/mods/b.jar", QByteArray(4000, 'b')));
		QVERIFY(writeFile(packDir + "/mods/c.jar", QByteArray(4000, 'c')));
		auto zip = root.absoluteFilePath("modpack.zip");
		QVERIFY(MMCZip::compressDir(zip, packDir, nullptr));

		const qint64 second = localHeaderOffset(zip, 2);
		const qint64 third = localHeaderOffset(zip, 3);
		QVERIFY(second > 0);
		QVERIFY(third > second);
		QVERIFY(truncateFile(zip, second + (third - second) / 2));

		auto target = root.absoluteFilePath("out");
		auto extracted = MMCZip::extractSubDir(zip, QString(""), target);
		QVERIFY(!extracted.has_value());
		QCOMPARE(countFilesUnder(target), 0);
	}

	/* An instance exports to and imports from a non-ASCII path (a user named
	 * Şafak failed every export). On Windows libarchive decodes a `char*` name
	 * in the ANSI code page, so UTF-8 bytes are wrong there; Linux passes
	 * either way. Entry names match the literal on every platform because
	 * MMCZip composes them to NFC. */
	void test_NonAsciiPathRoundTrip()
	{
		QTemporaryDir tempDir;
		QVERIFY(tempDir.isValid());
		QDir root(tempDir.path());

		// Turkish, Cyrillic and CJK together, so no single-byte code page can
		// hold the name. Written as UTF-8 bytes because MSVC decodes a plain
		// literal in the build machine's ANSI code page unless passed /utf-8.
		const QString awkward = QString::fromUtf8(
			"\xC5\x9E"                                          // Ş U+015E
			"afak-"                                             //
			"\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82"  // Привет
			"-"                                                 //
			"\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E");            // 日本語

		auto home = root.absoluteFilePath(awkward);
		auto packDir = home + "/pack";
		QVERIFY(writeFile(packDir + "/mods/a.jar", QByteArray(400, 'a')));
		QVERIFY(writeFile(packDir + "/" + awkward + "/note.txt", "hello"));

		const QString noteEntry = awkward + QStringLiteral("/note.txt");

		auto zip = home + "/" + awkward + ".zip";
		QVERIFY2(MMCZip::compressDir(zip, packDir, nullptr),
				 "could not write an archive to a non-ASCII path");
		QVERIFY(QFileInfo::exists(zip));

		auto entries = MMCZip::listEntries(zip);
		QCOMPARE(countEntries(entries, "mods/a.jar"), 1);
		QCOMPARE(countEntries(entries, noteEntry), 1);
		QCOMPARE(MMCZip::readFileFromZip(zip, noteEntry),
				 QByteArray("hello"));

		auto target = home + "/out-" + awkward;
		auto extracted = MMCZip::extractDir(zip, target);
		QVERIFY2(extracted.has_value(),
				 "could not extract an archive from a non-ASCII path");
		QCOMPARE(countFilesUnder(target), 2);
		QCOMPARE(QFileInfo(QDir(target).absoluteFilePath("mods/a.jar")).size(),
				 qint64(400));
		QVERIFY2(QFileInfo::exists(QDir(target).absoluteFilePath(noteEntry)),
				 qPrintable(noteEntry));
	}
};

QTEST_GUILESS_MAIN(MMCZipTest)

#include "MMCZip_test.moc"
