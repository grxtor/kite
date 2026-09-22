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

#include <QString>
#include <QStringList>
#include <QUrl>

#include "modplatform/ContentApi.h"

/* URL builder for the Modrinth API. */
class ModrinthApi final : public ModPlatform::ContentApi
{
  public:
	static const ModrinthApi& get();

	static QString apiHost();
	/* Scheme + host + version prefix, no trailing slash. */
	static QString apiBase();

	/* The host every Modrinth version file is served from. A different
	 * host from the API, and the only one the download-attribution
	 * header means anything to. */
	static QString cdnHost();

	/*
	 * Every host an `.mrpack` manifest is allowed to name a download
	 * from.
	 *
	 * Wider than cdnHost() on purpose, and not a matter of taste: the
	 * Modrinth modpack format permits these four, and Modrinth's own
	 * validation accepts a pack that names them. Treating the CDN as the
	 * only one has consequences in both directions - a conformant pack
	 * that points at a GitHub release gets flagged as untrusted on
	 * import, and on export a mod installed from one gets bundled into
	 * `overrides/` instead of being named, which is both larger and
	 * something Modrinth rejects for a published pack.
	 *
	 * Kept here rather than beside either user because the two have to
	 * agree: naming a URL we would then refuse to install would be
	 * worse than either mistake on its own.
	 */
	static const QStringList& mrpackHosts();

	/* Whether @p url is served from one of mrpackHosts(). Host-only -
	 * callers that care about the scheme check it themselves, because
	 * what counts as acceptable there differs between installing a file
	 * and merely naming one. */
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

	/* A single version addressed by its own id. Modrinth version ids are
	 * globally unique, so unlike CurseForge this needs no project id. */
	static QUrl versionUrl(const QString& versionId);

	/* Whether @p candidate can be a Modrinth version or project id.
	 *
	 * They are base62 and eight characters long, which is exactly what a
	 * version *number* is not: numbers carry dots, plus signs and loader
	 * names ("1.1.1+1.17"). The distinction matters because the two are
	 * easy to confuse - a file's CDN URL spells out the number, not the
	 * id - and asking the version endpoint for a number gets a 404 that
	 * looks like a dependency with nothing to resolve. */
	static bool isVersionId(const QString& candidate);

	/* The version that owns a file with the given SHA-1.
	 *
	 * The way back to a version when all we kept was the file: an mrpack
	 * manifest lists hashes and download URLs and no version ids at all,
	 * so for everything installed from one this is the only honest
	 * lookup. Answers with the same version object as versionUrl(). */
	static QUrl versionByHashUrl(const QString& sha1);

	/* Project versions restricted to an explicit set of loaders. Used
	 * for modpack browsing, where any of the mod loaders will do. */
	static QUrl projectVersionsUrlForLoaders(const QString& projectId,
											 const QStringList& loaders);

	/* Modpack browsing. Separate from searchUrl() because modpacks are
	 * not a ContentType - they create instances rather than being
	 * installed into one, so this takes its own, narrower filter set
	 * rather than the mod-oriented SearchFilters.
	 *
	 * @p gameVersion and @p loaders are optional (empty means "any") and
	 * additive to the existing three-argument form: every call site that
	 * predates them keeps compiling and keeps its old behaviour. */
	static QUrl
	modpackSearchUrl(const QString& term, int sortIndex, int offset,
					 const QString& gameVersion = QString(),
					 const QStringList& loaders = QStringList());

	/* Narrow mod-name lookup, used when a dependency could not be
	 * resolved on its own platform and we go looking for it here.
	 * `encodedTerm` must already be percent-encoded. */
	static QUrl nameSearchUrl(const QString& encodedTerm, int limit = 5);
};
