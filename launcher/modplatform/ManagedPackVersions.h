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

/* The list of versions a modpack has, in a shape that does not depend on
 * which catalogue answered.
 *
 * The existing Flame::IndexedPack / Modrinth::IndexedPack pair already
 * models "a pack and its versions", but each is shaped around what its
 * own browse page needs: Flame keeps ids as ints and has no version
 * number distinct from the display name, Modrinth keeps a single game
 * version rather than the list, and neither carries a changelog. The
 * managed-pack page needs the union of those, plus the changelog, and it
 * has to show one combo box whose entries read the same whichever
 * provider filled it. Widening both existing structs would push
 * provider-specific holes into the browse pages that are happy today, so
 * this is a separate model that the two parsers below normalise into.
 *
 * No networking lives here. Fetching is the caller's business - the same
 * split ContentApi makes - because the two providers need a different
 * number of requests to answer the same question and the page is the
 * thing that knows how to show progress for that. */
namespace ManagedPack
{

	/* How finished the pack author considers a version.
	 *
	 * Both providers publish this and both mean the same three things by
	 * it, they just spell it differently: Modrinth sends a lowercase
	 * string, CurseForge a small integer. Unknown covers "the field was
	 * missing or held something we do not recognise", which must stay
	 * distinct from Release - labelling an unknown version as a release
	 * would be a claim we cannot back up. */
	enum class ReleaseType { Unknown, Release, Beta, Alpha };

	/* Capitalised English name, or an empty string for Unknown. Empty is
	 * what lets label() leave the bracket off entirely rather than
	 * printing "[Unknown]". */
	QString releaseTypeToString(ReleaseType type);

	/* Parse Modrinth's `version_type` ("release" / "beta" / "alpha"). */
	ReleaseType releaseTypeFromModrinth(const QString& value);

	/* Parse CurseForge's `releaseType` (1 = release, 2 = beta,
	 * 3 = alpha). */
	ReleaseType releaseTypeFromCurseForge(int value);

	struct Version {
		/* Modrinth version id, or CurseForge file id rendered as a
		 * string. Kept as text because it is round-tripped through
		 * instance.cfg and through the importer's extra-info map, and
		 * because Modrinth's ids are not numbers at all. */
		QString versionId;

		/* The name the catalogue gives this version - often the pack
		 * title with a version in it, e.g. "Cool Pack 1.2.3". */
		QString displayName;

		/* The bare version, e.g. "1.2.3". CurseForge has no separate
		 * field for this, so it stays empty there; label() is written
		 * to cope. */
		QString versionNumber;

		/* Minecraft versions this pack version targets. A list because
		 * a pack may well declare several, and because the label needs
		 * to know whether any of them is already visible in
		 * displayName before it appends one. */
		QStringList mcVersions;

		ReleaseType releaseType = ReleaseType::Unknown;

		/* Direct download for the pack archive. Empty when the
		 * provider withheld it, which CurseForge does for packs whose
		 * author disabled third-party distribution. Such versions are
		 * still listed - the user should see that they exist - but they
		 * cannot be installed from here. */
		QString downloadUrl;

		/* Markdown or HTML, as the provider sent it. Modrinth includes
		 * it in the version list; CurseForge needs a second request per
		 * file, so there it is filled in lazily and `changelogLoaded`
		 * says whether that has happened. Without the flag an empty
		 * changelog and a not-yet-fetched one look identical, and the
		 * page would either refetch forever or show "no changelog" for
		 * something it simply has not asked about. */
		QString changelog;
		bool changelogLoaded = false;

		/* Whether this version can actually be installed - i.e. we have
		 * somewhere to download it from. */
		bool isInstallable() const
		{
			return !downloadUrl.isEmpty();
		}

		/* One line for the version combo box.
		 *
		 * Reads "<name>[ for <mc>] — <number>[ [Type]]", with each
		 * optional part left out when it would be redundant or unknown:
		 *
		 *  - the game version is appended only when the name does not
		 *    already contain one, because most pack authors put it in
		 *    the name and "Pack 1.20.1 for 1.20.1" reads badly;
		 *  - the bare version number is left out when the name already
		 *    contains it, for the same reason;
		 *  - the release type is left out when unknown.
		 *
		 * The result is deliberately not localised beyond the " for %1"
		 * fragment: the rest is punctuation and provider data. */
		QString label() const;
	};

	using VersionList = QVector<Version>;

	/* Parse the reply from Modrinth's `/project/{id}/version`.
	 *
	 * Versions with no usable download are still returned, unlike
	 * Modrinth::loadIndexedPackVersions which drops them: the page lists
	 * every version the pack has and only refuses to *install* the ones
	 * without a URL. Dropping them would make the combo box silently
	 * disagree with the pack's own version history.
	 *
	 * Order is preserved. Modrinth answers newest-first and the page
	 * relies on that, since entry 0 is what it preselects.
	 *
	 * Returns an empty list on malformed input; `ok` distinguishes "the
	 * pack genuinely has no versions" from "we could not read the
	 * reply", which the page shows very differently. */
	VersionList parseModrinthVersions(const QByteArray& bytes, bool* ok);

	/* Parse the reply from CurseForge's `/mods/{id}/files`.
	 *
	 * Sorted newest-first by file id, because CurseForge does not
	 * promise an order and the page needs entry 0 to be the newest.
	 * File ids are monotonic per project, so they order by age without
	 * having to parse dates. */
	VersionList parseCurseForgeFiles(const QByteArray& bytes, bool* ok);

	/* Pull the changelog out of CurseForge's
	 * `/mods/{id}/files/{fileId}/changelog`, which wraps it in a `data`
	 * string. Returns an empty string when the reply is unusable - a
	 * missing changelog is not worth failing the page over. */
	QString parseCurseForgeChangelog(const QByteArray& bytes);

	/* Index of the version whose id matches, or -1.
	 *
	 * Compared as trimmed strings rather than by number: the installed
	 * id comes back from instance.cfg, where an older Kite may have
	 * written it with different spacing, and on Modrinth it is not
	 * numeric to begin with. */
	int indexOfVersionId(const VersionList& versions, const QString& versionId);

	/* Index of the version whose *name* matches, or -1.
	 *
	 * A fallback for records that have a version label but no id, which
	 * is how some older imports look. Also useful because Modrinth's
	 * modpack index format has been known to disagree with the
	 * catalogue about the id while the human version string still
	 * matches. */
	int indexOfVersionName(const VersionList& versions, const QString& name);

} // namespace ManagedPack
