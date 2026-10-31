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
#include <QUrl>

#include "modplatform/ContentApi.h"

class ModrinthApi final : public ModPlatform::ContentApi
{
  public:
	static const ModrinthApi& get();

	static QString apiHost();
	static QString apiBase();

	/* The host Modrinth version files are served from; the only one the
	 * download-attribution header applies to. */
	static QString cdnHost();

	/* Every host an `.mrpack` manifest may name a download from. Wider than
	 * cdnHost() on purpose: the format permits these four, so a pack
	 * pointing at a GitHub release must not be flagged untrusted on import,
	 * and on export such a mod should be named rather than bundled. Shared
	 * by import and export so they agree: naming a URL we would refuse to
	 * install is worse. */
	static const QStringList& mrpackHosts();

	/* Whether @p url is served from one of mrpackHosts(). Host only: callers
	 * check the scheme themselves, as installing and naming differ. */
	static bool isMrpackHost(const QUrl& url);

	QString id() const override;
	QString displayName() const override;
	int searchPageSize() const override;
	bool supportsExtendedFilters() const override
	{
		return true;
	}
	QList<ModPlatform::SortingMethod> sortingMethods() const override;

	QUrl searchUrl(const ModPlatform::SearchQuery& query) const override;
	QUrl projectUrl(const QString& projectId) const override;
	QUrl
	projectVersionsUrl(const ModPlatform::VersionQuery& query) const override;
	QUrl projectBodyUrl(const QString& projectId) const override;
	QUrl categoriesUrl(ModPlatform::ContentType contentType) const override;
	QUrl projectPageUrl(const QString& projectId) const override;

	/* A version by its own id; ids are globally unique, so no project id. */
	static QUrl versionUrl(const QString& versionId);

	/* Whether @p candidate can be a Modrinth version or project id: base62,
	 * eight characters. A version *number* ("1.1.1+1.17", which a CDN URL
	 * spells out) is not, and asking the version endpoint for one gets a 404
	 * that looks like a dependency with nothing to resolve. */
	static bool isVersionId(const QString& candidate);

	/* The version that owns a file with this SHA-1. An mrpack manifest lists
	 * hashes and URLs but no version ids, so this is the way back for files
	 * installed from one. Answers like versionUrl(). */
	static QUrl versionByHashUrl(const QString& sha1);

	/* Many files at once: POST {"hashes": [...], "algorithm": "sha1"}; the
	 * reply maps each known hash to its version object. */
	static QUrl versionsByHashesUrl();

	/* Project versions for an explicit set of loaders (modpack browsing,
	 * where any loader will do). */
	static QUrl projectVersionsUrlForLoaders(const QString& projectId,
											 const QStringList& loaders);

	/* Modpack browsing. Separate from searchUrl(): modpacks are not a
	 * ContentType (they create instances), so this has its own, narrower
	 * filter set. @p gameVersion and @p loaders are optional (empty = any). */
	static QUrl
	modpackSearchUrl(const QString& term, int sortIndex, int offset,
					 const QString& gameVersion = QString(),
					 const QStringList& loaders = QStringList());

	/* Narrow mod-name lookup for a dependency not found on its own
	 * platform. `encodedTerm` must already be percent-encoded. */
	static QUrl nameSearchUrl(const QString& encodedTerm, int limit = 5);
};
