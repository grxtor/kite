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

#include "CrashSignature.h"

#include "CrashTrace.h"

#include <QCryptographicHash>
#include <QRegularExpression>

#include <algorithm>

namespace CrashDoctor
{
	namespace
	{
		using Re = QRegularExpression;

		QString hintedMod(const QString& className, const QList<ClassHint>& hints)
		{
			const ClassHint* best = nullptr;
			for (const ClassHint& hint : hints) {
				const bool covers = className == hint.prefix ||
									className.startsWith(hint.prefix + QLatin1Char('.'));
				if (covers && (!best || hint.prefix.size() > best->prefix.size())) {
					best = &hint;
				}
			}
			return best ? best->mod : QString();
		}

		/// The class of a scrubbed frame, "at a.B.c(B.java)" -> "a.B".
		QString frameClass(const QString& frame)
		{
			const QString call = frame.mid(3).section(QLatin1Char('('), 0, 0);
			return call.section(QLatin1Char('.'), 0, -2);
		}

		ReportLines fromChain(const Trace::Chain& chain, const Personal& personal,
							  const QList<ClassHint>& hints, const QSet<QString>& installed,
							  QString culprit)
		{
			ReportLines out;
			const auto addThrowable = [&](const Trace::Throwable& one) {
				out.lines << scrubMessage(one.header, personal);
				for (const QString& line : one.message.mid(0, kMaxMessageLines)) {
					out.lines << scrubMessage(line, personal);
				}
			};
			addThrowable(chain.first());
			if (chain.size() > 1) {
				addThrowable(chain.last());
			}
			auto withFrames = std::find_if(chain.rbegin(), chain.rend(),
										   [](const Trace::Throwable& one) { return !one.frames.isEmpty(); });
			const QStringList frames = withFrames->frames.mid(0, kMaxReportFrames);
			for (const QString& frame : frames) {
				QString module;
				const QString line = scrubFrame(frame, personal, &module);
				out.lines << line;
				const QString hinted = hintedMod(frameClass(line), hints);
				for (const QString& candidate : {module, hinted}) {
					if (culprit.isEmpty() && installed.contains(candidate)) {
						culprit = candidate;
					}
				}
			}
			out.culprit = culprit;
			return out;
		}

		/// Forge's and NeoForge's "Mod Loading has failed": its trace is the
		/// loader's own, the causes are in the report's mod sections.
		ReportLines fromModLoading(const QString& crashReport, const QString& header,
								   const Personal& personal, const QSet<QString>& installed)
		{
			static const Re kSection(QStringLiteral(R"(^-- (?:MOD|Mod loading issue for:) ([a-z0-9_.-]+) --$)"));
			static const Re kMessage(QStringLiteral(R"(^\s+((?:Failure|Exception) message: .+)$)"));
			static const QString kMissing = QStringLiteral("MISSING EXCEPTION MESSAGE");
			ReportLines out;
			int sections = 0;
			for (const QString& line : crashReport.split(QLatin1Char('\n'))) {
				if (const auto section = kSection.match(line); section.hasMatch()) {
					if (++sections > kMaxModSections) {
						break;
					}
					if (out.culprit.isEmpty() && installed.contains(section.captured(1))) {
						out.culprit = section.captured(1);
					}
				} else if (const auto message = kMessage.match(line);
						   sections > 0 && message.hasMatch() && !line.endsWith(kMissing)) {
					out.lines << scrubMessage(message.captured(1), personal);
				}
			}
			if (!out.lines.isEmpty()) {
				out.lines.prepend(scrubMessage(header, personal));
			}
			return out;
		}

		QString suspectedMod(const QString& crashReport, const QSet<QString>& installed)
		{
			static const Re kSuspected(QStringLiteral(R"(^Suspected Mods?:\s*\n\s+[^\n(]*\(([a-z0-9_.-]+)\))"),
									   Re::MultilineOption);
			const QString mod = kSuspected.match(crashReport).captured(1);
			return installed.contains(mod) ? mod : QString();
		}

