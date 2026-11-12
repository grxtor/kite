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

#include "SqliteReader.h"

#include <QFileInfo>
#include <QSet>
#include <QVector>
#include <QtEndian>

#include <cstring>

namespace
{
	/* The file format's fixed numbers (sqlite.org/fileformat2.html). */
	constexpr char kMagic[] = "SQLite format 3";
	constexpr int kHeaderSize = 100;
	constexpr int kPageSizeOffset = 16;
	constexpr int kReservedOffset = 20;
	constexpr int kChangeCounterOffset = 24;
	constexpr int kPageCountOffset = 28;
	constexpr int kEncodingOffset = 56;
	constexpr int kVersionValidOffset = 92;
	constexpr quint32 kEncodingUtf8 = 1;
	constexpr quint32 kMinPageSize = 512;
	constexpr quint32 kMaxPageSize = 65536;
	/* A page size of 65536 is stored as 1: two bytes cannot hold it. */
	constexpr quint32 kPageSizeStoredAsOne = 1;
	/* The smallest usable size the format allows. */
	constexpr quint32 kMinUsableSize = 480;

	constexpr quint8 kInteriorTablePage = 0x05;
	constexpr quint8 kLeafTablePage = 0x0D;
	constexpr int kLeafHeaderSize = 8;
	constexpr int kInteriorHeaderSize = 12;

	constexpr quint32 kWalMagicLittle = 0x377f0682;
	constexpr quint32 kWalMagicBig = 0x377f0683;
	constexpr int kWalHeaderSize = 32;
	constexpr int kWalFrameHeaderSize = 24;

	constexpr int kMaxVarintBytes = 9;

	quint16 be16(const QByteArray& data, int offset)
	{
		return qFromBigEndian<quint16>(data.constData() + offset);
	}
	quint32 be32(const QByteArray& data, int offset)
	{
		return qFromBigEndian<quint32>(data.constData() + offset);
	}

	/* A varint at @p offset; the bytes it took, or 0 when it runs past
	 * @p end. */
	int readVarint(const QByteArray& data, int offset, int end, qint64* value)
	{
		quint64 result = 0;
		for (int i = 0; i < kMaxVarintBytes; ++i) {
			if (offset + i >= end) {
				return 0;
			}
			const quint8 byte = static_cast<quint8>(data.at(offset + i));
			if (i == kMaxVarintBytes - 1) {
				result = (result << 8) | byte;
				*value = static_cast<qint64>(result);
				return kMaxVarintBytes;
			}
			result = (result << 7) | (byte & 0x7f);
			if ((byte & 0x80) == 0) {
				*value = static_cast<qint64>(result);
				return i + 1;
			}
		}
		return 0;
	}

	/* The log's running checksum over @p size bytes (a multiple of 8). */
	void walChecksum(bool bigEndian, const char* data, int size, quint32* s0,
					 quint32* s1)
	{
		for (int i = 0; i + 8 <= size; i += 8) {
			const quint32 x0 = bigEndian ? qFromBigEndian<quint32>(data + i)
										 : qFromLittleEndian<quint32>(data + i);
			const quint32 x1 = bigEndian
								   ? qFromBigEndian<quint32>(data + i + 4)
								   : qFromLittleEndian<quint32>(data + i + 4);
			*s0 += x0 + *s1;
			*s1 += x1 + *s0;
		}
	}

	bool validPageSize(quint32 size)
	{
		return size >= kMinPageSize && size <= kMaxPageSize &&
			   (size & (size - 1)) == 0;
	}

	/* An SQL identifier without its quotes. */
	QString unquote(QString name)
	{
		name = name.trimmed();
		if (name.size() >= 2) {
			const QChar first = name.front();
			const QChar last = name.back();
			if ((first == '"' && last == '"') || (first == '`' && last == '`') ||
				(first == '[' && last == ']') || (first == '\'' && last == '\'')) {
				return name.mid(1, name.size() - 2);
			}
		}
		return name;
	}

