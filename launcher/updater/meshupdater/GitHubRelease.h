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

#include <QDateTime>
#include <QDebug>
#include <QList>
#include <QString>
#include <QUrl>

#include "Version.h"

/*! One downloadable file attached to a release: only the GitHub releases API fields the updater
 * uses; the rest is dropped at parse time so nothing downstream can depend on it. */
struct GitHubReleaseAsset {
	qint64 id = -1;
	QString name;
	QString label;
	QString contentType;
	qint64 size = 0;
	QDateTime createdAt;
	QDateTime updatedAt;
	QUrl downloadUrl;

	/*! Usable when the API gave an identity and a download URL; checked here so an incomplete asset
	 * is rejected while alternatives remain. */
	bool isValid() const
	{
		return id > 0 && downloadUrl.isValid() && !downloadUrl.isEmpty();
	}
};

/*! A GitHub release plus the parsed form of its tag. \a version is derived from \a tagName once, so
 * every comparison goes through one parse and two code paths cannot disagree about which release is
 * newer. */
struct GitHubRelease {
	qint64 id = -1;
	QString name;
	QString tagName;
	QDateTime createdAt;
	QDateTime publishedAt;
	bool prerelease = false;
	bool draft = false;
	QString body;
	QList<GitHubReleaseAsset> assets;
	Version version;

	bool isValid() const
	{
		return id > 0;
	}

	//! What to show the user: the release name, or the tag when it has none.
	QString displayName() const
	{
		return name.isEmpty() ? tagName : name;
	}
};

namespace GitHub
{

	/*! Strips the leading "v" from a release tag: tags are "v7.19.0" while the launcher reports
	 * "7.19.0", and "v7" has no leading digits, so Version would fall back to string comparison.
	 * Normalise once, where a tag becomes a Version. */
	QString normalizeVersionTag(const QString& tag);

	/*! Parses one page of `GET /repos/{owner}/{repo}/releases`, appending to \a releases and
	 * returning the entry count (a short page is the last). Returns -1 and fills \a error when the
	 * response is not a release array (rate-limit message, HTML error page, truncated body). One
	 * malformed entry is skipped with a warning so one bad release cannot make the launcher
	 * un-updatable. */
	int parseReleasePage(const QByteArray& response,
						 QList<GitHubRelease>* releases, QString* error);

	/*! Turns a repository URL ("https://github.com/owner/repo", with or without ".git" or a trailing
	 * slash) into the releases endpoint. Returns an empty string, reported by the caller, for
	 * anything that is not a GitHub repository URL. */
	QString releasesApiUrl(const QUrl& repositoryUrl, QString* error);

} // namespace GitHub

QDebug operator<<(QDebug debug, const GitHubReleaseAsset& asset);
QDebug operator<<(QDebug debug, const GitHubRelease& release);
