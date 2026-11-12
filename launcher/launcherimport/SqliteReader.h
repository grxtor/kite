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

#pragma once

#include <QByteArray>
#include <QFile>
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include <functional>

/*
 * Reads the rows of one table out of an SQLite database file, without SQLite.
 * The Modrinth App keeps its instances in one (app.db), and Kite has no SQL
 * module, so this walks the file format (sqlite.org/fileformat2.html) for the
 * one thing needed: a rowid table's rows by column name.
 *
 * Read-only in every sense: the database and its write-ahead log are opened
 * for reading, no lock or -shm file is made, nothing is written. Changes still
 * in the log (the app is running) are read from it, up to the last complete
 * transaction. The file is untrusted: every offset is checked, every page is
 * visited at most once and the work is capped, so a damaged or hostile file
 * ends the read with an error instead of a crash or a hang.
 */
class SqliteReader
{
  public:
	/* Caps on one read. */
	static constexpr int kMaxPagesVisited = 1 << 18;	  /* 1 GiB of 4 KiB pages */
	static constexpr qint64 kMaxPayloadBytes = 16 << 20;  /* one row */
	static constexpr qint64 kMaxWalBytes = 256LL << 20;

	SqliteReader() = default;

	/* False when the file is not an SQLite 3 database Kite can read (wrong
	 * header, a page size SQLite never uses, a text encoding other than
	 * UTF-8). */
	bool open(const QString& path);

	QString error() const
	{
		return m_error;
	}

	/* The table's column names, in order, from its CREATE TABLE statement.
	 * Empty when there is no such table. */
	QStringList columns(const QString& table);

	/* Calls @p row for every row of @p table, values keyed by column name
	 * (NULL is an invalid QVariant, integers qlonglong, text QString, blobs
	 * QByteArray). @p row returns false to stop early. False when the table is
	 * missing or the file is damaged; rows already handed over stay valid. */
	bool readTable(const QString& table,
				   const std::function<bool(const QVariantMap&)>& row);

  private:
	struct TableInfo {
		quint32 rootPage = 0;
		QStringList columns;
		/* The INTEGER PRIMARY KEY column, which the record stores as NULL
		 * because it is the rowid; -1 when there is none. */
		int rowidColumn = -1;
	};

	bool fail(const QString& message);
	bool readHeader();
	bool readWal(const QString& walPath);
	QByteArray page(quint32 number);
	bool findTable(const QString& table, TableInfo* info);
	/* Walks the table b-tree from @p root, calling @p cell with each leaf
	 * cell's rowid and full payload. */
	bool walkTable(quint32 root,
				   const std::function<bool(qint64, const QByteArray&)>& cell);
	bool readPayload(const QByteArray& pageData, int offset, qint64 size,
					 QByteArray* out);
	static bool decodeRecord(const QByteArray& payload, QVariantList* values);
	static TableInfo parseCreateTable(const QString& sql);

	QFile m_db;
	QFile m_wal;
	QString m_error;
	quint32 m_pageSize = 0;
	quint32 m_usableSize = 0;
	quint32 m_pageCount = 0;
	/* Page number -> offset of its newest committed copy in the log. */
	QHash<quint32, qint64> m_walPages;
	int m_pagesVisited = 0;
};