	/* The first token of a column definition: a quoted name or a bare word. */
	QString firstToken(const QString& definition, int* end)
	{
		const QString text = definition.trimmed();
		if (text.isEmpty()) {
			*end = 0;
			return {};
		}
		const QChar open = text.front();
		const QChar close = open == '[' ? QChar(']') : open;
		if (open == '"' || open == '`' || open == '[' || open == '\'') {
			const int closing = text.indexOf(close, 1);
			*end = closing < 0 ? text.size() : closing + 1;
			return unquote(text.left(*end));
		}
		int i = 0;
		while (i < text.size() && !text.at(i).isSpace()) {
			++i;
		}
		*end = i;
		return text.left(i);
	}
} // namespace

bool SqliteReader::fail(const QString& message)
{
	if (m_error.isEmpty()) {
		m_error = message;
	}
	return false;
}

bool SqliteReader::open(const QString& path)
{
	m_error.clear();
	m_walPages.clear();
	m_pagesVisited = 0;
	m_pageSize = 0;
	m_pageCount = 0;
	m_db.close();
	m_wal.close();
	m_db.setFileName(path);
	if (!m_db.open(QIODevice::ReadOnly)) {
		return fail(QStringLiteral("cannot open the database"));
	}
	const QString walPath = path + QStringLiteral("-wal");
	if (QFileInfo(walPath).isFile() && !readWal(walPath)) {
		/* A log that cannot be read is left out: the database file alone is
		 * the state as of the last checkpoint. */
		m_walPages.clear();
		m_wal.close();
		m_error.clear();
	}
	return readHeader();
}

bool SqliteReader::readWal(const QString& walPath)
{
	m_wal.setFileName(walPath);
	if (!m_wal.open(QIODevice::ReadOnly) || m_wal.size() > kMaxWalBytes) {
		return false;
	}
	const QByteArray header = m_wal.read(kWalHeaderSize);
	if (header.size() < kWalHeaderSize) {
		return false;
	}
	const quint32 magic = be32(header, 0);
	if (magic != kWalMagicLittle && magic != kWalMagicBig) {
		return false;
	}
	const bool bigEndian = magic == kWalMagicBig;
	const quint32 pageSize = be32(header, 8);
	if (!validPageSize(pageSize)) {
		return false;
	}
	quint32 s0 = 0;
	quint32 s1 = 0;
	walChecksum(bigEndian, header.constData(), 24, &s0, &s1);
	if (s0 != be32(header, 24) || s1 != be32(header, 28)) {
		return false;
	}
	const quint32 salt1 = be32(header, 16);
	const quint32 salt2 = be32(header, 20);

	/* A frame counts once a later commit frame closes its transaction; the
	 * pages of a transaction still being written are not seen. */
	QHash<quint32, qint64> pending;
	qint64 offset = kWalHeaderSize;
	const qint64 frameSize = kWalFrameHeaderSize + pageSize;
	while (offset + frameSize <= m_wal.size()) {
		if (!m_wal.seek(offset)) {
			break;
		}
		const QByteArray frame = m_wal.read(frameSize);
		if (frame.size() != frameSize) {
			break;
		}
		if (be32(frame, 8) != salt1 || be32(frame, 12) != salt2) {
			break;
		}
		walChecksum(bigEndian, frame.constData(), 8, &s0, &s1);
		walChecksum(bigEndian, frame.constData() + kWalFrameHeaderSize,
					static_cast<int>(pageSize), &s0, &s1);
		if (s0 != be32(frame, 16) || s1 != be32(frame, 20)) {
			break;
		}
		const quint32 pageNumber = be32(frame, 0);
		if (pageNumber == 0) {
			break;
		}
		pending.insert(pageNumber, offset + kWalFrameHeaderSize);
		const quint32 commitSize = be32(frame, 4);
		if (commitSize > 0) {
			for (auto it = pending.cbegin(); it != pending.cend(); ++it) {
				m_walPages.insert(it.key(), it.value());
			}
			pending.clear();
			m_pageCount = commitSize;
		}
		offset += frameSize;
	}
	if (m_walPages.isEmpty()) {
		return false;
	}
	m_pageSize = pageSize;
	return true;
}

