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

#include "ModrinthApi.h"

#include <QObject>

namespace
{

	QString sortValueAt(const QList<ModPlatform::SortingMethod>& sorts,
						int sortIndex)
	{
		if (sortIndex >= 0 && sortIndex < sorts.size()) {
			return sorts.at(sortIndex).apiValue;
		}
		return sorts.first().apiValue;
	}

	/* Facets are an array of arrays: the outer is ANDed, each inner ORed. Two
	 * ticked loaders make one group, not two. */
	QString facetGroups(const QList<QStringList>& groups)
	{
		QStringList rendered;
		rendered.reserve(groups.size());
		for (const QStringList& group : groups) {
			if (group.isEmpty()) {
				continue;
			}
			QStringList quoted;
			quoted.reserve(group.size());
			for (const QString& term : group) {
				quoted.append(QString("\"%1\"").arg(term));
			}
			rendered.append(QStringLiteral("[") +
							quoted.join(QLatin1Char(',')) +
							QStringLiteral("]"));
		}
		return QStringLiteral("[") + rendered.join(QLatin1Char(',')) +
			   QStringLiteral("]");
	}

	/* Environment choice -> facet groups. "Client side" is two conditions
	 * (does something on the client, not required on the server), so each
	 * choice adds two ANDed groups. `Universal` means required on both. */
	void appendSideFacets(QList<QStringList>& facets,
						  ModPlatform::SideFilter side)
	{
		switch (side) {
			case ModPlatform::SideFilter::Client:
				facets.append({QStringLiteral("client_side:required"),
							   QStringLiteral("client_side:optional")});
				facets.append({QStringLiteral("server_side:optional"),
							   QStringLiteral("server_side:unsupported")});
				break;
			case ModPlatform::SideFilter::Server:
				facets.append({QStringLiteral("server_side:required"),
							   QStringLiteral("server_side:optional")});
				facets.append({QStringLiteral("client_side:optional"),
							   QStringLiteral("client_side:unsupported")});
				break;
			case ModPlatform::SideFilter::Universal:
				facets.append({QStringLiteral("client_side:required")});
				facets.append({QStringLiteral("server_side:required")});
				break;
			case ModPlatform::SideFilter::Any:
				break;
		}
	}

	QString quotedJsonArray(const QStringList& values)
	{
		QStringList quoted;
		quoted.reserve(values.size());
		for (const QString& value : values) {
			quoted.append(QString("\"%1\"").arg(value));
		}
		return QStringLiteral("[") + quoted.join(QLatin1Char(',')) +
			   QStringLiteral("]");
	}

	void appendParam(QString& url, const QString& param)
	{
		url += (url.contains(QLatin1Char('?')) ? QLatin1Char('&')
											   : QLatin1Char('?'));
		url += param;
	}

} // namespace

const ModrinthApi& ModrinthApi::get()
{
	static const ModrinthApi instance;
	return instance;
}

QString ModrinthApi::apiHost()
{
	return QStringLiteral("api.modrinth.com");
}

QString ModrinthApi::apiBase()
{
	return QStringLiteral("https://") + apiHost() + QStringLiteral("/v2");
}

QString ModrinthApi::cdnHost()
{
	return QStringLiteral("cdn.modrinth.com");
}

const QStringList& ModrinthApi::mrpackHosts()
{
	/* The format's allowlist. The CDN serves anything installed through the
	 * launcher; the others are where authors point at mods not on Modrinth. */
	static const QStringList hosts = {QStringLiteral("cdn.modrinth.com"),
									  QStringLiteral("github.com"),
									  QStringLiteral("raw.githubusercontent.com"),
									  QStringLiteral("gitlab.com")};
	return hosts;
}

bool ModrinthApi::isMrpackHost(const QUrl& url)
{
	if (!url.isValid()) {
		return false;
	}
	/* Case-folded: host names are case insensitive and manifests are
	 * hand-edited. */
	return mrpackHosts().contains(url.host().toLower());
}

