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

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "GZip.h"
#include "MMCZip.h"
#include "models/WorldTools.h"

namespace
{
// A level.dat as gzip'd NBT: Data { LevelName, RandomSeed }. Just enough for
// World to call the folder a world.
QByteArray levelDat(const QByteArray& name, qint64 seed)
{
	QByteArray out;
	auto u16 = [&out](quint16 v) {
		out.append(char(v >> 8));
		out.append(char(v & 0xFF));
	};
	auto header = [&](char type, const QByteArray& tag) {
		out.append(type);
		u16(quint16(tag.size()));
		out.append(tag);
	};
	header(10, "");
	header(10, "Data");
	header(8, "LevelName");
	u16(quint16(name.size()));
	out.append(name);
	header(4, "RandomSeed");
	for (int shift = 56; shift >= 0; shift -= 8) {
		out.append(char((seed >> shift) & 0xFF));
	}
	out.append(char(0)); // end Data
	out.append(char(0)); // end root
	QByteArray packed;
	GZip::zip(out, packed);
	return packed;
}

bool makeWorldZip(const QString& zipPath, const QString& folder, const QByteArray& name)
{
	MMCZip::ZipWriter zip(zipPath);
	return zip.open() &&
		   zip.addFile(folder + "/level.dat", levelDat(name, 42)) &&
		   zip.addFile(folder + "/region/r.0.0.mca", QByteArray("region")) &&
		   zip.close();
}
} // namespace

class WorldToolsTest : public QObject
{
	Q_OBJECT
  private slots:

	void noProgramForAnEmptyOrMissingPath()
	{
		QVERIFY(WorldTools::mceditProgram("").isEmpty());
		QVERIFY(WorldTools::mceditProgram("   ").isEmpty());
		QVERIFY(WorldTools::mceditProgram("/nonexistent/mcedit").isEmpty());
	}

	void findsTheLaunchScriptForThePlatform()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
#if defined(Q_OS_MACOS)
		QCOMPARE(WorldTools::mceditProgram(dir.path()), dir.path());
#elif defined(Q_OS_WIN32)
		QVERIFY(WorldTools::mceditProgram(dir.path()).isEmpty());
		QFile exe(dir.filePath("mcedit.exe"));
		QVERIFY(exe.open(QIODevice::WriteOnly));
		exe.close();
		QCOMPARE(WorldTools::mceditProgram(dir.path()), dir.filePath("mcedit.exe"));
#else
		QVERIFY(WorldTools::mceditProgram(dir.path()).isEmpty());
		QFile script(dir.filePath("mcedit.sh"));
		QVERIFY(script.open(QIODevice::WriteOnly));
		script.close();
		QCOMPARE(WorldTools::mceditProgram(dir.path()), dir.filePath("mcedit.sh"));
#endif
	}

	void isWorldRecognisesAWorldZipAndRejectsOthers()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString world = dir.filePath("My Save.zip");
		QVERIFY(makeWorldZip(world, "My Save", "My Save"));
		QVERIFY(WorldTools::isWorld(world));

		const QString notWorld = dir.filePath("notes.zip");
		MMCZip::ZipWriter zip(notWorld);
		QVERIFY(zip.open());
		QVERIFY(zip.addFile("readme.txt", QByteArray("hi")));
		QVERIFY(zip.close());
		QVERIFY(!WorldTools::isWorld(notWorld));
		QVERIFY(!WorldTools::isWorld(dir.filePath("missing.zip")));
	}

	void installWorldUnpacksTheZipIntoTheWorldsFolder()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString zipPath = dir.filePath("save.zip");
		QVERIFY(makeWorldZip(zipPath, "Castle", "Castle"));
		const QString saves = dir.filePath("saves");
		QVERIFY(QDir().mkpath(saves));

		QVERIFY(WorldTools::installWorld(zipPath, saves));

		const QDir installed(saves + "/Castle");
		QVERIFY(installed.exists());
		QVERIFY(installed.exists("level.dat"));
		QVERIFY(installed.exists("region/r.0.0.mca"));
	}

	void installWorldRefusesSomethingThatIsNotAWorld()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
		const QString junk = dir.filePath("junk.zip");
		MMCZip::ZipWriter zip(junk);
		QVERIFY(zip.open());
		QVERIFY(zip.addFile("a.txt", QByteArray("x")));
		QVERIFY(zip.close());
		const QString saves = dir.filePath("saves");
		QVERIFY(QDir().mkpath(saves));

		QVERIFY(!WorldTools::installWorld(junk, saves));
		QVERIFY(QDir(saves).isEmpty());
		QVERIFY(!WorldTools::installWorld(dir.filePath("gone.zip"), saves));
	}

	void openingWithoutMCEditSaysSo()
	{
		const QString message = WorldTools::openInMCEdit("/nonexistent/mcedit", "/tmp");
		QVERIFY(message.contains("MCEdit"));
	}

	void openingAMissingWorldFolderSaysSo()
	{
		QTemporaryDir dir;
		QVERIFY(dir.isValid());
#if defined(Q_OS_MACOS)
		const QString editor = dir.path();
#else
		QFile script(dir.filePath("mcedit.sh"));
		QVERIFY(script.open(QIODevice::WriteOnly));
		script.close();
		const QString editor = dir.path();
#endif
		const QString message = WorldTools::openInMCEdit(editor, "/nonexistent/world");
		QVERIFY(!message.isEmpty());
		QVERIFY(message.contains("folder"));
	}
};

QTEST_GUILESS_MAIN(WorldToolsTest)

#include "WorldTools_test.moc"