bool SqliteReader::readHeader()
{
	QByteArray header;
	if (m_walPages.contains(1)) {
		header = page(1).left(kHeaderSize);
	} else {
		m_db.seek(0);
		header = m_db.read(kHeaderSize);
	}
	if (header.size() < kHeaderSize ||
		std::memcmp(header.constData(), kMagic, sizeof(kMagic)) != 0) {
		return fail(QStringLiteral("not an SQLite 3 database"));
	}
	quint32 pageSize = be16(header, kPageSizeOffset);
	if (pageSize == kPageSizeStoredAsOne) {
		pageSize = kMaxPageSize;
	}
	if (!validPageSize(pageSize) || (m_pageSize != 0 && m_pageSize != pageSize)) {
		return fail(QStringLiteral("invalid page size"));
	}
	m_pageSize = pageSize;
	const quint32 reserved = static_cast<quint8>(header.at(kReservedOffset));
	if (pageSize - reserved < kMinUsableSize) {
		return fail(QStringLiteral("invalid reserved space"));
	}
	m_usableSize = pageSize - reserved;
	if (be32(header, kEncodingOffset) != kEncodingUtf8 &&
		be32(header, kEncodingOffset) != 0) {
		return fail(QStringLiteral("text is not UTF-8"));
	}
	if (m_walPages.isEmpty()) {
		const quint32 counted = be32(header, kPageCountOffset);
		const bool countValid = counted > 0 &&
			be32(header, kChangeCounterOffset) == be32(header, kVersionValidOffset);
		m_pageCount = countValid ? counted
								 : static_cast<quint32>(m_db.size() / pageSize);
	}
	return true;
}

QByteArray SqliteReader::page(quint32 number)
{
	if (number == 0 || (m_pageCount > 0 && number > m_pageCount)) {
		return {};
	}
	if (++m_pagesVisited > kMaxPagesVisited) {
		fail(QStringLiteral("too many pages"));
		return {};
	}
	QFile& file = m_walPages.contains(number) ? m_wal : m_db;
	const qint64 offset = m_walPages.contains(number)
							  ? m_walPages.value(number)
							  : static_cast<qint64>(number - 1) * m_pageSize;
	if (!file.seek(offset)) {
		return {};
	}
	const QByteArray data = file.read(m_pageSize);
	return data.size() == static_cast<int>(m_pageSize) ? data : QByteArray();
}

bool SqliteReader::readPayload(const QByteArray& pageData, int offset,
							   qint64 size, QByteArray* out)
{
	const qint64 usable = m_usableSize;
	const qint64 maxLocal = usable - 35;
	qint64 local = size;
	if (size > maxLocal) {
		const qint64 minLocal = ((usable - 12) * 32 / 255) - 23;
		const qint64 k = minLocal + ((size - minLocal) % (usable - 4));
		local = k <= maxLocal ? k : minLocal;
	}
	if (offset + local > usable) {
		return false;
	}
	out->clear();
	out->reserve(static_cast<int>(size));
	out->append(pageData.constData() + offset, static_cast<int>(local));
	if (local == size) {
		return true;
	}
	if (offset + local + 4 > usable) {
		return false;
	}
	quint32 next = be32(pageData, static_cast<int>(offset + local));
	/* Each overflow page carries usable - 4 bytes; a chain longer than the
	 * payload needs is a loop. */
	const qint64 chunk = usable - 4;
	qint64 hops = (size - local + chunk - 1) / chunk;
	while (out->size() < size) {
		if (next == 0 || hops-- <= 0) {
			return false;
		}
		const QByteArray overflow = page(next);
		if (overflow.isEmpty()) {
			return false;
		}
		const qint64 take = qMin(chunk, size - out->size());
		out->append(overflow.constData() + 4, static_cast<int>(take));
		next = be32(overflow, 0);
	}
	return true;
}

