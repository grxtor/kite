/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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
#include <QString>
#include <QStringList>
#include <QUrl>

#include "modplatform/ContentType.h"

/* One place that knows how to address CurseForge and Modrinth. Pure URL
 * builders: no networking, no state. The caller issues the request and
 * parses the reply, whose JSON differs per platform. */
namespace ModPlatform
{

	/* One entry of a provider's sort menu; each provider has its own list. */
	struct SortingMethod {
		/* Value the provider's own API expects. */
		QString apiValue;
		/* Label shown in the sort combo box. */
		QString readableName;
	};

	/* Which side of the game a project has to work on. Only Modrinth
	 * indexes this; CurseForge ignores it. */
	enum class SideFilter { Any, Client, Server, Universal };

	/* One entry of a provider's category list. `id` is what the provider
	 * wants back in a search (a number on CurseForge, the name on Modrinth). */
	struct Category {
		QString name;
		QString id;
	};

	struct SearchFilters {
		/* Empty means every Minecraft version. Several can be ticked, but
		 * only Modrinth can express that; see each searchUrl(). */
		QStringList mcVersions;
		/* Empty means "any loader". Ignored for content types that are not
		 * loader-specific, see contentTypeUsesLoader(). */
		QStringList loaders;
		SideFilter side = SideFilter::Any;
		bool openSourceOnly = false;
		/* Provider-specific category ids from categoriesUrl(). Empty means
		 * every category. */
		QStringList categoryIds;

		bool operator==(const SearchFilters& other) const
		{
			return mcVersions == other.mcVersions &&
				   loaders == other.loaders && side == other.side &&
				   openSourceOnly == other.openSourceOnly &&
				   categoryIds == other.categoryIds;
		}
		bool operator!=(const SearchFilters& other) const
		{
			return !(*this == other);
		}
	};

	/* What to search for. */
	struct SearchQuery {
		ContentType contentType = ContentType::Mod;
		QString term;
		SearchFilters filters;
		/* Index into this provider's sortingMethods(); out of range falls back
		 * to the first entry. */
		int sortIndex = 0;
		int offset = 0;
	};

	/* Which versions of a project to list. Empty filters are omitted from
	 * the query string. The providers differ on more than one version or
	 * loader (see each projectVersionsUrl()), so they get the whole list. */
	struct VersionQuery {
		QString projectId;
		ContentType contentType = ContentType::Mod;
		QStringList mcVersions;
		QStringList loaders;
	};

	/* One loader as a query list; empty yields an empty list, meaning "no
	 * loader filter". */
	inline QStringList singleLoaderList(const QString& loader)
	{
		return loader.isEmpty() ? QStringList() : QStringList{loader};
	}

	/* Same for a single Minecraft version. */
	inline QStringList singleVersionList(const QString& version)
	{
		return version.isEmpty() ? QStringList() : QStringList{version};
	}

	/* A user-typed search term, percent-encoded: an unescaped "&", "+" or
	 * "#" would end or change the query parameter. */
	inline QString encodeSearchTerm(const QString& term)
	{
		return QString::fromLatin1(QUrl::toPercentEncoding(term));
	}

	class ContentApi
	{
	  public:
		virtual ~ContentApi() = default;

		/* Stable identifier stored in sidecars and SelectedMod/DownloadItem:
		 * "curseforge" or "modrinth". */
		virtual QString id() const = 0;
		/* Name shown to the user. */
		virtual QString displayName() const = 0;
		/* How many results the provider returns per search page. */
		virtual int searchPageSize() const = 0;

		/* Whether the search indexes SearchFilters' `side` and `openSourceOnly`
		 * (only Modrinth does); the filter panel leaves those groups out
		 * otherwise. */
		virtual bool supportsExtendedFilters() const = 0;

		/* The sorts offered, in UI order. Never empty. */
		virtual QList<SortingMethod> sortingMethods() const = 0;

		virtual QUrl searchUrl(const SearchQuery& query) const = 0;
		virtual QUrl projectUrl(const QString& projectId) const = 0;
		virtual QUrl projectVersionsUrl(const VersionQuery& query) const = 0;

		/* Long-form project description as HTML; converting from Markdown is
		 * part of parsing the reply. */
		virtual QUrl projectBodyUrl(const QString& projectId) const = 0;

		/* The categories offered for a kind of content. Fetched once when the
		 * filter panel is built. */
		virtual QUrl categoriesUrl(ContentType contentType) const = 0;

		/* The project's page on the provider's website, for a browser; not an
		 * API endpoint. */
		virtual QUrl projectPageUrl(const QString& projectId) const = 0;
	};

} // namespace ModPlatform
