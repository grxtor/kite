/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

#include <QDate>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

#include "models/CatPackList.h"
#include "ui/themes/CatPack.h"

namespace
{
	void touch(const QString& path, const QByteArray& content = "x")
	{
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile f(path);
		QVERIFY(f.open(QIODevice::WriteOnly));
		f.write(content);
	}

	QStringList ids(const QVariantList& rows)
	{
		QStringList out;
		for (const auto& row : rows)
			out << row.toMap().value("id").toString();
		return out;
	}
} // namespace

class CatPackListTest : public QObject
{
	Q_OBJECT

  private slots:
	void builtInPacksComeFirst();
	void picksTheSeasonalPicture();
	void findsLooseAndFolderPacks();
	void readsAManifestWithDatedVariants();
	void skipsABrokenManifestAndDuplicates();
	void turnsPathsIntoUrls();
	void fallsBackToTheFirstPack();
};

void CatPackListTest::builtInPacksComeFirst()
{
	QTemporaryDir empty;
	const auto rows = CatPackList::list(empty.path(), QDate(2026, 3, 3));
	QCOMPARE(ids(rows),
			 (QStringList{"kitteh", "rory", "rory-flat", "teawie"}));
	QCOMPARE(rows.first().toMap().value("source").toString(),
			 QString("qrc:/backgrounds/kitteh"));
}

void CatPackListTest::picksTheSeasonalPicture()
{
	BasicCatPack cat("kitteh", "Cat");
	QCOMPARE(cat.pathOn(QDate(2026, 3, 3)), QString(":/backgrounds/kitteh"));
	QCOMPARE(cat.pathOn(QDate(2026, 12, 24)),
			 QString(":/backgrounds/kitteh-xmas"));
	QCOMPARE(cat.pathOn(QDate(2026, 10, 30)),
			 QString(":/backgrounds/kitteh-spooky"));
	QCOMPARE(cat.pathOn(QDate(2026, 11, 4)), QString(":/backgrounds/kitteh-bday"));
}

void CatPackListTest::findsLooseAndFolderPacks()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	touch(dir.filePath("mycat.png"));
	touch(dir.filePath("notes.txt"));
	touch(dir.filePath("folder/other.png"));
	const auto rows = CatPackList::list(dir.path(), QDate(2026, 3, 3));
	const QStringList found = ids(rows);
	QVERIFY(found.contains("mycat"));
	QVERIFY(found.contains("other"));
	QVERIFY(!found.contains("notes"));
	// A user's picture is a file URL.
	for (const auto& row : rows) {
		if (row.toMap().value("id").toString() == "mycat")
			QVERIFY(row.toMap().value("source").toString().startsWith("file:///"));
	}
}

void CatPackListTest::readsAManifestWithDatedVariants()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	touch(dir.filePath("fancy/catpack.json"),
		  "{\"name\":\"Fancy\",\"default\":\"plain.png\","
		  "\"variants\":[{\"path\":\"winter.png\","
		  "\"startTime\":{\"month\":12,\"day\":20},"
		  "\"endTime\":{\"month\":1,\"day\":5}}]}");
	const auto summer = CatPackList::list(dir.path(), QDate(2026, 7, 1));
	const auto winter = CatPackList::list(dir.path(), QDate(2026, 12, 25));
	auto find = [](const QVariantList& rows) {
		for (const auto& row : rows)
			if (row.toMap().value("id").toString() == "fancy")
				return row.toMap();
		return QVariantMap();
	};
	QCOMPARE(find(summer).value("name").toString(), QString("Fancy"));
	QVERIFY(find(summer).value("source").toString().endsWith("plain.png"));
	QVERIFY(find(winter).value("source").toString().endsWith("winter.png"));
	// The range wraps the new year too.
	const auto january = CatPackList::list(dir.path(), QDate(2027, 1, 2));
	QVERIFY(find(january).value("source").toString().endsWith("winter.png"));
}

void CatPackListTest::skipsABrokenManifestAndDuplicates()
{
	QTemporaryDir dir;
	QVERIFY(dir.isValid());
	touch(dir.filePath("broken/catpack.json"), "{ not json");
	// Same id as a built-in pack: the built-in one stays.
	touch(dir.filePath("kitteh.png"));
	const auto rows = CatPackList::list(dir.path(), QDate(2026, 3, 3));
	QVERIFY(!ids(rows).contains("broken"));
	QCOMPARE(ids(rows).count("kitteh"), 1);
	QCOMPARE(rows.first().toMap().value("source").toString(),
			 QString("qrc:/backgrounds/kitteh"));
}

void CatPackListTest::turnsPathsIntoUrls()
{
	QCOMPARE(CatPackList::toUrl(":/backgrounds/rory"),
			 QString("qrc:/backgrounds/rory"));
	QCOMPARE(CatPackList::toUrl("/tmp/a b.png"),
			 QString("file:///tmp/a%20b.png"));
}

void CatPackListTest::fallsBackToTheFirstPack()
{
	CatPackList packs;
	QVERIFY(!packs.packs().isEmpty());
	const QString first = packs.packs().first().toMap().value("source").toString();
	QCOMPARE(packs.sourceFor("no-such-pack"), first);
	QCOMPARE(packs.sourceFor(""), first);
}

QTEST_GUILESS_MAIN(CatPackListTest)
#include "CatPackList_test.moc"
