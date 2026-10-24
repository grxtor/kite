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

#include <QList>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <optional>

#include "VersionRangeCommon.h"

/* Fabric's semantic versions and version predicates, for VersionRange. */
namespace VersionRangeDetail
{
	// ---- semantic versions (Fabric, Quilt) ---------------------------------

	struct SemVer {
		QList<qint64> core;
		/// Has a "-" part, which may be empty.
		bool preRelease = false;
		/// The dot-separated parts of the pre-release.
		QStringList identifiers;
	};

	inline std::optional<SemVer> parseSemVer(QString text)
	{
		text = text.trimmed();
		if (text.size() > 1 &&
			(text.startsWith(QLatin1Char('v')) ||
			 text.startsWith(QLatin1Char('V'))) &&
			text.at(1).isDigit()) {
			text.remove(0, 1);
		}
		// Build metadata never takes part in ordering.
		if (const qsizetype plus = text.indexOf(QLatin1Char('+')); plus >= 0)
			text.truncate(plus);

		SemVer version;
		QString corePart = text;
		if (const qsizetype dash = text.indexOf(QLatin1Char('-')); dash >= 0) {
			corePart = text.left(dash);
			version.preRelease = true;
			const QString pre = text.mid(dash + 1);
			if (!pre.isEmpty()) {
				version.identifiers = pre.split(QLatin1Char('.'));
				if (version.identifiers.contains(QString()))
					return std::nullopt;
			}
		}

		const std::optional<QList<qint64>> core = parseNumbers(corePart);
		if (!core)
			return std::nullopt;
		version.core = *core;
		return version;
	}

	//! Numeric identifiers by value and before the alphanumeric ones, which
	//! compare as text - SemVer's rule.
	inline int compareIdentifier(const QString& a, const QString& b)
	{
		const std::optional<qint64> left = parseNumber(a);
		const std::optional<qint64> right = parseNumber(b);
		if (left && right)
			return sign(*left - *right);
		if (left)
			return -1;
		if (right)
			return 1;
		return sign(QString::compare(a, b));
	}

	inline int compareSemVer(const SemVer& a, const SemVer& b)
	{
		if (const int core = compareNumbers(a.core, b.core); core != 0)
			return core;
		if (a.preRelease != b.preRelease)
			return a.preRelease ? -1 : 1;

		const qsizetype shared =
			std::min(a.identifiers.size(), b.identifiers.size());
		for (qsizetype i = 0; i < shared; ++i) {
			if (const int order =
					compareIdentifier(a.identifiers.at(i), b.identifiers.at(i));
				order != 0) {
				return order;
			}
		}
		// A list that is a prefix of the other sorts first; this is also what
		// puts the empty pre-release ("1.20.2-") below every other one.
		return sign(a.identifiers.size() - b.identifiers.size());
	}

	// ---- Fabric predicates -----------------------------------------------

	enum class Op {
		Exact,
		Greater,
		GreaterEqual,
		Less,
		LessEqual,
		Tilde,
		Caret
	};

	struct Predicate {
		Op op = Op::Exact;
		QString version;
	};

	inline Predicate splitPredicate(const QString& token)
	{
		// Two-character operators first, or ">=" would read as ">" and "=1".
		static const struct {
			const char* text;
			Op op;
		} operators[] = {
			{ ">=", Op::GreaterEqual }, { "<=", Op::LessEqual },
			{ ">", Op::Greater },		{ "<", Op::Less },
			{ "=", Op::Exact },			{ "~", Op::Tilde },
			{ "^", Op::Caret },
		};
		for (const auto& candidate : operators) {
			const QLatin1String text(candidate.text);
			if (token.startsWith(text))
				return { candidate.op, token.mid(text.size()) };
		}
		return { Op::Exact, token };
	}

	inline QStringList splitWords(const QString& text)
	{
		static const QRegularExpression spaces(QStringLiteral("\\s+"));
		return text.split(spaces, Qt::SkipEmptyParts);
	}

	inline bool isWildcard(const QString& part)
	{
		return part == QLatin1String("x") || part == QLatin1String("X") ||
			   part == QLatin1String("*");
	}

	inline bool containsWildcard(const QString& text)
	{
		const QStringList parts = text.split(QLatin1Char('.'));
		return std::any_of(parts.begin(), parts.end(), isWildcard);
	}

	//! "1.20.x" -> 1, 20: the numbers in front of the wildcard. Nothing when
	//! anything but wildcards follows it.
	inline std::optional<QList<qint64>> wildcardPrefix(const QString& text)
	{
		QList<qint64> prefix;
		bool wildcardSeen = false;
		for (const QString& part : text.split(QLatin1Char('.'))) {
			if (isWildcard(part)) {
				wildcardSeen = true;
				continue;
			}
			const std::optional<qint64> number = parseNumber(part);
			if (wildcardSeen || !number)
				return std::nullopt;
			prefix.append(*number);
		}
		return prefix;
	}

	/* Fabric ignores the pre-release once a wildcard is reached, so
	 * "1.20.x" takes 1.20.4-beta too. Wildcards under any other operator
	 * are left alone: nobody writes them and the loader's reading of them
	 * is not worth guessing. */
	inline Match matchWildcard(const std::optional<SemVer>& version,
							   const Predicate& predicate)
	{
		const std::optional<QList<qint64>> prefix =
			wildcardPrefix(predicate.version);
		if (predicate.op != Op::Exact || !prefix)
			return Match::Unknown;
		if (prefix->isEmpty())
			return Match::Yes;
		if (!version)
			return Match::Unknown;
		for (qsizetype i = 0; i < prefix->size(); ++i) {
			const qint64 have =
				i < version->core.size() ? version->core.at(i) : 0;
			if (have != prefix->at(i))
				return Match::No;
		}
		return Match::Yes;
	}

	/* The first version "~" or "^" no longer covers: the next minor (major)
	 * with an empty pre-release, which sorts below everything of that
	 * number - so its own betas are out as well. */
	inline SemVer upperBound(const SemVer& base, Op op)
	{
		const qsizetype position = op == Op::Tilde ? 1 : 0;
		QList<qint64> core = base.core;
		while (core.size() <= position)
			core.append(0);
		core = core.mid(0, position + 1);
		core[position] += 1;
		return { core, true, {} };
	}

	inline Match matchPredicate(const std::optional<SemVer>& version,
								const QString& token)
	{
		const Predicate predicate = splitPredicate(token);
		if (predicate.version.isEmpty())
			return Match::Unknown;
		if (containsWildcard(predicate.version))
			return matchWildcard(version, predicate);

		const std::optional<SemVer> wanted = parseSemVer(predicate.version);
		if (!wanted || !version)
			return Match::Unknown;

		const int order = compareSemVer(*version, *wanted);
		switch (predicate.op) {
		case Op::Exact:
			return fromBool(order == 0);
		case Op::Greater:
			return fromBool(order > 0);
		case Op::GreaterEqual:
			return fromBool(order >= 0);
		case Op::Less:
			return fromBool(order < 0);
		case Op::LessEqual:
			return fromBool(order <= 0);
		case Op::Tilde:
		case Op::Caret: {
			const SemVer upper = upperBound(*wanted, predicate.op);
			return both(fromBool(order >= 0),
						fromBool(compareSemVer(*version, upper) < 0));
		}
		}
		return Match::Unknown;
	}

	inline Match matchAlternative(const std::optional<SemVer>& version,
								  const QString& alternative)
	{
		Match result = Match::Yes;
		for (const QString& token : splitWords(alternative))
			result = both(result, matchPredicate(version, token));
		return result;
	}
} // namespace VersionRangeDetail
