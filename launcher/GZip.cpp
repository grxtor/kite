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

#include "GZip.h"
#include <zlib.h>
#include <QByteArray>
#include <limits>

bool GZip::unzip(const QByteArray& compressedBytes,
				 QByteArray& uncompressedBytes, qsizetype maxOutput,
				 bool* exceededCap)
{
	if (exceededCap) {
		*exceededCap = false;
	}
	if (compressedBytes.size() == 0) {
		uncompressedBytes = compressedBytes;
		return true;
	}
	if (static_cast<qint64>(compressedBytes.size()) >
		static_cast<qint64>(std::numeric_limits<uInt>::max())) {
		return false;
	}

	const bool limited = maxOutput >= 0;
	// One byte past the cap, so that exceeding it is detectable.
	const qsizetype hardCap =
		limited ? maxOutput + 1 : std::numeric_limits<qsizetype>::max();
	qsizetype capacity = qMin(compressedBytes.size(), hardCap);
	qsizetype produced = 0;
	uncompressedBytes.clear();
	uncompressedBytes.resize(capacity);

	z_stream strm;
	memset(&strm, 0, sizeof(strm));
	strm.next_in = (uint8_t*)compressedBytes.data();
	strm.avail_in = static_cast<uInt>(compressedBytes.size());

	bool done = false;

	if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
		return false;
	}

	bool ok = true;
	while (!done) {
		if (produced == capacity) {
			if (capacity >= hardCap) {
				ok = false;
				if (exceededCap) {
					*exceededCap = true;
				}
				break;
			}
			capacity = capacity > hardCap / 2 ? hardCap : capacity * 2;
			uncompressedBytes.resize(capacity);
		}

		const uInt room = static_cast<uInt>(
			qMin<qsizetype>(capacity - produced,
							std::numeric_limits<uInt>::max()));
		strm.next_out = (uint8_t*)(uncompressedBytes.data() + produced);
		strm.avail_out = room;

		const int err = inflate(&strm, Z_SYNC_FLUSH);
		produced += room - strm.avail_out;
		if (limited && produced > maxOutput) {
			ok = false;
			if (exceededCap) {
				*exceededCap = true;
			}
			break;
		}
		if (err == Z_STREAM_END)
			done = true;
		else if (err != Z_OK) {
			break;
		}
	}

	if (inflateEnd(&strm) != Z_OK || !done || !ok) {
		return false;
	}

	uncompressedBytes.resize(produced);
	return true;
}

bool GZip::zip(const QByteArray& uncompressedBytes, QByteArray& compressedBytes)
{
	if (uncompressedBytes.size() == 0) {
		compressedBytes = uncompressedBytes;
		return true;
	}

	/* std::min needs one type for both arguments, so pin qsizetype. */
	unsigned compLength = static_cast<unsigned>(
		std::min<qsizetype>(16, uncompressedBytes.size()));
	compressedBytes.clear();
	compressedBytes.resize(compLength);

	z_stream zs;
	memset(&zs, 0, sizeof(zs));

	if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, (16 + MAX_WBITS), 8,
					 Z_DEFAULT_STRATEGY) != Z_OK) {
		return false;
	}

	// See the note on next_in in unzip() above.
	zs.next_in = (uint8_t*)uncompressedBytes.data();
	zs.avail_in = uncompressedBytes.size();

	int ret;
	compressedBytes.resize(uncompressedBytes.size());

	unsigned offset = 0;
	unsigned temp = 0;
	do {
		auto remaining = compressedBytes.size() - offset;
		if (remaining < 1) {
			compressedBytes.resize(compressedBytes.size() * 2);
		}
		zs.next_out = (uint8_t*)(compressedBytes.data() + offset);
		temp = zs.avail_out = compressedBytes.size() - offset;
		ret = deflate(&zs, Z_FINISH);
		offset += temp - zs.avail_out;
	} while (ret == Z_OK);

	compressedBytes.resize(offset);

	if (deflateEnd(&zs) != Z_OK) {
		return false;
	}

	if (ret != Z_STREAM_END) {
		return false;
	}
	return true;
}