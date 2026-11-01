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

#include "FlameApi.h"

#include <QObject>

namespace
{

	void appendParam(QString& url, const QString& param)
	{
		url += (url.contains(QLatin1Char('?')) ? QLatin1Char('&')
											   : QLatin1Char('?'));
		url += param;
	}

} // namespace

const FlameApi& FlameApi::get()
{
	static const FlameApi instance;
	return instance;
}

QString FlameApi::apiHost()
{
	return QStringLiteral("api.curseforge.com");
}

QString FlameApi::apiBase()
{
	return QStringLiteral("https://") + apiHost() + QStringLiteral("/v1");
}

QString FlameApi::siteHost()
{
	return QStringLiteral("www.curseforge.com");
}

int FlameApi::minecraftGameId()
{
	return 432;
}

int FlameApi::modpackClassId()
{
	return 4471;
}

QString FlameApi::id() const
{
	return QStringLiteral("curseforge");
}

QString FlameApi::displayName() const
{
	return QStringLiteral("CurseForge");
}

int FlameApi::searchPageSize() const
{
	return 25;
}

QList<ModPlatform::SortingMethod> FlameApi::sortingMethods() const
{
	/* CurseForge's ModsSearchSortField values, sent verbatim as sortField;
	 * the list order is free. */
	return {
		{QStringLiteral("1"), QObject::tr("Sort by Featured")},
		{QStringLiteral("2"), QObject::tr("Sort by Popularity")},
		{QStringLiteral("3"), QObject::tr("Sort by Last Updated")},
		{QStringLiteral("4"), QObject::tr("Sort by Name")},
		{QStringLiteral("5"), QObject::tr("Sort by Author")},
		{QStringLiteral("6"), QObject::tr("Sort by Downloads")},
		{QStringLiteral("7"), QObject::tr("Sort by Category")},
		{QStringLiteral("8"), QObject::tr("Sort by Game Version")},
	};
}

QUrl FlameApi::searchUrl(const ModPlatform::SearchQuery& query) const
{
	/* One multi-arg call: the term is user-supplied, and a chained arg()
	 * would read a "%7" in it as the next placeholder. */
	QString url = QString("%1/mods/search?"
						  "gameId=%2&"
						  "classId=%3&"
						  "index=%4&"
						  "pageSize=%5&"
						  "searchFilter=%6&"
						  "sortField=%7&"
						  "sortOrder=desc")
					  .arg(apiBase(), QString::number(minecraftGameId()),
						   QString::number(
							   ModPlatform::contentTypeToCurseForgeClassId(
								   query.contentType)),
						   QString::number(query.offset),
						   QString::number(searchPageSize()),
						   ModPlatform::encodeSearchTerm(query.term),
						   ModPlatform::sortValueAt(
						       sortingMethods(), query.sortIndex));

	if (!query.filters.mcVersions.isEmpty()) {
		/* CurseForge's search takes one game version, so a multi-version filter
		 * is honoured for the first only (guessing the newest would be wrong). */
		url += QStringLiteral("&gameVersion=") +
			   query.filters.mcVersions.first();
	}

	if (!query.filters.loaders.isEmpty() &&
		ModPlatform::contentTypeUsesLoader(query.contentType)) {
		/* Numeric ModLoaderType ids. A loader CurseForge has no id for is
		 * dropped; if none remain the parameter is omitted, since an empty
		 * bracket pair is not promised to be accepted. */
		QStringList loaderIds;
		loaderIds.reserve(query.filters.loaders.size());
		for (const QString& loader : query.filters.loaders) {
			const int id = ModPlatform::loaderToCurseForgeModLoaderType(loader);
			if (id > 0) {
				loaderIds.append(QString::number(id));
			}
		}
		if (!loaderIds.isEmpty()) {
			url += QStringLiteral("&modLoaderTypes=[") +
				   loaderIds.join(QLatin1Char(',')) + QStringLiteral("]");
		}
	}

	if (!query.filters.categoryIds.isEmpty()) {
		/* Bare numeric ids, unquoted: CurseForge rejects the quoted form. */
		url += QStringLiteral("&categoryIds=[") +
			   query.filters.categoryIds.join(QLatin1Char(',')) +
			   QStringLiteral("]");
	}

	/* SearchFilters::side and ::openSourceOnly are not used: CurseForge
	 * indexes neither, and the panel hides both boxes here. */

	return QUrl(url);
}

