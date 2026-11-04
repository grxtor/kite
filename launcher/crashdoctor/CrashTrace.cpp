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

#include "CrashTrace.h"

#include <QRegularExpression>

#include <algorithm>

namespace CrashDoctor::Trace
{
	namespace
	{
		using Re = QRegularExpression;

		// Reading stops collecting frames and message lines here; only the
		// first few are sent.
		constexpr int kMaxReadFrames = 32;
		constexpr int kMaxReadMessage = 8;

		const Re kClassStart(QStringLiteral(R"(^(?:[A-Za-z_$][\w$]*\.)+[A-Za-z_$][\w$]*(?::(?: .*)?)?$)"));
		const Re kThreadPrefix(QStringLiteral(R"(^Exception in thread "[^"]*" )"));
		const Re kFrame(QStringLiteral(R"(^\s+at \S)"));
		const Re kMore(QStringLiteral(R"(^\s+\.\.\. \d+ (?:more|common frames omitted))"));
		const Re kSuppressed(QStringLiteral(R"(^\s+Suppressed: )"));
		const QString kCausedBy = QStringLiteral("Caused by: ");
		// A trace the game printed to stderr arrives as log lines:
		// "[20:16:56] [Render thread/INFO]: [STDERR]: \tat a.B.c(B.java:1)".
		const Re kStdPrefix(QStringLiteral(R"(^\[[^\]\n]*\] \[[^\]\n]*\](?: \[[^\]\n]*\])?: \[(?:STDERR|STDOUT)\]: )"));

		bool hasFrames(const Chain& chain)
		{
			return std::any_of(chain.begin(), chain.end(),
							   [](const Throwable& one) { return !one.frames.isEmpty(); });
		}

		/// The header a line starts, without "Exception in thread "x" ", or "".
		QString headerOf(const QString& line)
		{
			QString text = line;
			text.remove(kThreadPrefix);
			return kClassStart.match(text).hasMatch() ? text : QString();
		}

	} // namespace

QList<Chain> read(const QString& text)
	{
		QList<Chain> chains;
		Chain chain;
		bool skipping = false;
		const auto close = [&] {
			if (hasFrames(chain)) {
				chains.append(chain);
			}
			chain.clear();
			skipping = false;
		};
		for (QString line : text.split(QLatin1Char('\n'))) {
			line.remove(kStdPrefix);
			if (kFrame.match(line).hasMatch()) {
				if (!chain.isEmpty() && !skipping && chain.last().frames.size() < kMaxReadFrames) {
					chain.last().frames.append(line);
				}
				continue;
			}
			if (kMore.match(line).hasMatch()) {
				continue;
			}
			if (!chain.isEmpty() && kSuppressed.match(line).hasMatch()) {
				skipping = true;
				continue;
			}
			// A suppressed exception's own causes are indented.
			if (skipping && line.startsWith(QLatin1Char('\t'))) {
				continue;
			}
			if (!chain.isEmpty() && line.startsWith(kCausedBy)) {
				const QString header = headerOf(line.mid(kCausedBy.size()));
				if (!header.isEmpty()) {
					chain.append({header, {}, {}});
					skipping = false;
					continue;
				}
			}
			if (const QString header = headerOf(line); !header.isEmpty()) {
				close();
				chain.append({header, {}, {}});
				continue;
			}
			if (!chain.isEmpty() && !skipping && chain.last().frames.isEmpty() &&
				chain.last().message.size() < kMaxReadMessage) {
				if (!line.trimmed().isEmpty()) {
					chain.last().message.append(line);
				}
				continue;
			}
			close();
		}
		close();
		return chains;
	}
} // namespace CrashDoctor::Trace