bool SqliteReader::walkTable(
	quint32 root, const std::function<bool(qint64, const QByteArray&)>& cell)
{
	QSet<quint32> seen;
	QVector<quint32> stack{root};
	while (!stack.isEmpty()) {
		const quint32 number = stack.takeLast();
		if (seen.contains(number)) {
			return fail(QStringLiteral("a page is linked twice"));
		}
		seen.insert(number);
		const QByteArray data = page(number);
		if (data.isEmpty()) {
			return fail(QStringLiteral("a page cannot be read"));
		}
		const int h = number == 1 ? kHeaderSize : 0;
		const quint8 type = static_cast<quint8>(data.at(h));
		const int cells = be16(data, h + 3);
		const bool leaf = type == kLeafTablePage;
		if (!leaf && type != kInteriorTablePage) {
			return fail(QStringLiteral("not a table page"));
		}
		const int pointers = h + (leaf ? kLeafHeaderSize : kInteriorHeaderSize);
		if (pointers + cells * 2 > static_cast<int>(m_usableSize)) {
			return fail(QStringLiteral("too many cells"));
		}
		if (!leaf) {
			/* Pushed right to left so the children are visited in order. */
			stack.append(be32(data, h + 8));
			for (int i = cells - 1; i >= 0; --i) {
				const int at = be16(data, pointers + i * 2);
				if (at < pointers || at + 4 > static_cast<int>(m_usableSize)) {
					return fail(QStringLiteral("a cell lies outside its page"));
				}
				stack.append(be32(data, at));
			}
			continue;
		}
		for (int i = 0; i < cells; ++i) {
			int at = be16(data, pointers + i * 2);
			if (at < pointers || at >= static_cast<int>(m_usableSize)) {
				return fail(QStringLiteral("a cell lies outside its page"));
			}
			qint64 size = 0;
			qint64 rowid = 0;
			const int sizeBytes =
				readVarint(data, at, static_cast<int>(m_usableSize), &size);
			const int rowidBytes =
				sizeBytes ? readVarint(data, at + sizeBytes,
									   static_cast<int>(m_usableSize), &rowid)
						  : 0;
			if (!sizeBytes || !rowidBytes || size < 0) {
				return fail(QStringLiteral("a cell is damaged"));
			}
			at += sizeBytes + rowidBytes;
			/* A row past the cap is skipped, not read in part. */
			if (size > kMaxPayloadBytes) {
				continue;
			}
			QByteArray payload;
			if (!readPayload(data, at, size, &payload)) {
				return fail(QStringLiteral("a row is damaged"));
			}
			if (!cell(rowid, payload)) {
				return true;
			}
		}
	}
	return true;
}

bool SqliteReader::decodeRecord(const QByteArray& payload, QVariantList* values)
{
	qint64 headerSize = 0;
	const int end = payload.size();
	int at = readVarint(payload, 0, end, &headerSize);
	if (!at || headerSize < at || headerSize > end) {
		return false;
	}
	QVector<qint64> types;
	while (at < headerSize) {
		qint64 type = 0;
		const int used = readVarint(payload, at, static_cast<int>(headerSize), &type);
		if (!used) {
			return false;
		}
		types.append(type);
		at += used;
	}
	int body = static_cast<int>(headerSize);
	static const int intSizes[] = {0, 1, 2, 3, 4, 6, 8};
	for (const qint64 type : types) {
		if (type == 0) {
			values->append(QVariant());
		} else if (type >= 1 && type <= 6) {
			const int size = intSizes[type];
			if (body + size > end) {
				return false;
			}
			quint64 bits = 0;
			for (int i = 0; i < size; ++i) {
				bits = (bits << 8) | static_cast<quint8>(payload.at(body + i));
			}
			/* Big-endian two's complement of @p size bytes, sign-extended. */
			const int width = size * 8;
			if (width < 64 && (bits & (quint64(1) << (width - 1)))) {
				bits |= ~quint64(0) << width;
			}
			values->append(QVariant::fromValue<qlonglong>(static_cast<qint64>(bits)));
			body += size;
		} else if (type == 7) {
			if (body + 8 > end) {
				return false;
			}
			const quint64 bits = qFromBigEndian<quint64>(payload.constData() + body);
			double value = 0;
			std::memcpy(&value, &bits, sizeof(value));
			values->append(value);
			body += 8;
		} else if (type == 8 || type == 9) {
			values->append(QVariant::fromValue<qlonglong>(type == 9 ? 1 : 0));
		} else if (type >= 12) {
			const qint64 size = (type - (type % 2 == 0 ? 12 : 13)) / 2;
			if (body + size > end) {
				return false;
			}
			const QByteArray bytes = payload.mid(body, static_cast<int>(size));
			if (type % 2 == 0) {
				values->append(bytes);
			} else {
				values->append(QString::fromUtf8(bytes));
			}
			body += static_cast<int>(size);
		} else {
			return false;
		}
	}
	return true;
}