QUrl FlameApi::projectUrl(const QString& projectId) const
{
	return QUrl(QString("%1/mods/%2").arg(apiBase(), projectId));
}

QUrl FlameApi::projectBodyUrl(const QString& projectId) const
{
	/* The long description is ready-made HTML on its own endpoint. */
	return QUrl(QString("%1/mods/%2/description").arg(apiBase(), projectId));
}

QUrl FlameApi::categoriesUrl(ModPlatform::ContentType contentType) const
{
	return QUrl(
		QString("%1/categories?gameId=%2&classId=%3")
			.arg(apiBase(), QString::number(minecraftGameId()),
				 QString::number(ModPlatform::contentTypeToCurseForgeClassId(
					 contentType))));
}

QUrl FlameApi::projectPageUrl(const QString& projectId) const
{
	/* Not under the API host and not per content type: the site resolves a
	 * numeric project id and redirects. */
	return QUrl(QStringLiteral("https://www.curseforge.com/projects/") +
				projectId);
}

QUrl FlameApi::projectVersionsUrl(const ModPlatform::VersionQuery& query) const
{
	/* The whole list: the default page of fifty files shows an arbitrary
	 * window of a long-lived mod's history, and callers picking "the newest
	 * that fits" need all of them. */
	QString url = QString("%1/mods/%2/files?pageSize=10000")
					  .arg(apiBase(), query.projectId);

	if (!query.mcVersions.isEmpty()) {
		appendParam(url, QStringLiteral("gameVersion=") +
							 query.mcVersions.first());
	}

	/* The file list takes a single modLoaderType, so it is sent only for
	 * exactly one loader; otherwise every file is listed rather than hiding
	 * versions behind an arbitrary pick. */
	if (query.loaders.size() == 1 &&
		ModPlatform::contentTypeUsesLoader(query.contentType)) {
		const int loaderType =
			ModPlatform::loaderToCurseForgeModLoaderType(query.loaders.first());
		if (loaderType > 0) {
			appendParam(url, QStringLiteral("modLoaderType=") +
								 QString::number(loaderType));
		}
	}

	return QUrl(url);
}

QUrl FlameApi::fileUrl(const QString& projectId, const QString& fileId)
{
	return QUrl(QString("%1/mods/%2/files/%3").arg(apiBase(), projectId,
												   fileId));
}

QUrl FlameApi::matchFingerprintsUrl()
{
	return QUrl(QString("%1/fingerprints").arg(apiBase()));
}

QUrl FlameApi::projectsUrl()
{
	return QUrl(QString("%1/mods").arg(apiBase()));
}

QUrl FlameApi::modpackSearchUrl(const QString& term, int sortIndex, int offset)
{
	return QUrl(QString("%1/mods/search?"
						"gameId=%2&"
						"classId=%3&"
						"index=%4&"
						"pageSize=%5&"
						"searchFilter=%6&"
						"sortField=%7&"
						"sortOrder=desc")
					.arg(apiBase(), QString::number(minecraftGameId()),
						 QString::number(modpackClassId()),
						 QString::number(offset),
						 QString::number(get().searchPageSize()),
						 ModPlatform::encodeSearchTerm(term),
						 ModPlatform::sortValueAt(
						     get().sortingMethods(), sortIndex)));
}

QUrl FlameApi::nameSearchUrl(const QString& encodedTerm, int limit)
{
	return QUrl(QString("%1/mods/search?"
						"gameId=%2&"
						"classId=%3&"
						"searchFilter=%4&"
						"sortField=2&"
						"sortOrder=desc&"
						"pageSize=%5")
					.arg(apiBase(), QString::number(minecraftGameId()),
						 QString::number(
							 ModPlatform::contentTypeToCurseForgeClassId(
								 ModPlatform::ContentType::Mod)),
						 encodedTerm, QString::number(limit)));
}

QString FlameApi::browserDownloadUrl(const QString& projectId,
									 const QString& fileId)
{
	return QString("https://%1/api/v1/mods/%2/files/%3/download")
		.arg(siteHost(), projectId, fileId);
}
