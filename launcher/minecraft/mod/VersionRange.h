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

#include <QString>
#include <QStringList>
#include <optional>

#include "ModDetails.h"

// Whether a version falls inside what a mod's metadata asks for, by the
// loader's own rules: Fabric's semantic-version predicates for fabric.mod.json
// and quilt.mod.json, Maven ranges for mods.toml. Anything unreadable answers
// "don't know" rather than guess: mod versions are free text often enough
// ("1.20.1-4.2.0", "${file.jarVersion}", "23w45a") that a wrong answer would
// turn the health check into false alarms.
namespace VersionRange
{
	enum class Match { Yes, No, Unknown };

	// <0, 0 or >0 as @p a sorts before, with or after @p b; nothing if either
	// is not orderable: a numeric core ("1", "0.92.2"), optionally
	// "-<pre-release>" and/or "+<build>". Missing components count as 0 (1.21
	// == 1.21.0); a pre-release sorts before its release; build metadata is
	// ignored. Pre-release identifiers compare as SemVer does (numeric by
	// value, before alphanumeric text; a prefix sorts first). An empty
	// pre-release ("1.20.2-") sorts before every other of that core (Fabric's
	// "including pre-releases" spelling).
	std::optional<int> compare(const QString& a, const QString& b);

	// fabric.mod.json / quilt.mod.json. Any of @p alternatives may match (empty
	// matches everything). An alternative is space-separated predicates that
	// all must match:
	//   "*"                  any version
	//   "1.2.3", "=1.2.3"    exactly that version
	//   ">", ">=", "<", "<=" the comparison
	//   "~1.2.3"             >=1.2.3 and below the next minor (<1.3)
	//   "^1.2.3"             >=1.2.3 and below the next major (<2)
	//   "1.20.x", "1.x", "1.20.*"  the components before the wildcard match
	// Unknown when @p version or a deciding predicate cannot be read.
	Match matchesFabric(const QString& version,
						const QStringList& alternatives);

	// mods.toml versionRange: "" or "*" is any version; a bare version
	// ("1.20.1") is Maven's soft requirement and accepts any; "[1.20.1]" is
	// exactly that; "[a,b)", "(a,b]", "[a,)", "(,b]" are bounded, several
	// joined by "," are alternatives. Qualifiers follow Maven: "-alpha",
	// "-beta", "-rc", "-pre" and "-snapshot" sort before the release,
	// "-<number>" after it ("1.20.1-47.2.0" > "1.20.1"); any other qualifier
	// is Unknown.
	Match matchesMaven(const QString& version, const QString& range);

	Match matches(const QString& version, const ModDependency& dependency);

	// What @p dependency asks for, for a person: "0.92.0 or newer", "older than
	// 1.21", "1.20.x", "1.20.1"; the metadata's own text if more involved;
	// empty for any version.
	QString describe(const ModDependency& dependency);
} // namespace VersionRange
