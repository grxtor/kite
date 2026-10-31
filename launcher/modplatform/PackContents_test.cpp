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
#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "modplatform/PackContents.h"

class PackContentsTest : public QObject
{
	Q_OBJECT
  private slots:

	void test_RoundTrip()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());

		QVERIFY(PackContents::write(root.path(),
									{"mods/sodium.jar", "config/sodium.txt"}));

		QStringList read;
		QVERIFY(PackContents::read(root.path(), read));
		QCOMPARE(read, QStringList({"config/sodium.txt", "mods/sodium.jar"}));
	}

	// Stored sorted and de-duplicated, so identical installs give identical
	// files.
	void test_WriteIsSortedAndDeduplicated()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());

		QVERIFY(PackContents::write(
			root.path(), {"mods/b.jar", "mods/a.jar", "mods/b.jar"}));

		QStringList read;
		QVERIFY(PackContents::read(root.path(), read));
		QCOMPARE(read, QStringList({"mods/a.jar", "mods/b.jar"}));
	}

	// A missing list (installed before it was recorded) must differ from "the
	// version shipped nothing": the two mean opposite things for a delete loop.
	void test_MissingListIsNotReadable()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());

		QStringList read{"stays untouched"};
		QVERIFY(!PackContents::read(root.path(), read));
	}

	void test_EmptyListIsReadable()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());

		QVERIFY(PackContents::write(root.path(), {}));

		QStringList read{"replaced"};
		QVERIFY(PackContents::read(root.path(), read));
		QVERIFY(read.isEmpty());
	}

	// A list from a future build may mean something else; guessing deletes
	// files.
	void test_UnknownFormatVersionIsRefused()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());

		writeRaw(root.path(),
				 R"({"formatVersion":2,"paths":["mods/sodium.jar"]})");

		QStringList read;
		QVERIFY(!PackContents::read(root.path(), read));
	}

	void test_MalformedListIsRefused()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());

		writeRaw(root.path(), R"({"formatVersion":1,"paths":)");

		QStringList read;
		QVERIFY(!PackContents::read(root.path(), read));
	}

	void test_ListWithoutPathsArrayIsRefused()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());

		writeRaw(root.path(), R"({"formatVersion":1})");

		QStringList read;
		QVERIFY(!PackContents::read(root.path(), read));
	}

	void test_NormalizePath()
	{
		QCOMPARE(PackContents::normalizePath("mods/sodium.jar"),
				 QStringLiteral("mods/sodium.jar"));
		QCOMPARE(PackContents::normalizePath("mods\\sodium.jar"),
				 QStringLiteral("mods/sodium.jar"));
		QCOMPARE(PackContents::normalizePath("./mods//sodium.jar"),
				 QStringLiteral("mods/sodium.jar"));
		QCOMPARE(PackContents::normalizePath("config/../mods/sodium.jar"),
				 QStringLiteral("mods/sodium.jar"));

		QVERIFY(PackContents::normalizePath("").isEmpty());
		QVERIFY(PackContents::normalizePath(".").isEmpty());
		QVERIFY(PackContents::normalizePath("../outside.jar").isEmpty());
		QVERIFY(
			PackContents::normalizePath("mods/../../outside.jar").isEmpty());
		QVERIFY(PackContents::normalizePath("/etc/passwd").isEmpty());
	}

	void test_RefusedPathsNeverReachTheList()
	{
		QTemporaryDir root;
		QVERIFY(root.isValid());

		QVERIFY(PackContents::write(
			root.path(), {"mods/ok.jar", "../escape.jar", "/etc/passwd"}));

		QStringList read;
		QVERIFY(PackContents::read(root.path(), read));
		QCOMPARE(read, QStringList({"mods/ok.jar"}));
	}

	void test_StaleEntriesFindsDroppedFiles()
	{
		const QStringList stale = PackContents::staleEntries(
			{"mods/kept.jar", "mods/dropped.jar"}, {"mods/kept.jar"});

		QVERIFY(stale.contains("mods/dropped.jar"));
		QVERIFY(!stale.contains("mods/kept.jar"));
	}

	// A file the user put there is in no list of ours, so it can never be
	// removed.
	void test_StaleEntriesIgnoresUnrecordedFiles()
	{
		const QStringList stale =
			PackContents::staleEntries({"mods/pack.jar"}, {});

		QCOMPARE(stale.count("mods/user-added.jar"), 0);
		QVERIFY(stale.contains("mods/pack.jar"));
	}

	// A mod the user turned off is still the pack's file, recorded under its
	// install-time name.
	void test_StaleEntriesFollowsDisabledRename()
	{
		const QStringList stale =
			PackContents::staleEntries({"mods/dropped.jar"}, {});

		QVERIFY(stale.contains("mods/dropped.jar"));
		QVERIFY(stale.contains("mods/dropped.jar.disabled"));
	}

	void test_StaleEntriesFollowsEnabledRename()
	{
		const QStringList stale =
			PackContents::staleEntries({"mods/dropped.jar.disabled"}, {});

		QVERIFY(stale.contains("mods/dropped.jar.disabled"));
		QVERIFY(stale.contains("mods/dropped.jar"));
	}

	// A required-to-optional update installs ".disabled"; deleting the file
	// just written would be worse than keeping a stale one.
	void test_StaleEntriesNeverTouchesWhatTheUpdateShips()
	{
		const QStringList stale = PackContents::staleEntries(
			{"mods/flipped.jar"}, {"mods/flipped.jar.disabled"});

		QVERIFY(!stale.contains("mods/flipped.jar.disabled"));
		QVERIFY(stale.contains("mods/flipped.jar"));
	}

	void test_StaleEntriesNeverTouchesWhatTheUpdateShipsEnabled()
	{
		const QStringList stale = PackContents::staleEntries(
			{"mods/flipped.jar.disabled"}, {"mods/flipped.jar"});

		QVERIFY(!stale.contains("mods/flipped.jar"));
		QVERIFY(stale.contains("mods/flipped.jar.disabled"));
	}

	// Stripping ".disabled" off a bare suffix would name the containing
	// folder.
	void test_StaleEntriesNeverNamesAFolder()
	{
		const QStringList stale =
			PackContents::staleEntries({"mods/.disabled"}, {});

		QVERIFY(stale.contains("mods/.disabled"));
		QVERIFY(!stale.contains("mods"));
		QVERIFY(!stale.contains("mods/"));
	}

	// Identical lists (a re-install) delete nothing.
	void test_StaleEntriesOfIdenticalListsIsEmpty()
	{
		const QStringList files{"mods/a.jar", "config/a.txt"};
		QVERIFY(PackContents::staleEntries(files, files).isEmpty());
	}

	// Written on Windows, diffed on Linux: the same file must not differ.
	void test_StaleEntriesComparesNormalizedPaths()
	{
		QVERIFY(PackContents::staleEntries({"mods\\a.jar"}, {"mods/a.jar"})
					.isEmpty());
	}

  private:
	static void writeRaw(const QString& instanceRoot, const char* contents)
	{
		QFile file(PackContents::listPath(instanceRoot));
		QVERIFY(file.open(QIODevice::WriteOnly));
		QVERIFY(file.write(contents) > 0);
		file.close();
	}
};

QTEST_GUILESS_MAIN(PackContentsTest)

#include "PackContents_test.moc"
