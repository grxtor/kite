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

#include <QJsonArray>
#include <QList>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QUrl>

#include "models/ManagedPackController.h"

// Finding a modpack's own picture (Modrinth gallery, CurseForge screenshots) for an instance cover; pure, no network.
namespace PlatformCover
{
	using Provider = ManagedPackController::Provider;

	/* What an instance says about the pack it came from. */
	struct PackFacts {
		// Kite's own keys (PackProvider, PackId, PackSlug).
		QString packProvider;
		QString packId;
		QString packSlug;
		// What other launchers wrote (ManagedPackType, ManagedPackID).
		QString legacyProvider;
		QString legacyId;
		QString iconKey;
		// Names the pack may be listed under, best first.
		QStringList packTitles;
		QString instanceName;
	};

	/* One way to find the pack: by its project id or slug, or by title. */
	struct Lookup {
		Provider provider = Provider::Unknown;
		QString projectId; ///< id or slug; empty when searching by title
		QString title;

		bool byId() const
		{
			return !projectId.isEmpty();
		}
		bool operator==(const Lookup& other) const
		{
			return provider == other.provider &&
				   projectId == other.projectId && title == other.title;
		}
	};

	/* "modrinth_<slug>" gives the slug; anything else, empty. */
	QString slugFromIconKey(const QString& iconKey);

	/* The lookups to try, in order: a single one for an id (Kite's keys, the
	 * other launchers', a Modrinth slug in the icon key), else exact-title
	 * searches on the platform the facts point to; none when they point to no
	 * platform, so a home-made instance is never searched. */
	QList<Lookup> lookupsFor(const PackFacts& facts);

	/* Image formats Qt can decode here, lower case with jpg as jpeg. */
	QStringList decodableFormats();

	/* Modrinth gallery pictures to try, best first: featured then the rest,
	 * all originals ("raw_url") before the 350 px "url" copies, only formats in
	 * @p formats on the platform's own image host (no GIFs), at most
	 * kMaxCandidates. */
	QStringList modrinthImages(const QJsonArray& gallery,
							   const QStringList& formats);
	// The same for CurseForge screenshots, in their order.
	QStringList flameImages(const QJsonArray& screenshots,
							const QStringList& formats);

	constexpr int kMaxCandidates = 4;
	constexpr int kMinImageWidth = 1000;
	constexpr double kMinAspect = 1.3;
	// Whether a picture of @p size can be a wide hero without a blown-up crop.
	bool suitableAsCover(const QSize& size);

	/* Id of the one search hit titled @p title (trimmed, ignoring case), or
	 * empty when none or several are: a guess between two packs would put
	 * the wrong picture on an instance. */
	QString uniqueModrinthMatch(const QJsonArray& hits, const QString& title);
	QString uniqueFlameMatch(const QJsonArray& hits, const QString& title);

	/* The modpack search (relevance order) or project request for @p lookup. */
	QUrl searchUrl(const Lookup& lookup);
	QUrl projectUrl(Provider provider, const QString& projectId);

	/* "name" of <instanceRoot>/flame/manifest.json, the pack's title as
	 * CurseForge lists it; empty when absent. Reads a file: off the GUI
	 * thread. */
	QString flameManifestName(const QString& instanceRoot);

	/* A lookup that found nothing is not repeated for this long. */
	constexpr qint64 kRetryAfterSeconds = 7 * 24 * 60 * 60;
	/* Whether a lookup is due, given when one last found no art (0 = never). */
	bool retryDue(qint64 checkedAt, qint64 now);
} // namespace PlatformCover
