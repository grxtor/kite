/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QTemporaryDir>
#include <QTest>

#include "launcherimport/SqliteReader.h"

/* The fixtures are written by SQLite itself (testdata/make_modrinth_db.py), so
 * the reader is checked against the real format, not against itself. */
class SqliteReaderTest : public QObject
{
	Q_OBJECT

	QTemporaryDir m_tmp;

	static QString fixture(const QString& name)
	{
		return QFileInfo(QFINDTESTDATA("testdata/" + name)).absoluteFilePath();
	}

	static QByteArray bytesOf(const QString& path)
	{
		QFile file(path);
		return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
	}

	QString write(const QString& name, const QByteArray& bytes)
	{
		const QString path = m_tmp.filePath(name);
		QFile file(path);
		if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			file.write(bytes);
		}
		return path;
	}

	static QList<QVariantMap> rows(const QString& path, const QString& table, bool* ok = nullptr)
	{
		QList<QVariantMap> out;
		SqliteReader db;
		bool read = db.open(path) && db.readTable(table, [&](const QVariantMap& row) {
			out.append(row);
			return true;
		});
		if (ok) {
			*ok = read;
		}
		return out;
	}

	static QStringList paths(const QList<QVariantMap>& rows)
	{
		QStringList out;
		for (const QVariantMap& row : rows) {
			out << row.value("path").toString();
		}
		return out;
	}

  private slots:
	void initTestCase()
	{
		QVERIFY(m_tmp.isValid());
	}

	void columnsComeFromTheCreateStatement()
	{
		SqliteReader db;
		QVERIFY(db.open(fixture("modrinth/app.db")));
		QCOMPARE(db.columns("profiles"),
				 QStringList({"path", "install_stage", "name", "icon_path", "game_version", "mod_loader",
							  "mod_loader_version", "groups", "created", "modified", "last_played"}));
		QCOMPARE(db.columns("cache").size(), 6);
		QVERIFY(db.columns("nothing").isEmpty());
	}

	void rowsAreTyped()
	{
		bool ok = false;
		const auto profiles = rows(fixture("modrinth/app.db"), "profiles", &ok);
		QVERIFY(ok);
		QCOMPARE(paths(profiles), QStringList({"Fabric Profile", "Forge Profile", "Installing", "../escape"}));
		const QVariantMap& fabric = profiles.first();
		QCOMPARE(fabric.value("last_played").toLongLong(), 1730000000LL);
		QCOMPARE(fabric.value("game_version").toString(), QString("1.20.1"));
		QVERIFY(!fabric.value("icon_path").isValid()); // NULL
		QVERIFY(!profiles.at(1).value("last_played").isValid());
		// This row is larger than a page: its "groups" runs over overflow pages.
		const QJsonArray groups = QJsonDocument::fromJson(fabric.value("groups").toString().toUtf8()).array();
		QCOMPARE(groups.size(), 800);
		QCOMPARE(groups.last().toString(), QString("group-0799"));
	}

	void tablesSpanningPagesAreReadWhole()
	{
		bool ok = false;
		const auto cache = rows(fixture("modrinth/app.db"), "cache", &ok);
		QVERIFY(ok);
		QCOMPARE(cache.size(), 302);
		QCOMPARE(cache.last().value("id").toString(), QString("filler-0299"));
	}

	void readingStopsWhenAsked()
	{
		SqliteReader db;
		QVERIFY(db.open(fixture("modrinth/app.db")));
		int seen = 0;
		QVERIFY(db.readTable("cache", [&](const QVariantMap&) { return ++seen < 5; }));
		QCOMPARE(seen, 5);
	}

	void committedLogIsRead()
	{
		const auto profiles = rows(fixture("modrinth-wal/app.db"), "profiles");
		QVERIFY(paths(profiles).contains("WAL Profile"));
		// Without its log the file is as of the last checkpoint.
		const QString alone = write("alone.db", bytesOf(fixture("modrinth-wal/app.db")));
		QVERIFY(!paths(rows(alone, "profiles")).contains("WAL Profile"));
	}

	void tornLogIsLeftOut()
	{
		const QString db = write("torn.db", bytesOf(fixture("modrinth-wal/app.db")));
		QByteArray wal = bytesOf(fixture("modrinth-wal/app.db-wal"));
		// The last byte of the commit frame's page: its checksum no longer holds.
		wal[wal.size() - 1] = char(wal.at(wal.size() - 1) ^ 0x5a);
		write("torn.db-wal", wal);
		bool ok = false;
		const auto profiles = rows(db, "profiles", &ok);
		QVERIFY(ok);
		QVERIFY(!paths(profiles).contains("WAL Profile"));
		QCOMPARE(profiles.size(), 4);
	}

	void notADatabaseIsRefused()
	{
		SqliteReader db;
		QVERIFY(!db.open(write("garbage.db", QByteArray(8192, 'k'))));
		QVERIFY(!db.open(write("empty.db", QByteArray())));
		QVERIFY(!db.open(m_tmp.filePath("missing.db")));
	}

	void truncatedFileEndsTheReadWithAnError()
	{
		const QByteArray whole = bytesOf(fixture("modrinth/app.db"));
		SqliteReader db;
		QVERIFY(db.open(write("cut.db", whole.left(4096 * 2))));
		QVERIFY(!db.readTable("cache", [](const QVariantMap&) { return true; }));
		QVERIFY(!db.error().isEmpty());
	}

	/* Damaged files of every kind: the read may fail, but it must end, and
	 * without reading out of bounds (the sanitizer builds would catch it). */
	void damagedFilesNeverHang()
	{
		const QByteArray whole = bytesOf(fixture("modrinth/app.db"));
		QRandomGenerator random(20261104);
		for (int round = 0; round < 400; ++round) {
			QByteArray damaged = whole;
			const int flips = 1 + random.bounded(16);
			for (int i = 0; i < flips; ++i) {
				const int at = random.bounded(damaged.size());
				damaged[at] = char(random.bounded(256));
			}
			const QString path = write("fuzz.db", damaged);
			SqliteReader db;
			if (db.open(path)) {
				int seen = 0;
				db.readTable("profiles", [&](const QVariantMap&) { return ++seen < 100000; });
				db.readTable("cache", [&](const QVariantMap&) { return ++seen < 100000; });
			}
		}
	}

	void pageLinkedToItselfIsCaught()
	{
		QByteArray db = bytesOf(fixture("modrinth/app.db"));
		const int pageSize = 4096;
		/* Find the cache table's interior page and point its right-most child
		 * at itself. */
		int interior = -1;
		for (int page = 1; page < db.size() / pageSize; ++page) {
			if (quint8(db.at(page * pageSize)) == 0x05) {
				interior = page;
			}
		}
		QVERIFY(interior > 0);
		const quint32 self = quint32(interior + 1);
		db[interior * pageSize + 8] = char(self >> 24);
		db[interior * pageSize + 9] = char(self >> 16);
		db[interior * pageSize + 10] = char(self >> 8);
		db[interior * pageSize + 11] = char(self);
		SqliteReader reader;
		QVERIFY(reader.open(write("loop.db", db)));
		QVERIFY(!reader.readTable("cache", [](const QVariantMap&) { return true; }));
	}
};

QTEST_GUILESS_MAIN(SqliteReaderTest)

#include "SqliteReader_test.moc"