SqliteReader::TableInfo SqliteReader::parseCreateTable(const QString& sql)
{
	TableInfo info;
	const int open = sql.indexOf('(');
	if (open < 0) {
		return info;
	}
	/* The column list, split at the commas outside brackets and quotes. */
	QStringList parts;
	QString current;
	int depth = 0;
	QChar quote;
	int close = -1;
	for (int i = open + 1; i < sql.size(); ++i) {
		const QChar c = sql.at(i);
		if (!quote.isNull()) {
			if (c == quote) {
				quote = QChar();
			}
			current += c;
			continue;
		}
		if (c == '"' || c == '\'' || c == '`') {
			quote = c;
		} else if (c == '[') {
			quote = ']';
		} else if (c == '(') {
			++depth;
		} else if (c == ')') {
			if (depth == 0) {
				close = i;
				break;
			}
			--depth;
		} else if (c == ',' && depth == 0) {
			parts.append(current);
			current.clear();
			continue;
		}
		current += c;
	}
	if (close < 0) {
		return info;
	}
	parts.append(current);
	if (sql.mid(close).contains(QStringLiteral("WITHOUT ROWID"),
								Qt::CaseInsensitive)) {
		return info;
	}
	static const QStringList constraints = {
		QStringLiteral("CONSTRAINT"), QStringLiteral("PRIMARY"),
		QStringLiteral("UNIQUE"), QStringLiteral("CHECK"),
		QStringLiteral("FOREIGN")};
	for (const QString& part : parts) {
		int end = 0;
		const QString name = firstToken(part, &end);
		if (name.isEmpty()) {
			continue;
		}
		const QString trimmed = part.trimmed();
		const bool quoted = !trimmed.isEmpty() && !trimmed.front().isLetterOrNumber() &&
							trimmed.front() != '_';
		if (!quoted && constraints.contains(name.toUpper())) {
			continue;
		}
		const QString rest = trimmed.mid(end).trimmed();
		if (rest.startsWith(QStringLiteral("INTEGER"), Qt::CaseInsensitive) &&
			rest.contains(QStringLiteral("PRIMARY KEY"), Qt::CaseInsensitive)) {
			info.rowidColumn = info.columns.size();
		}
		info.columns.append(name);
	}
	return info;
}

bool SqliteReader::findTable(const QString& table, TableInfo* info)
{
	bool found = false;
	const bool ok = walkTable(1, [&](qint64, const QByteArray& payload) {
		QVariantList values;
		if (!decodeRecord(payload, &values) || values.size() < 5) {
			return true;
		}
		if (values.at(0).toString() != QLatin1String("table") ||
			values.at(1).toString().compare(table, Qt::CaseInsensitive) != 0) {
			return true;
		}
		*info = parseCreateTable(values.at(4).toString());
		info->rootPage = static_cast<quint32>(values.at(3).toLongLong());
		found = !info->columns.isEmpty() && info->rootPage > 0;
		return false;
	});
	return ok && found;
}

QStringList SqliteReader::columns(const QString& table)
{
	TableInfo info;
	return findTable(table, &info) ? info.columns : QStringList();
}

bool SqliteReader::readTable(const QString& table,
							 const std::function<bool(const QVariantMap&)>& row)
{
	TableInfo info;
	if (!findTable(table, &info)) {
		return fail(QStringLiteral("no table %1").arg(table));
	}
	bool damaged = false;
	const bool ok = walkTable(info.rootPage, [&](qint64 rowid,
												  const QByteArray& payload) {
		QVariantList values;
		if (!decodeRecord(payload, &values)) {
			damaged = true;
			return false;
		}
		QVariantMap map;
		for (int i = 0; i < info.columns.size(); ++i) {
			QVariant value = i < values.size() ? values.at(i) : QVariant();
			if (i == info.rowidColumn && !value.isValid()) {
				value = QVariant::fromValue<qlonglong>(rowid);
			}
			map.insert(info.columns.at(i), value);
		}
		return row(map);
	});
	if (damaged) {
		return fail(QStringLiteral("a row is damaged"));
	}
	return ok;
}