		/// The JVM's own crash: what killed it, the frame it died in and the
		/// native frames above it.
		ReportLines fromJvmError(const QString& text, const Personal& personal)
		{
			static const Re kSignal(QStringLiteral(R"(^#\s+((?:SIG[A-Z]+|EXCEPTION_[A-Z_]+)) \(0x[0-9a-fA-F]+\))"),
									Re::MultilineOption);
			static const Re kProblem(QStringLiteral(R"(^# Problematic frame:\n# (.+)$)"), Re::MultilineOption);
			static const Re kFirstError(QStringLiteral(R"(^# A fatal error has been detected[^\n]*\n#\n#\s+(\S[^\n]*)$)"),
										Re::MultilineOption);
			ReportLines out;
			const auto signal = kSignal.match(text);
			const QString first = signal.hasMatch() ? signal.captured(1) : kFirstError.match(text).captured(1);
			if (first.isEmpty()) {
				return out;
			}
			out.lines << scrubMessage(first, personal);
			if (const QString frame = kProblem.match(text).captured(1); !frame.isEmpty()) {
				out.lines << scrubMessage(frame, personal);
			}
			const qsizetype native = text.indexOf(QLatin1String("\nNative frames:"));
			if (native >= 0) {
				const QStringList rest = text.mid(native + 1).split(QLatin1Char('\n')).mid(1);
				for (const QString& line : rest.mid(0, kMaxReportFrames)) {
					if (line.trimmed().isEmpty()) {
						break;
					}
					out.lines << scrubMessage(line, personal);
				}
			}
			return out;
		}

		/// The last error lines of a log without a stack trace (a JVM that
		/// would not start, a loader that refused), without their prefixes.
		ReportLines fromLogErrors(const QStringList& tail, const Personal& personal)
		{
			static const Re kError(QStringLiteral(R"((?:/(?:ERROR|FATAL)\]|^Error[: ]|^Exception in thread ))"));
			static const Re kPrefix(QStringLiteral(R"(^(?:\[[^\]]*\]\s*)+:?\s*)"));
			ReportLines out;
			for (const QString& line : tail) {
				if (!kError.match(line).hasMatch()) {
					continue;
				}
				QString text = line;
				const QString clean = scrubMessage(text.remove(kPrefix), personal);
				if (!clean.isEmpty() && (out.lines.isEmpty() || out.lines.last() != clean)) {
					out.lines << clean;
				}
			}
			out.lines = out.lines.mid(qMax<qsizetype>(0, out.lines.size() - kMaxFallbackLines));
			return out;
		}
	} // namespace

	ReportLines reportLines(const Evidence& raw, const Personal& personal,
							const QList<ClassHint>& hints, const QSet<QString>& installed)
	{
		const QString crashReport = normalize(raw.crashReport);
		if (!crashReport.isEmpty()) {
			const qsizetype described = crashReport.indexOf(QLatin1String("\nDescription:"));
			const QList<Trace::Chain> chains = Trace::read(crashReport.mid(qMax<qsizetype>(0, described)));
			if (!chains.isEmpty()) {
				if (const ReportLines loading =
						fromModLoading(crashReport, chains.first().first().header, personal, installed);
					!loading.lines.isEmpty()) {
					return loading;
				}
				return fromChain(chains.first(), personal, hints, installed,
								 suspectedMod(crashReport, installed));
			}
		}
		if (const ReportLines native = fromJvmError(normalize(raw.jvmError), personal); !native.lines.isEmpty()) {
			return native;
		}
		const QStringList all = normalize(raw.log).split(QLatin1Char('\n'));
		const QStringList tail = all.mid(qMax<qsizetype>(0, all.size() - kLogTailLines));
		const QList<Trace::Chain> chains = Trace::read(tail.join(QLatin1Char('\n')));
		if (!chains.isEmpty()) {
			return fromChain(chains.last(), personal, hints, installed, QString());
		}
		return fromLogErrors(tail, personal);
	}

	QString signatureOf(const QStringList& lines)
	{
		return QString::fromLatin1(
			QCryptographicHash::hash(lines.join(QLatin1Char('\n')).toUtf8(), QCryptographicHash::Sha256).toHex());
	}
} // namespace CrashDoctor