QString ModrinthApi::id() const
{
	return QStringLiteral("modrinth");
}

QString ModrinthApi::displayName() const
{
	return QStringLiteral("Modrinth");
}

int ModrinthApi::searchPageSize() const
{
	return 20;
}

QList<ModPlatform::SortingMethod> ModrinthApi::sortingMethods() const
{
	/* Modrinth names its sorts and offers a different set from CurseForge. */
	return {
		{QStringLiteral("relevance"), QObject::tr("Sort by Relevance")},
		{QStringLiteral("downloads"), QObject::tr("Sort by Downloads")},
		{QStringLiteral("follows"), QObject::tr("Sort by Follows")},
		{QStringLiteral("newest"), QObject::tr("Sort by Newest")},
		{QStringLiteral("updated"), QObject::tr("Sort by Last Updated")},
	};
}

QUrl ModrinthApi::searchUrl(const ModPlatform::SearchQuery& query) const
{
	QList<QStringList> facets;
	facets.append(QStringList{
		QStringLiteral("project_type:") +
		ModPlatform::contentTypeToModrinthFacet(query.contentType)});

	if (!query.filters.mcVersions.isEmpty()) {
		QStringList versionFacets;
		versionFacets.reserve(query.filters.mcVersions.size());
		for (const QString& version : query.filters.mcVersions) {
			versionFacets.append(QStringLiteral("versions:") + version);
		}
		/* One group: several ticked versions mean "any of these". */
		facets.append(versionFacets);
	}

	/* Modrinth models the loader as a category. Sending it for a resource
	 * or data pack matches nothing, so only mods get it. */
	if (!query.filters.loaders.isEmpty() &&
		ModPlatform::contentTypeUsesLoader(query.contentType)) {
		QStringList loaderFacets;
		loaderFacets.reserve(query.filters.loaders.size());
		for (const QString& loader : query.filters.loaders) {
			loaderFacets.append(QStringLiteral("categories:") + loader);
		}
		facets.append(loaderFacets);
	}

	if (!query.filters.categoryIds.isEmpty()) {
		QStringList categoryFacets;
		categoryFacets.reserve(query.filters.categoryIds.size());
		for (const QString& category : query.filters.categoryIds) {
			categoryFacets.append(QStringLiteral("categories:") + category);
		}
		/* One group: two categories mean "either". */
		facets.append(categoryFacets);
	}

	appendSideFacets(facets, query.filters.side);

	if (query.filters.openSourceOnly) {
		facets.append(QStringList{QStringLiteral("open_source:true")});
	}

	/* One multi-arg call: the term is user-supplied, and a chained arg()
	 * would read a "%4" in it as the next placeholder. */
	QString url = QString("%1/search?"
						  "query=%2&"
						  "index=%3&"
						  "offset=%4&"
						  "limit=%5")
					  .arg(apiBase(),
						   ModPlatform::encodeSearchTerm(query.term),
						   sortValueAt(sortingMethods(), query.sortIndex),
						   QString::number(query.offset),
						   QString::number(searchPageSize()));
	url += QStringLiteral("&facets=") + facetGroups(facets);

	return QUrl(url);
}

QUrl ModrinthApi::projectUrl(const QString& projectId) const
{
	return QUrl(QString("%1/project/%2").arg(apiBase(), projectId));
}

QUrl ModrinthApi::projectBodyUrl(const QString& projectId) const
{
	/* No description endpoint: the body is a Markdown field of the project. */
	return projectUrl(projectId);
}

QUrl ModrinthApi::categoriesUrl(ModPlatform::ContentType contentType) const
{
	/* One list for the whole site; parsing filters by project type. */
	Q_UNUSED(contentType)
	return QUrl(QString("%1/tag/category").arg(apiBase()));
}

QUrl ModrinthApi::projectPageUrl(const QString& projectId) const
{
	/* `/mod/` redirects to the right section for every project type. */
	return QUrl(QStringLiteral("https://modrinth.com/mod/") + projectId);
}

