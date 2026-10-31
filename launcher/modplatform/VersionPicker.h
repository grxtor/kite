/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 Project Tick
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

#include <QJsonArray>
#include <QJsonObject>
#include <QString>

/* Which file of a project to install when nobody picked one. Neither API
 * promises an order, and CurseForge returns files for other loaders among
 * the matches, so "first" may not run here. Both pickers drop entries for
 * a different loader and take the most recently published. Not a version
 * comparison: version numbering is not semver ("1.9" would sort above
 * "1.10"), while both providers state the publish date. */
namespace ModPlatform
{

	/* One entry of a CurseForge /mods/{id}/files reply, or an empty object.
	 * `loader` may be empty (unknown or irrelevant). The Minecraft version is
	 * not re-checked: the query asked for it, and CurseForge mixes game
	 * versions and loaders in one array. */
	QJsonObject newestCurseForgeFile(const QJsonArray& files,
									 const QString& loader);

	/* The same for a Modrinth /project/{id}/version reply. */
	QJsonObject newestModrinthVersion(const QJsonArray& versions,
									  const QString& loader);

	/* Whether `tag` (compared case-insensitively) names a mod loader that
	 * CurseForge mixes into a file's "gameVersions" alongside Minecraft
	 * versions. Shared with FlameContentModel::parseVersionsResponse(). */
	bool isKnownLoaderName(const QString& tag);

} // namespace ModPlatform
