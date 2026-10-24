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

/* Whether a version falls inside what a mod's metadata asks for, by the
 * rules the loader itself applies: Fabric's semantic-version predicates
 * for fabric.mod.json and quilt.mod.json, Maven ranges for mods.toml.
 *
 * Everything here answers "don't know" rather than guess. Mod versions
 * are free text often enough ("1.20.1-4.2.0", "${file.jarVersion}",
 * "23w45a") that a confident wrong answer would turn the mod health
 * check into a list of false alarms, and an alarm about a mod that works
 * is worse than no alarm at all. */
namespace VersionRange
{
	enum class Match { Yes, No, Unknown };

	/* <0, 0 or >0 as @p a sorts before, with or after @p b, or nothing
	 * when either is not a version this can order: a numeric core
	 * ("1", "0.92.2", "1.20.1"), optionally followed by "-<pre-release>"
	 * and/or "+<build>". Missing components count as 0 (1.21 == 1.21.0);
	 * a pre-release sorts before its release; build metadata is ignored.
	 * Pre-release identifiers compare as SemVer does: numeric ones by
	 * value, before alphanumeric ones, which compare as text; a shorter
	 * list sorts first when it is a prefix of the other. An empty
	 * pre-release ("1.20.2-") sorts before every other pre-release of
	 * that core - Fabric's "including pre-releases" spelling. */
	std::optional<int> compare(const QString& a, const QString& b);

	/* fabric.mod.json / quilt.mod.json. Any of @p alternatives may match
	 * (an empty list matches everything). Each alternative is predicates
	 * separated by spaces, all of which must match:
	 *   "*"                  any version
	 *   "1.2.3", "=1.2.3"    exactly that version
	 *   ">", ">=", "<", "<=" the comparison
	 *   "~1.2.3"             >=1.2.3 and below the next minor (<1.3)
	 *   "^1.2.3"             >=1.2.3 and below the next major (<2)
	 *   "1.20.x", "1.x", "1.20.*"
	 *                        the components before the wildcard match
	 * Unknown when @p version or a predicate that decides the answer
	 * cannot be read. */
	Match matchesFabric(const QString& version,
						const QStringList& alternatives);

	/* mods.toml versionRange: "" or "*" is any version; a bare version
	 * ("1.20.1") is Maven's soft "recommended" requirement, which also
	 * accepts any; "[1.20.1]" is exactly that; "[a,b)", "(a,b]", "[a,)",
	 * "(,b]" are the bounded forms, and several of them joined by ","
	 * ("[1,2),[3,)") are alternatives. Qualifiers follow Maven: a
	 * "-alpha"/"-beta"/"-rc"/"-pre"/"-snapshot" version sorts before its
	 * release, a "-<number>" one after it ("1.20.1-47.2.0" > "1.20.1");
	 * any other qualifier is Unknown. */
	Match matchesMaven(const QString& version, const QString& range);

	/* The matcher @p dependency's syntax calls for. */
	Match matches(const QString& version, const ModDependency& dependency);

	/* What @p dependency asks for, for a person to read: "0.92.0 or
	 * newer", "older than 1.21", "1.20.x", "1.20.1"; the metadata's own
	 * text when it is anything more involved; empty for any version. */
	QString describe(const ModDependency& dependency);
} // namespace VersionRange
