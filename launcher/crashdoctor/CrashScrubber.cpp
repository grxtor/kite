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

#include "CrashScrubber.h"

#include <QDir>

#include <algorithm>

namespace CrashDoctor
{
	namespace
	{
		using Re = QRegularExpression;

		const QString kHome = QStringLiteral("~");
		const QString kPathMark = QStringLiteral("<path>");
		// A file name kept after "<path>/": it names a mod or a config, not a person.
		const Re kKeptFile(QStringLiteral(R"(^[\w.+-]+\.(?:jar|json5?|toml|cfg|properties|txt|class|dll|so|dylib|jnilib|log)$)"),
						   Re::CaseInsensitiveOption);

		struct Replacement {
			Re pattern;
			QString with;
		};

		/// Before paths: a URL or an e-mail address is not a path.
		const QList<Replacement>& addresses()
		{
			static const QList<Replacement> steps{
				{Re(QStringLiteral(R"(\b[A-Za-z][A-Za-z0-9+.-]*://[^\s"'<>()]+)")), QStringLiteral("<url>")},
				{Re(QStringLiteral(R"([A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,})")),
				 QStringLiteral("<email>")},
			};
			return steps;
		}

		/// After paths and names, every line goes through these, in this order.
		const QList<Replacement>& common()
		{
			static const QList<Replacement> steps{
				{Re(QStringLiteral(R"(\b[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}\b)")),
				 QStringLiteral("<uuid>")},
				{Re(QStringLiteral(R"(\beyJ[\w-]+\.[\w-]+(?:\.[\w-]+)?)")), QStringLiteral("<token>")},
				{Re(QStringLiteral(R"((?<![\w.])\d{1,3}(?:\.\d{1,3}){3}(?::\d{1,5})?(?![\w]|\.\d))")),
				 QStringLiteral("<ip>")},
				{Re(QStringLiteral(R"(\b\d{4}-\d{2}-\d{2}(?:[T _]\d{2}[:.]\d{2}(?:[:.]\d{2})?(?:[.,]\d+)?(?:Z|[+-]\d{2}:?\d{2})?)?\b)")),
				 QStringLiteral("<time>")},
				{Re(QStringLiteral(R"(\b\d{2}[A-Z][a-z]{2}\d{4}\b)")), QStringLiteral("<time>")},
				{Re(QStringLiteral(R"(\b\d{1,2}:\d{2}(?::\d{2})?(?:[.,]\d+)?\b)")), QStringLiteral("<time>")},
				{Re(QStringLiteral(R"((?<![\w:.])(?=[0-9A-Fa-f:]*\d)(?:[0-9A-Fa-f]{0,4}:){2,7}[0-9A-Fa-f]{0,4}(?:%\w+)?(?![\w:]))")),
				 QStringLiteral("<ip>")},
				{Re(QStringLiteral(R"(\b0x[0-9a-fA-F]+\b)")), QStringLiteral("<hex>")},
				{Re(QStringLiteral(R"(@[0-9a-fA-F]{4,}\b)")), QStringLiteral("@<hex>")},
				{Re(QStringLiteral(R"(\b(?=[0-9a-fA-F]*\d)(?=[0-9a-fA-F]*[a-fA-F])[0-9a-fA-F]{12,}\b)")),
				 QStringLiteral("<hex>")},
			};
			return steps;
		}

		/// The steps only messages go through: frames have no hosts, and their
		/// numbers are handled by name.
		const QList<Replacement>& messageOnly()
		{
			static const QList<Replacement> steps{
				{Re(QStringLiteral(R"(\b(?:[A-Za-z0-9-]+\.)+[A-Za-z]{2,}:\d{2,5}\b)")), QStringLiteral("<host>")},
				{Re(QStringLiteral(R"((?<![\w.$/-])(?:[a-z0-9-]+\.)+(?:com|net|org|io|gg|me|tr|de|uk|ru|fr|nl|pl|br|eu|us|co|xyz|cc|tv|info|biz)(?![\w.-]))"),
					Re::CaseInsensitiveOption),
				 QStringLiteral("<host>")},
				{Re(QStringLiteral(R"((?<![\w.$<])\d+(?![\w.>]))")), QStringLiteral("<n>")},
			};
			return steps;
		}

		/// Generated classes and methods get a new number on every run.
		const QList<Replacement>& generatedNames()
		{
			static const QList<Replacement> steps{
				{Re(QStringLiteral(R"(\$\$Lambda(?:\$\d+)?(?:/(?:0x)?[0-9a-fA-F]+)?)")), QStringLiteral("$$Lambda")},
				{Re(QStringLiteral(R"(\blambda\$([\w$]*?)\$\d+\b)")), QStringLiteral("lambda$\\1$<n>")},
				{Re(QStringLiteral(R"(\$[a-z]{3}\d{3}\$)")), QStringLiteral("$")},
				{Re(QStringLiteral(R"(\b(Generated(?:Serialization)?(?:Method|Constructor)Accessor)\d+)")), QStringLiteral("\\1<n>")},
				{Re(QStringLiteral(R"(\$Proxy\d+)")), QStringLiteral("$Proxy<n>")},
				{Re(QStringLiteral(R"(\bASMEventHandler_\d+_)")), QStringLiteral("ASMEventHandler_<n>_")},
				{Re(QStringLiteral(R"(\$ByteBuddy\$\w+)")), QStringLiteral("$ByteBuddy")},
			};
			return steps;
		}

		QString replacedPaths(const QString& text)
		{
			static const Re kPath(QStringLiteral(
				R"((?:~[\\/][^\s"'<>|]*|[A-Za-z]:[\\/][^\s"'<>|]*|\\\\[^\s"'<>|]+|(?<![\w.$/<>~-])/(?:[^\s/"'<>|:]+/)+[^\s/"'<>|:]*))"));
			// A folder name with a space ("My World/config"): the words after it
			// that still hold a separator belong to the path.
			static const Re kMore(QStringLiteral(R"(\G [^\s"'<>|]*[\\/][^\s"'<>|]*)"));
			static const Re kSeparator(QStringLiteral(R"([\\/])"));
			QString out;
			qsizetype from = 0;
			auto it = kPath.globalMatch(text);
			while (it.hasNext()) {
				const auto match = it.next();
				if (match.capturedStart() < from) {
					continue;
				}
				out += text.mid(from, match.capturedStart() - from);
				qsizetype end = match.capturedEnd();
				for (auto more = kMore.match(text, end); more.hasMatch(); more = kMore.match(text, end)) {
					end = more.capturedEnd();
				}
				const QString file = text.mid(match.capturedStart(), end - match.capturedStart()).section(kSeparator, -1);
				out += kKeptFile.match(file).hasMatch() ? kPathMark + QLatin1Char('/') + file : kPathMark;
				from = end;
			}
			return out + text.mid(from);
		}

		QString withoutHomes(QString text, const QStringList& homes)
		{
			for (const QString& home : homes) {
				const QString clean = QDir::fromNativeSeparators(home);
				if (clean.size() < kMinPersonalLength) {
					continue;
				}
				for (const QString& form : {clean, QString(clean).replace(QLatin1Char('/'), QLatin1Char('\\')),
											QString(clean).replace(QLatin1Char('\\'), QLatin1Char('/'))}) {
					text.replace(form, kHome, Qt::CaseInsensitive);
				}
			}
			return text;
		}

		QString applied(QString text, const QList<Replacement>& steps)
		{
			for (const Replacement& step : steps) {
				text.replace(step.pattern, step.with);
			}
			return text;
		}

		/// No control or bidi characters, one space between words, and at most
		/// kMaxReportLineLength code units, never half a surrogate pair.
		QString tidy(QString text)
		{
			static const Re kInvisible(QStringLiteral(R"([\x{0}-\x{1f}\x{7f}-\x{9f}\x{200e}\x{200f}\x{202a}-\x{202e}\x{2066}-\x{2069}])"));
			static const Re kSpaces(QStringLiteral(R"(\s+)"));
			text.replace(QLatin1Char('\t'), QLatin1Char(' '));
			text.remove(kInvisible);
			text = text.replace(kSpaces, QStringLiteral(" ")).trimmed();
			if (text.size() > kMaxReportLineLength) {
				const bool split = text.at(kMaxReportLineLength - 1).isHighSurrogate();
				text.truncate(kMaxReportLineLength - (split ? 1 : 0));
			}
			return text;
		}

		QString scrubbed(const QString& text, const Personal& personal)
		{
			QString out = withoutHomes(applied(text, generatedNames()), personal.homes());
			out = replacedPaths(applied(out, addresses()));
			if (personal.names().isValid() && !personal.names().pattern().isEmpty()) {
				out.replace(personal.names(), QStringLiteral("<user>"));
			}
			return applied(out, common());
		}
	} // namespace

	Personal::Personal(const QStringList& homes, const QStringList& names) : m_homes(homes)
	{
		QStringList words;
		for (const QString& name : names) {
			const QString word = name.trimmed();
			if (word.size() >= kMinPersonalLength && !words.contains(word, Qt::CaseInsensitive)) {
				words << QRegularExpression::escape(word);
			}
		}
		// Longest first, so a name inside another does not cut it in two.
		std::sort(words.begin(), words.end(),
				  [](const QString& a, const QString& b) { return a.size() > b.size(); });
		if (!words.isEmpty()) {
			m_names = QRegularExpression(QStringLiteral(R"((?<![\w])(?:%1)(?![\w]))").arg(words.join(QLatin1Char('|'))),
										 QRegularExpression::CaseInsensitiveOption);
		}
	}

	QString scrubMessage(const QString& line, const Personal& personal)
	{
		return tidy(applied(scrubbed(line, personal), messageOnly()));
	}

	QString scrubFrame(const QString& frame, const Personal& personal, QString* module)
	{
		static const Re kAt(QStringLiteral(R"(^\s*at\s+)"));
		static const Re kLineNumber(QStringLiteral(R"(:\d+$)"));
		static const Re kModuleMod(QStringLiteral(R"(^([a-z0-9_.-]+)@)"));
		// Synthetic methods (lambda$tick$12, mixinextras$bridge$load$17, access$000).
		static const Re kSyntheticNumber(QStringLiteral(R"(\$\d+$)"));
		if (module) {
			module->clear();
		}
		QString call = frame;
		call.remove(kAt);
		const qsizetype open = call.indexOf(QLatin1Char('('));
		const qsizetype close = open < 0 ? -1 : call.indexOf(QLatin1Char(')'), open);
		QString where;
		if (close > open) {
			where = call.mid(open + 1, close - open - 1).remove(kLineNumber);
			call.truncate(open);
		} else {
			call = call.section(QLatin1Char(' '), 0, 0);
		}
		call = applied(call, generatedNames()).replace(kSyntheticNumber, QStringLiteral("$<n>"));
		// "TRANSFORMER/create@6.0/", "java.base/", "knot//": class loader and module.
		const qsizetype slash = call.lastIndexOf(QLatin1Char('/'));
		if (slash >= 0) {
			for (const QString& part : call.left(slash).split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
				const auto mod = kModuleMod.match(part);
				if (module && mod.hasMatch() && module->isEmpty()) {
					*module = mod.captured(1);
				}
			}
			call = call.mid(slash + 1);
		}
		const QString text = close > open ? QStringLiteral("at %1(%2)").arg(call, where)
										  : QStringLiteral("at %1").arg(call);
		return tidy(scrubbed(text, personal));
	}
} // namespace CrashDoctor
