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
#include <QString>
#include <QStringList>
#include <algorithm>
#include <optional>

#include "VersionRange.h"

/* Pieces shared by the Fabric and Maven matchers behind VersionRange: the
 * three-valued answers and dotted numbers. Private to VersionRange.cpp; the
 * helpers are header-only so the file stays one translation unit. */
namespace VersionRangeDetail
{
	using VersionRange::Match;

	// ---- three-valued answers --------------------------------------------

	inline Match fromBool(bool yes)
	{
		return yes ? Match::Yes : Match::No;
	}

	//! Both hold: a No settles it, otherwise anything unsure leaves it unsure.
	inline Match both(Match a, Match b)
	{
		if (a == Match::No || b == Match::No)
			return Match::No;
		if (a == Match::Unknown || b == Match::Unknown)
			return Match::Unknown;
		return Match::Yes;
	}

	//! At least one holds: a Yes settles it, otherwise anything unsure leaves
	//! it unsure.
	inline Match either(Match a, Match b)
	{
		if (a == Match::Yes || b == Match::Yes)
			return Match::Yes;
		if (a == Match::Unknown || b == Match::Unknown)
			return Match::Unknown;
		return Match::No;
	}

	// ---- numbers ---------------------------------------------------------

	inline int sign(qint64 value)
	{
		return (value > 0) - (value < 0);
	}

	inline std::optional<qint64> parseNumber(const QString& text)
	{
		if (text.isEmpty())
			return std::nullopt;
		for (const QChar c : text) {
			if (c < QLatin1Char('0') || c > QLatin1Char('9'))
				return std::nullopt;
		}
		bool ok = false;
		const qint64 value = text.toLongLong(&ok);
		if (!ok)
			return std::nullopt;
		return value;
	}

	//! "1.20.1" -> 1, 20, 1; nothing when any part is not a plain number.
	inline std::optional<QList<qint64>> parseNumbers(const QString& text)
	{
		QList<qint64> numbers;
		for (const QString& part : text.split(QLatin1Char('.'))) {
			const std::optional<qint64> number = parseNumber(part);
			if (!number)
				return std::nullopt;
			numbers.append(*number);
		}
		return numbers;
	}

	//! Missing components count as 0, so 1.21 and 1.21.0 are the same.
	inline int compareNumbers(const QList<qint64>& a, const QList<qint64>& b)
	{
		const qsizetype count = std::max(a.size(), b.size());
		for (qsizetype i = 0; i < count; ++i) {
			const qint64 left = i < a.size() ? a.at(i) : 0;
			const qint64 right = i < b.size() ? b.at(i) : 0;
			if (left != right)
				return sign(left - right);
		}
		return 0;
	}
} // namespace VersionRangeDetail
