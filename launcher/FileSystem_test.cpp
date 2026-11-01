/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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
#include <QTemporaryDir>
#include <QStandardPaths>
#include "TestUtil.h"

#include "FileSystem.h"

class FileSystemTest : public QObject
{
	Q_OBJECT

	const QString bothSlash = "/foo/";
	const QString trailingSlash = "foo/";
	const QString leadingSlash = "/foo";

  private slots:
	void test_pathCombine()
	{
		QCOMPARE(QString("/foo/foo"), FS::PathCombine(bothSlash, bothSlash));
		QCOMPARE(QString("foo/foo"),
				 FS::PathCombine(trailingSlash, trailingSlash));
		QCOMPARE(QString("/foo/foo"),
				 FS::PathCombine(leadingSlash, leadingSlash));

		QCOMPARE(QString("/foo/foo/foo"),
				 FS::PathCombine(bothSlash, bothSlash, bothSlash));
		QCOMPARE(QString("foo/foo/foo"),
				 FS::PathCombine(trailingSlash, trailingSlash, trailingSlash));
		QCOMPARE(QString("/foo/foo/foo"),
				 FS::PathCombine(leadingSlash, leadingSlash, leadingSlash));
	}

	void test_PathCombine1_data()
	{
		QTest::addColumn<QString>("result");
		QTest::addColumn<QString>("path1");
		QTest::addColumn<QString>("path2");

		QTest::newRow("qt 1") << "/abc/def/ghi/jkl" << "/abc/def" << "ghi/jkl";
		QTest::newRow("qt 2") << "/abc/def/ghi/jkl" << "/abc/def/" << "ghi/jkl";
#if defined(Q_OS_WIN)
		QTest::newRow("win native, from C:") << "C:/abc" << "C:" << "abc";
		QTest::newRow("win native 1")
			<< "C:/abc/def/ghi/jkl" << "C:\\abc\\def" << "ghi\\jkl";
		QTest::newRow("win native 2")
			<< "C:/abc/def/ghi/jkl" << "C:\\abc\\def\\" << "ghi\\jkl";
#endif
	}

	void test_PathCombine1()
	{
		QFETCH(QString, result);
		QFETCH(QString, path1);
		QFETCH(QString, path2);

		QCOMPARE(FS::PathCombine(path1, path2), result);
	}

	void test_PathCombine2_data()
	{
		QTest::addColumn<QString>("result");
		QTest::addColumn<QString>("path1");
		QTest::addColumn<QString>("path2");
		QTest::addColumn<QString>("path3");

		QTest::newRow("qt 1")
			<< "/abc/def/ghi/jkl" << "/abc" << "def" << "ghi/jkl";
		QTest::newRow("qt 2")
			<< "/abc/def/ghi/jkl" << "/abc/" << "def" << "ghi/jkl";
		QTest::newRow("qt 3")
			<< "/abc/def/ghi/jkl" << "/abc" << "def/" << "ghi/jkl";
		QTest::newRow("qt 4")
			<< "/abc/def/ghi/jkl" << "/abc/" << "def/" << "ghi/jkl";
#if defined(Q_OS_WIN)
		QTest::newRow("win 1")
			<< "C:/abc/def/ghi/jkl" << "C:\\abc" << "def" << "ghi\\jkl";
		QTest::newRow("win 2")
			<< "C:/abc/def/ghi/jkl" << "C:\\abc\\" << "def" << "ghi\\jkl";
		QTest::newRow("win 3")
			<< "C:/abc/def/ghi/jkl" << "C:\\abc" << "def\\" << "ghi\\jkl";
		QTest::newRow("win 4")
			<< "C:/abc/def/ghi/jkl" << "C:\\abc\\" << "def" << "ghi\\jkl";
#endif
	}

	void test_PathCombine2()
	{
		QFETCH(QString, result);
		QFETCH(QString, path1);
		QFETCH(QString, path2);
		QFETCH(QString, path3);

		QCOMPARE(FS::PathCombine(path1, path2, path3), result);
	}