QUrl ModrinthApi::projectVersionsUrl(
	const ModPlatform::VersionQuery& query) const
{
	QString url =
		QString("%1/project/%2/version").arg(apiBase(), query.projectId);

	/* Modrinth treats the list as "any of these"; CurseForge only reads
	 * the first. */
	if (!query.mcVersions.isEmpty()) {
		appendParam(url, QStringLiteral("game_versions=") +
							 quotedJsonArray(query.mcVersions));
	}

	/* Modrinth treats the list as "any of these". */
	if (!query.loaders.isEmpty() &&
		ModPlatform::contentTypeUsesLoader(query.contentType)) {
		appendParam(url, QStringLiteral("loaders=") +
							 quotedJsonArray(query.loaders));
	}

	return QUrl(url);
}

QUrl ModrinthApi::versionUrl(const QString& versionId)
{
	return QUrl(QString("%1/version/%2").arg(apiBase(), versionId));
}

bool ModrinthApi::isVersionId(const QString& candidate)
{
	/* Eight base62 characters; checked by shape to avoid a request that
	 * cannot succeed. */
	if (candidate.size() != 8) {
		return false;
	}
	for (const QChar c : candidate) {
		if (c.unicode() > 127 || !c.isLetterOrNumber()) {
			return false;
		}
	}
	return true;
}

QUrl ModrinthApi::versionByHashUrl(const QString& sha1)
{
	/* The algorithm is named: the endpoint's SHA-1 default is not a promise
	 * and a digest under the wrong name matches nothing. */
	return QUrl(QString("%1/version_file/%2?algorithm=sha1")
					.arg(apiBase(), sha1.toLower()));
}

QUrl ModrinthApi::versionsByHashesUrl()
{
	return QUrl(QString("%1/version_files").arg(apiBase()));
}

QUrl ModrinthApi::projectVersionsUrlForLoaders(const QString& projectId,
											   const QStringList& loaders)
{
	QString url = QString("%1/project/%2/version").arg(apiBase(), projectId);
	if (!loaders.isEmpty()) {
		appendParam(url, QStringLiteral("loaders=") + quotedJsonArray(loaders));
	}
	return QUrl(url);
}

QUrl ModrinthApi::modpackSearchUrl(const QString& term, int sortIndex,
								   int offset, const QString& gameVersion,
								   const QStringList& loaders)
{
	QList<QStringList> facets;
	facets.append(QStringList{QStringLiteral("project_type:modpack")});

	if (!gameVersion.isEmpty()) {
		facets.append(
			QStringList{QStringLiteral("versions:") + gameVersion});
	}

	if (!loaders.isEmpty()) {
		/* One group: several ticked loaders mean "any of these". */
		QStringList loaderFacets;
		loaderFacets.reserve(loaders.size());
		for (const QString& loader : loaders) {
			loaderFacets.append(QStringLiteral("categories:") + loader);
		}
		facets.append(loaderFacets);
	}

	return QUrl(QString("%1/search?"
						"query=%2&"
						"facets=%3&"
						"index=%4&"
						"offset=%5&"
						"limit=%6")
					.arg(apiBase(), ModPlatform::encodeSearchTerm(term),
						 facetGroups(facets),
						 sortValueAt(get().sortingMethods(), sortIndex),
						 QString::number(offset),
						 QString::number(get().searchPageSize())));
}

QUrl ModrinthApi::nameSearchUrl(const QString& encodedTerm, int limit)
{
	QString url = QString("%1/search?query=%2&limit=%3")
					  .arg(apiBase(), encodedTerm, QString::number(limit));
	/* Quotes are percent-encoded because the string goes straight to QUrl;
	 * no arg() call, which would read "%22" as placeholder 22. */
	url += QStringLiteral("&facets=[[%22project_type:mod%22]]");
	return QUrl(url);
}
