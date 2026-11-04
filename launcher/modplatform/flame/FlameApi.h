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

class FlameApi final : public ModPlatform::ContentApi
{
  public:
	static const FlameApi& get();

	/* Host serving the REST API. Net::Download attaches the x-api-key header
	 * to requests for exactly this host, so the two must agree. */
	static QString apiHost();
	static QString apiBase();
	/* Host serving the website, through which files whose author opted out
	 * of third-party downloads must be fetched. */
	static QString siteHost();

	static int minecraftGameId();
	/* Section id for modpacks; not a ContentType, as they create instances. */
	static int modpackClassId();

	QString id() const override;
	QString displayName() const override;
	int searchPageSize() const override;
	/* CurseForge's search indexes neither environment nor licence. */
	bool supportsExtendedFilters() const override
	{
		return false;
	}
	QList<ModPlatform::SortingMethod> sortingMethods() const override;

	QUrl searchUrl(const ModPlatform::SearchQuery& query) const override;
	QUrl projectUrl(const QString& projectId) const override;
	QUrl
	projectVersionsUrl(const ModPlatform::VersionQuery& query) const override;
	QUrl projectBodyUrl(const QString& projectId) const override;
	QUrl categoriesUrl(ModPlatform::ContentType contentType) const override;
	QUrl projectPageUrl(const QString& projectId) const override;

	static QUrl fileUrl(const QString& projectId, const QString& fileId);
	/* Several files by id: a POST of {"fileIds": [...]}, answered with `data`
	 * as an array. */
	static QUrl filesUrl();

	/* The versions owning a set of file fingerprints. A POST (the question
	 * is a list, see Net::JsonPost): CurseForge indexes files only by
	 * FlameFingerprint, so exports have no other route. Answers with
	 * `data.exactMatches`, each carrying the `file` it matched. */
	static QUrl matchFingerprintsUrl();

	/* Several projects by id; also a POST. Needed after a fingerprint match,
	 * which names a project by number only: the slug and authors a mod list
	 * prints are not in that answer. Answers with `data` as an array. */
	static QUrl projectsUrl();

	/* Modpack browsing: a different section, with no version or loader
	 * filter. */
	static QUrl modpackSearchUrl(const QString& term, int sortIndex,
								 int offset);

	/* Narrow mod-name lookup for a dependency not found on its own
	 * platform. `encodedTerm` must already be percent-encoded. */
	static QUrl nameSearchUrl(const QString& encodedTerm, int limit = 5);

	/* Browser download link for a file with no API download URL. */
	static QString browserDownloadUrl(const QString& projectId,
									  const QString& fileId);
};
