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

#include <QString>
#include <QStringList>
#include <QVector>

/* Versions of a modpack in a provider-neutral shape, normalised from the
 * Modrinth and CurseForge replies. No networking here: the page fetches. */
namespace ManagedPack
{

	/* How finished the author considers a version. Modrinth sends a
	 * lowercase string, CurseForge an integer. Unknown (missing or
	 * unrecognised) must stay distinct from Release. */
	enum class ReleaseType { Unknown, Release, Beta, Alpha };

	/* Capitalised English name; empty for Unknown, so label() leaves the
	 * bracket off. */
	QString releaseTypeToString(ReleaseType type);

	/* Parses Modrinth's `version_type`. */
	ReleaseType releaseTypeFromModrinth(const QString& value);

	/* Parses CurseForge's `releaseType` (1 = release, 2 = beta, 3 = alpha). */
	ReleaseType releaseTypeFromCurseForge(int value);

	struct Version {
		/* Modrinth version id, or CurseForge file id as text. Text because it
		 * is round-tripped through instance.cfg and Modrinth ids are not
		 * numbers. */
		QString versionId;

		/* The catalogue's name for this version, often "Cool Pack 1.2.3". */
		QString displayName;

		/* The bare version, e.g. "1.2.3"; empty on CurseForge. */
		QString versionNumber;

		/* Minecraft versions this pack version targets. */
		QStringList mcVersions;

		ReleaseType releaseType = ReleaseType::Unknown;

		/* Direct download for the pack archive. Empty when the provider withheld
		 * it (CurseForge, third-party distribution disabled); such versions are
		 * listed but cannot be installed. */
		QString downloadUrl;

		/* Markdown or HTML, as sent. Modrinth includes it in the list;
		 * CurseForge needs a request per file, so it is filled lazily and
		 * `changelogLoaded` tells empty from not yet fetched. */
		QString changelog;
		bool changelogLoaded = false;

		/* Whether there is somewhere to download it from. */
		bool isInstallable() const
		{
			return !downloadUrl.isEmpty();
		}

		/* One line for the version combo box: "<name>[ for <mc>] — <number>[ [Type]]".
		 * The game version is left out when the name already contains one, the
		 * number when the name contains it, the type when unknown. Only the
		 * " for %1" fragment is localised. */
		QString label() const;
	};

	using VersionList = QVector<Version>;

	/* Parses Modrinth's `/project/{id}/version` reply, newest first (entry 0
	 * is preselected). Versions without a download URL are kept; only
	 * installing them is refused. `ok` tells a malformed reply from a pack
	 * with no versions. */
	VersionList parseModrinthVersions(const QByteArray& bytes, bool* ok);

	/* Parses CurseForge's `/mods/{id}/files` reply, sorted newest first by
	 * file id: CurseForge promises no order and ids grow with age. */
	VersionList parseCurseForgeFiles(const QByteArray& bytes, bool* ok);

	/* Extracts the changelog from `/mods/{id}/files/{fileId}/changelog`;
	 * empty when the reply is unusable. */
	QString parseCurseForgeChangelog(const QByteArray& bytes);

	/* Index of the version whose id matches, or -1. Compared as trimmed
	 * strings: instance.cfg may hold odd spacing, and Modrinth ids are not
	 * numeric. */
	int indexOfVersionId(const VersionList& versions, const QString& versionId);

	/* Index of the version whose name matches, or -1. Fallback for records
	 * that have a label but no id. */
	int indexOfVersionName(const VersionList& versions, const QString& name);

} // namespace ManagedPack
