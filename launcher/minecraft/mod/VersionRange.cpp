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

#include "VersionRange.h"

#include <QCoreApplication>

#include "VersionRangeFabric.h"
#include "VersionRangeMaven.h"

using namespace VersionRangeDetail;

namespace
{
	// ---- wording ---------------------------------------------------------

	QString orNewer(const QString& version)
	{
		return QCoreApplication::translate("VersionRange", "%1 or newer")
			.arg(version);
	}

	QString newerThan(const QString& version)
	{
		return QCoreApplication::translate("VersionRange", "newer than %1")
			.arg(version);
	}

	QString olderThan(const QString& version)
	{
		return QCoreApplication::translate("VersionRange", "older than %1")
			.arg(version);
	}

	QString orOlder(const QString& version)
	{
		return QCoreApplication::translate("VersionRange", "%1 or older")
			.arg(version);
	}

	QString orNewerBelow(const QString& lower, const QString& upper)
	{
		return QCoreApplication::translate("VersionRange",
										   "%1 or newer, below %2")
			.arg(lower, upper);
	}

	/* The plain shapes both syntaxes share: one open end, or "from X up to,
	 * not including, Y". Anything else has no short wording. */
	std::optional<QString> describeInterval(const QString& lower,
											bool lowerInclusive,
											const QString& upper,
											bool upperInclusive)
	{
		if (!lower.isEmpty() && upper.isEmpty())
			return lowerInclusive ? orNewer(lower) : newerThan(lower);
		if (lower.isEmpty() && !upper.isEmpty())
			return upperInclusive ? orOlder(upper) : olderThan(upper);
		if (lowerInclusive && !upperInclusive && !lower.isEmpty())
			return orNewerBelow(lower, upper);
		return std::nullopt;
	}

	QString ownText(const QStringList& texts)
	{
		QStringList trimmed;
		for (const QString& text : texts)
			trimmed.append(text.trimmed());
		return trimmed.join(QLatin1String(" or "));
	}

	std::optional<QString> describeFabricAlternative(const QString& alternative)
	{
		const QStringList words = splitWords(alternative);
		if (words.isEmpty() || words.size() > 2)
			return std::nullopt;

		QString lower;
		QString upper;
		bool lowerInclusive = false;
		bool upperInclusive = false;
		for (const QString& word : words) {
			const Predicate predicate = splitPredicate(word);
			if (predicate.version.isEmpty())
				return std::nullopt;
			if (words.size() == 1 && predicate.op == Op::Exact)
				return predicate.version;

			// Only one of each side: ">=1 >=2" has no short wording.
			const bool isLowerSide = predicate.op == Op::Greater ||
									 predicate.op == Op::GreaterEqual;
			const bool isUpperSide =
				predicate.op == Op::Less || predicate.op == Op::LessEqual;
			if ((isLowerSide && !lower.isEmpty()) ||
				(isUpperSide && !upper.isEmpty()) ||
				(!isLowerSide && !isUpperSide)) {
				return std::nullopt;
			}
			if (isLowerSide) {
				lower = predicate.version;
				lowerInclusive = predicate.op == Op::GreaterEqual;
			} else {
				upper = predicate.version;
				upperInclusive = predicate.op == Op::LessEqual;
			}
		}
		return describeInterval(lower, lowerInclusive, upper, upperInclusive);
	}

	QString describeFabric(const QStringList& alternatives)
	{
		for (const QString& alternative : alternatives) {
			const QString trimmed = alternative.trimmed();
			if (trimmed.isEmpty() || trimmed == QLatin1String("*"))
				return QString();
		}
		if (alternatives.size() == 1) {
			if (const std::optional<QString> phrase =
					describeFabricAlternative(alternatives.first())) {
				return *phrase;
			}
		}
		return ownText(alternatives);
	}

	QString describeMaven(const QStringList& ranges)
	{
		if (ranges.size() != 1)
			return ownText(ranges);

		const QString spec = ranges.first().trimmed();
		if (spec.isEmpty() || spec == QLatin1String("*"))
			return QString();
		// A bare version is only a recommendation: any version is accepted.
		if (!startsRange(spec) && !spec.contains(QLatin1Char(',')))
			return QString();

		const std::optional<QList<Bounds>> parsed = parseRanges(spec);
		if (parsed && parsed->size() == 1) {
			const Bounds& bounds = parsed->first();
			if (bounds.exact)
				return bounds.lower;
			if (const std::optional<QString> phrase = describeInterval(
					bounds.lower, bounds.lowerInclusive, bounds.upper,
					bounds.upperInclusive)) {
				return *phrase;
			}
		}
		return spec;
	}
} // namespace

namespace VersionRange
{
	std::optional<int> compare(const QString& a, const QString& b)
	{
		const std::optional<SemVer> left = parseSemVer(a);
		const std::optional<SemVer> right = parseSemVer(b);
		if (!left || !right)
			return std::nullopt;
		return compareSemVer(*left, *right);
	}

	Match matchesFabric(const QString& version, const QStringList& alternatives)
	{
		if (alternatives.isEmpty())
			return Match::Yes;

		const std::optional<SemVer> parsed = parseSemVer(version);
		Match result = Match::No;
		for (const QString& alternative : alternatives)
			result = either(result, matchAlternative(parsed, alternative));
		return result;
	}

	Match matchesMaven(const QString& version, const QString& range)
	{
		const QString spec = range.trimmed();
		if (spec.isEmpty() || spec == QLatin1String("*"))
			return Match::Yes;

		const std::optional<MavenVersion> parsed = parseMavenVersion(version);
		if (!startsRange(spec)) {
			// "1.20.1" asks for nothing; a comma here is some other syntax.
			const bool plain = !spec.contains(QLatin1Char(',')) &&
							   parseMavenVersion(spec).has_value();
			return plain ? Match::Yes : Match::Unknown;
		}

		const std::optional<QList<Bounds>> ranges = parseRanges(spec);
		if (!ranges)
			return Match::Unknown;
		Match result = Match::No;
		for (const Bounds& bounds : *ranges)
			result = either(result, matchBounds(parsed, bounds));
		return result;
	}

	Match matches(const QString& version, const ModDependency& dependency)
	{
		if (dependency.versions.isEmpty())
			return Match::Yes;
		if (dependency.syntax == ModDependency::Syntax::Fabric)
			return matchesFabric(version, dependency.versions);

		Match result = Match::No;
		for (const QString& range : dependency.versions)
			result = either(result, matchesMaven(version, range));
		return result;
	}

	QString describe(const ModDependency& dependency)
	{
		if (dependency.versions.isEmpty())
			return QString();
		return dependency.syntax == ModDependency::Syntax::Fabric
				   ? describeFabric(dependency.versions)
				   : describeMaven(dependency.versions);
	}
} // namespace VersionRange
