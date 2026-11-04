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

#include "ProcessOutput.h"

#include <QStringDecoder>

namespace ProcessOutput
{
	LineDecoder::LineDecoder(QStringConverter::Encoding fallback) : m_fallback(fallback) {}

	QStringList LineDecoder::feed(const QByteArray& data)
	{
		m_rest += data;
		QStringList lines;
		qsizetype start = 0;
		for (qsizetype end = m_rest.indexOf('\n'); end >= 0; end = m_rest.indexOf('\n', start)) {
			lines.append(decode(m_rest.mid(start, end - start)));
			start = end + 1;
		}
		m_rest.remove(0, start);
		return lines;
	}

	QString LineDecoder::finish()
	{
		const QString last = decode(m_rest);
		m_rest.clear();
		return last;
	}

	QString LineDecoder::decode(QByteArray line) const
	{
		// Neither UTF-8 nor a code page has 0x0D inside a character.
		line.replace('\r', QByteArray());
		// Stateless: a sequence cut off at the end is an error, not kept for later.
		QStringDecoder utf8(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
		const QString text = utf8.decode(line);
		if (!utf8.hasError()) {
			return text;
		}
		QStringDecoder fallback(m_fallback);
		return fallback.decode(line);
	}

	QStringList jvmArguments(int javaMajor)
	{
		const QStringList modern{QStringLiteral("-Dstdout.encoding=UTF-8"),
								 QStringLiteral("-Dstderr.encoding=UTF-8")};
		const QStringList legacy{QStringLiteral("-Dsun.stdout.encoding=UTF-8"),
								 QStringLiteral("-Dsun.stderr.encoding=UTF-8")};
		if (javaMajor <= 0) {
			return modern + legacy;
		}
		return javaMajor >= kStdoutEncodingSince ? modern : legacy;
	}
} // namespace ProcessOutput