	void test_copy()
	{
		QString folder = QFINDTESTDATA("data/test_folder");
		auto f = [&folder]() {
			QTemporaryDir tempDir;
			tempDir.setAutoRemove(true);
			qDebug() << "From:" << folder << "To:" << tempDir.path();

			QDir target_dir(FS::PathCombine(tempDir.path(), "test_folder"));
			qDebug() << tempDir.path();
			qDebug() << target_dir.path();
			FS::copy c(folder, target_dir.path());
			c();

			for (auto entry : target_dir.entryList()) {
				qDebug() << entry;
			}
			QVERIFY(target_dir.entryList().contains("pack.mcmeta"));
			QVERIFY(target_dir.entryList().contains("assets"));
		};

		QVERIFY(!folder.endsWith('/'));
		f();

		folder.append('/');
		QVERIFY(folder.endsWith('/'));
		f();
	}

	void test_getDesktop()
	{
		QCOMPARE(FS::getDesktopDir(), QStandardPaths::writableLocation(
										  QStandardPaths::DesktopLocation));
	}

	void test_sha1Hex()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString path = dir.filePath("abc.txt");
		FS::write(path, "abc");
		QCOMPARE(FS::sha1Hex(path),
				 QStringLiteral("a9993e364706816aba3e25717850c26c9cd0d89d"));

		FS::write(path, "");
		QCOMPARE(FS::sha1Hex(path),
				 QStringLiteral("da39a3ee5e6b4b0d3255bfef95601890afd80709"));
	}

	void test_sha1Hex_unreadable()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		QVERIFY(FS::sha1Hex(dir.filePath("missing.txt")).isEmpty());
	}

	void test_pathFromUrlOrPath_data()
	{
		QTest::addColumn<QString>("input");
		QTest::addColumn<QString>("expected");

		QTest::newRow("file url") << "file:///tmp/a%20b.zip" << "/tmp/a b.zip";
		QTest::newRow("plain path") << "/tmp/a b.zip" << "/tmp/a b.zip";
		QTest::newRow("remote url")
			<< "https://example.com/a.zip" << "https://example.com/a.zip";
		QTest::newRow("empty") << "" << "";
	}

	void test_pathFromUrlOrPath()
	{
		QFETCH(QString, input);
		QFETCH(QString, expected);
		QCOMPARE(FS::pathFromUrlOrPath(input), expected);
	}

	void test_pathFromUrl()
	{
		QCOMPARE(FS::pathFromUrl(QUrl("file:///tmp/a%20b.zip")),
				 QStringLiteral("/tmp/a b.zip"));
		QCOMPARE(FS::pathFromUrl(QUrl("https://example.com/a.zip")),
				 QStringLiteral("https://example.com/a.zip"));
	}

// this is only valid on linux
// FIXME: implement on windows, OSX, then test.
#if defined(Q_OS_LINUX)
	void test_createShortcut_data()
	{
		QTest::addColumn<QString>("location");
		QTest::addColumn<QString>("dest");
		QTest::addColumn<QStringList>("args");
		QTest::addColumn<QString>("name");
		QTest::addColumn<QString>("iconLocation");
		QTest::addColumn<QByteArray>("result");

		QTest::newRow("unix")
			<< QDir::currentPath() << "asdfDest"
			<< (QStringList() << "arg1" << "arg2") << "asdf" << QString()
#if defined(Q_OS_LINUX)
			<< GET_TEST_FILE("data/FileSystem-test_createShortcut-unix")
#elif defined(Q_OS_WIN)
			<< QByteArray()
#endif
			;
	}

	void test_createShortcut()
	{
		QFETCH(QString, location);
		QFETCH(QString, dest);
		QFETCH(QStringList, args);
		QFETCH(QString, name);
		QFETCH(QString, iconLocation);
		QFETCH(QByteArray, result);

		const QString written = FS::createShortcut(
			FS::PathCombine(location, name), dest, args, name, iconLocation);
		QVERIFY(!written.isEmpty());
		QCOMPARE(QString::fromLocal8Bit(TestsInternal::readFile(written)),
				 QString::fromLocal8Bit(result));

	}
#endif
};

QTEST_GUILESS_MAIN(FileSystemTest)

#include "FileSystem_test.moc"
