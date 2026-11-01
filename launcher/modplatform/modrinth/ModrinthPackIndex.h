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

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Modrinth
{

	struct IndexedVersion {
		QString id;
		QString projectId;
		QString name;
		QString versionNumber;
		QString mcVersion;
		QString downloadUrl;
		int downloadSize = 0;
		QString sha1;
		QString loaders;

		/* Full lists for consumers needing more than the single mcVersion /
		 * comma-joined loaders above (e.g. a QML detail view); see
		 * loadIndexedPackVersions(). */
		QStringList gameVersions;
		QStringList loaderList;
		QString datePublished;
		bool featured = false;
	};

	struct IndexedPack {
		QString projectId;
		QString slug;
		QString name;
		QString description;
		QString author;
		QString iconUrl;
		int downloads = 0;

		/* From the search hit only (see loadIndexedPack()): extras for a QML
		 * browse page without another round trip; defaults otherwise. */
		int follows = 0;
		QString dateModified;
		QStringList gameVersions;
		QString latestVersion;
		QStringList categories;
		/* The curated "display_categories" for a card; empty when the hit had
		 * none, then fall back to `categories` (which also has filter-only tags). */
		QStringList displayCategories;
		/* A card cover image: the hit's "featured_gallery", else the first of
		 * "gallery" (plain URLs on a search hit; richer objects come from the
		 * project endpoint, see fetchDetailBody()). Empty if no gallery. */
		QString featuredGalleryUrl;
		QStringList galleryUrls;
		/* Modrinth's generated accent colour as 0xRRGGBB; -1 when the hit had
		 * none (null or missing). */
		int color = -1;

		bool versionsLoaded = false;
		QVector<IndexedVersion> versions;
	};

	void loadIndexedPack(IndexedPack& pack, QJsonObject& obj);
	void loadIndexedPackVersions(IndexedPack& pack, QJsonArray& arr);

} // namespace Modrinth

Q_DECLARE_METATYPE(Modrinth::IndexedPack)
