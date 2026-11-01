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

#include "modplatform/flame/FlameFingerprint.h"

#include <QDebug>
#include <QFile>

namespace
{
	constexpr quint32 SEED = 1;
	constexpr quint32 MULTIPLIER = 0x5bd1e995u;
	constexpr int SHIFT = 24;

	/* Read granularity: keeps call overhead off the filtering loop without
	 * holding a resource pack in memory. */
	constexpr qint64 BLOCK_SIZE = 64 * 1024;

	/* The four bytes CurseForge drops before hashing; part of the identifier,
	 * see the header. */
	inline bool isStripped(char byte)
	{
		return byte == '\t' || byte == '\n' || byte == '\r' || byte == ' ';
	}

	qint64 countKept(const char* data, qint64 length)
	{
		qint64 kept = 0;
		for (qint64 i = 0; i < length; i++) {
			if (!isStripped(data[i])) {
				kept++;
			}
		}
		return kept;
	}

	/* MurmurHash2 fed one byte at a time. The filtered bytes are scattered,
	 * and groups of four straddle read blocks, so the group being assembled
	 * is held here. The length seeds the hash and must be known before the
	 * first byte is mixed, hence the file is read twice. */
	class Murmur2
	{
	  public:
		explicit Murmur2(quint32 keptLength) : m_hash(SEED ^ keptLength) {}

		void addFiltered(const char* data, qint64 length)
		{
			for (qint64 i = 0; i < length; i++) {
				if (isStripped(data[i])) {
					continue;
				}
				m_group[m_grouped++] = static_cast<quint8>(data[i]);
				if (m_grouped == 4) {
					mixGroup();
					m_grouped = 0;
				}
			}
		}

		quint32 finish()
		{
			/* The leftover bytes are folded in without group mixing: asymmetric in
			 * MurmurHash2 and load-bearing, a mistake gives a plausible number that
			 * matches nothing. */
			switch (m_grouped) {
				case 3:
					m_hash ^= static_cast<quint32>(m_group[2]) << 16;
					[[fallthrough]];
				case 2:
					m_hash ^= static_cast<quint32>(m_group[1]) << 8;
					[[fallthrough]];
				case 1:
					m_hash ^= static_cast<quint32>(m_group[0]);
					m_hash *= MULTIPLIER;
					break;
				default:
					break;
			}

			m_hash ^= m_hash >> 13;
			m_hash *= MULTIPLIER;
			m_hash ^= m_hash >> 15;
			return m_hash;
		}

	  private:
		void mixGroup()
		{
			/* Little-endian as in the reference, assembled by hand so the result
			 * does not depend on the host's byte order. */
			quint32 block = static_cast<quint32>(m_group[0]) |
							(static_cast<quint32>(m_group[1]) << 8) |
							(static_cast<quint32>(m_group[2]) << 16) |
							(static_cast<quint32>(m_group[3]) << 24);

			block *= MULTIPLIER;
			block ^= block >> SHIFT;
			block *= MULTIPLIER;

			m_hash *= MULTIPLIER;
			m_hash ^= block;
		}

		quint32 m_hash;
		quint8 m_group[4] = {0, 0, 0, 0};
		int m_grouped = 0;
	};
} // namespace

quint32 FlameFingerprint::ofData(const QByteArray& data)
{
	const qint64 kept = countKept(data.constData(), data.size());

	Murmur2 hash(static_cast<quint32>(kept));
	hash.addFiltered(data.constData(), data.size());
	return hash.finish();
}

std::optional<quint32> FlameFingerprint::ofFile(const QString& path)
{
	QFile file(path);
	if (!file.open(QIODevice::ReadOnly)) {
		qWarning() << "Could not open" << path
				   << "to fingerprint it:" << file.errorString();
		return std::nullopt;
	}

	QByteArray block;

	/* First pass: how many bytes survive the filter; the count seeds the
	 * hash. */
	qint64 kept = 0;
	while (!file.atEnd()) {
		block = file.read(BLOCK_SIZE);
		if (block.isEmpty()) {
			break;
		}
		kept += countKept(block.constData(), block.size());
	}
	if (file.error() != QFileDevice::NoError) {
		qWarning() << "Could not read" << path
				   << "to fingerprint it:" << file.errorString();
		return std::nullopt;
	}

	if (!file.seek(0)) {
		qWarning() << "Could not rewind" << path
				   << "to fingerprint it:" << file.errorString();
		return std::nullopt;
	}

	Murmur2 hash(static_cast<quint32>(kept));
	while (!file.atEnd()) {
		block = file.read(BLOCK_SIZE);
		if (block.isEmpty()) {
			break;
		}
		hash.addFiltered(block.constData(), block.size());
	}
	if (file.error() != QFileDevice::NoError) {
		qWarning() << "Could not read" << path
				   << "to fingerprint it:" << file.errorString();
		return std::nullopt;
	}

	return hash.finish();
}
