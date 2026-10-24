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

/* Maven version ranges and the version ordering they use, for VersionRange. */
namespace VersionRangeDetail
{
	// ---- Maven ranges (mods.toml) ----------------------------------------

	/* The text of one range; the versions in it are not read yet. */
	struct Bounds {
		QString lower;
		QString upper;
		bool lowerInclusive = false;
		bool upperInclusive = false;
		/// "[x]": only that version, kept in `lower`.
		bool exact = false;
	};

	inline bool startsRange(const QString& spec)
	{
		return spec.startsWith(QLatin1Char('[')) ||
			   spec.startsWith(QLatin1Char('('));
	}

	//! Reads one range starting at @p pos; @p pos ends up after its bracket.
	inline std::optional<Bounds> parseOneRange(const QString& spec,
											   qsizetype& pos)
	{
		const qsizetype closeSquare = spec.indexOf(QLatin1Char(']'), pos);
		const qsizetype closeRound = spec.indexOf(QLatin1Char(')'), pos);
		if (closeSquare < 0 && closeRound < 0)
			return std::nullopt;
		// Whichever bracket closes first; either may be missing.
		qsizetype close = closeSquare;
		if (closeSquare < 0 || (closeRound >= 0 && closeRound < closeSquare))
			close = closeRound;

		Bounds bounds;
		bounds.lowerInclusive = spec.at(pos) == QLatin1Char('[');
		bounds.upperInclusive = spec.at(close) == QLatin1Char(']');
		const QStringList ends =
			spec.mid(pos + 1, close - pos - 1).split(QLatin1Char(','));
		if (ends.size() == 1) {
			bounds.exact = true;
			bounds.lower = ends.at(0).trimmed();
			if (!bounds.lowerInclusive || !bounds.upperInclusive ||
				bounds.lower.isEmpty()) {
				return std::nullopt;
			}
		} else if (ends.size() == 2) {
			bounds.lower = ends.at(0).trimmed();
			bounds.upper = ends.at(1).trimmed();
			if (bounds.lower.isEmpty() && bounds.upper.isEmpty())
				return std::nullopt;
		} else {
			return std::nullopt;
		}

		pos = close + 1;
		return bounds;
	}

	//! "[1,2),[3,)" -> two ranges; nothing unless every character belongs.
	inline std::optional<QList<Bounds>> parseRanges(const QString& spec)
	{
		QList<Bounds> ranges;
		qsizetype pos = 0;
		const auto skipSpaces = [&] {
			while (pos < spec.size() && spec.at(pos).isSpace())
				++pos;
		};
		while (pos < spec.size()) {
			if (!startsRange(spec.mid(pos)))
				return std::nullopt;
			const std::optional<Bounds> bounds = parseOneRange(spec, pos);
			if (!bounds)
				return std::nullopt;
			ranges.append(*bounds);

			skipSpaces();
			if (pos < spec.size()) {
				// Ranges are separated by a comma, and one must follow it.
				if (spec.at(pos) != QLatin1Char(','))
					return std::nullopt;
				++pos;
				skipSpaces();
				if (pos >= spec.size())
					return std::nullopt;
			}
		}
		return ranges;
	}

	/* A release sorts between its pre-releases and its builds: 1.0-rc1 <
	 * 1.0 < 1.0-1. */
	struct MavenVersion {
		enum class Tier { PreRelease, Release, Build };

		QList<qint64> core;
		Tier tier = Tier::Release;
		/// Which pre-release name, among the ones below.
		int rank = 0;
		/// The number after the pre-release name ("rc2"), or the build
		/// ("-47.2.0").
		QList<qint64> numbers;
	};

	inline int preReleaseRank(const QString& name)
	{
		static const QStringList names = {
			QStringLiteral("alpha"), QStringLiteral("beta"),
			QStringLiteral("pre"),	 QStringLiteral("rc"),
			QStringLiteral("snapshot"),
		};
		return int(names.indexOf(name.toLower())) + 1;
	}

	//! The part of a Maven version after the first "-".
	inline bool parseQualifier(const QString& qualifier, MavenVersion& version)
	{
		if (const auto build = parseNumbers(qualifier)) {
			version.tier = MavenVersion::Tier::Build;
			version.numbers = *build;
			return true;
		}

		static const QRegularExpression preRelease(
			QStringLiteral(
				"^(alpha|beta|pre|rc|snapshot)[.-]?(\\d+(\\.\\d+)*)?$"),
			QRegularExpression::CaseInsensitiveOption);
		const QRegularExpressionMatch found = preRelease.match(qualifier);
		if (!found.hasMatch())
			return false;
		version.tier = MavenVersion::Tier::PreRelease;
		version.rank = preReleaseRank(found.captured(1));
		if (const QString digits = found.captured(2); !digits.isEmpty())
			version.numbers = parseNumbers(digits).value_or(QList<qint64>());
		return true;
	}

	/* Only what can be ordered the way Maven orders it. Anything else -
	 * "+build" suffixes, "_beta", "v1", a qualifier not named above - comes
	 * back as nothing, because Maven's own treatment of those is not worth
	 * guessing at. */
	inline std::optional<MavenVersion> parseMavenVersion(const QString& text)
	{
		const QString trimmed = text.trimmed();
		const qsizetype dash = trimmed.indexOf(QLatin1Char('-'));
		const std::optional<QList<qint64>> core =
			parseNumbers(dash < 0 ? trimmed : trimmed.left(dash));
		if (!core)
			return std::nullopt;

		MavenVersion version;
		version.core = *core;
		if (dash >= 0 && !parseQualifier(trimmed.mid(dash + 1), version))
			return std::nullopt;
		return version;
	}

	inline int compareMaven(const MavenVersion& a, const MavenVersion& b)
	{
		if (const int core = compareNumbers(a.core, b.core); core != 0)
			return core;
		if (a.tier != b.tier)
			return a.tier < b.tier ? -1 : 1;
		if (a.rank != b.rank)
			return a.rank < b.rank ? -1 : 1;
		return compareNumbers(a.numbers, b.numbers);
	}

	inline Match matchBound(const std::optional<MavenVersion>& version,
							const QString& boundText, bool inclusive,
							bool isLower)
	{
		const std::optional<MavenVersion> bound = parseMavenVersion(boundText);
		if (!bound || !version)
			return Match::Unknown;
		const int order = compareMaven(*version, *bound);
		if (isLower)
			return fromBool(inclusive ? order >= 0 : order > 0);
		return fromBool(inclusive ? order <= 0 : order < 0);
	}

	inline Match matchBounds(const std::optional<MavenVersion>& version,
							 const Bounds& bounds)
	{
		if (bounds.exact) {
			const std::optional<MavenVersion> wanted =
				parseMavenVersion(bounds.lower);
			if (!wanted || !version)
				return Match::Unknown;
			return fromBool(compareMaven(*version, *wanted) == 0);
		}

		// An open end is open whichever bracket it was written with.
		Match result = Match::Yes;
		if (!bounds.lower.isEmpty()) {
			result = both(result, matchBound(version, bounds.lower,
											 bounds.lowerInclusive, true));
		}
		if (!bounds.upper.isEmpty()) {
			result = both(result, matchBound(version, bounds.upper,
											 bounds.upperInclusive, false));
		}
		return result;
	}
} // namespace VersionRangeDetail
