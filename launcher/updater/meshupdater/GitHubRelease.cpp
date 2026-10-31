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

#include "GitHubRelease.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

namespace
{

	QDateTime parseTimestamp(const QJsonValue& value)
	{
		// GitHub timestamps are ISO 8601 with a Z suffix. An unparsable one stays invalid rather
		// than defaulting to the epoch, which would be displayed as fact.
		return QDateTime::fromString(value.toString(), Qt::ISODate);
	}

	GitHubReleaseAsset parseAsset(const QJsonObject& object)
	{
		GitHubReleaseAsset asset;
		asset.id = static_cast<qint64>(object.value("id").toDouble(-1));
		asset.name = object.value("name").toString();
		asset.label = object.value("label").toString();
		asset.contentType = object.value("content_type").toString();
		asset.size = static_cast<qint64>(object.value("size").toDouble(0));
		asset.createdAt = parseTimestamp(object.value("created_at"));
		asset.updatedAt = parseTimestamp(object.value("updated_at"));
		asset.downloadUrl =
			QUrl(object.value("browser_download_url").toString());
		return asset;
	}

} // namespace

QString GitHub::normalizeVersionTag(const QString& tag)
{
	QString normalized = tag.trimmed();
	if (normalized.startsWith(QLatin1Char('v')) ||
		normalized.startsWith(QLatin1Char('V'))) {
		normalized.remove(0, 1);
	}
	return normalized;
}

int GitHub::parseReleasePage(const QByteArray& response,
							 QList<GitHubRelease>* releases, QString* error)
{
	Q_ASSERT(releases);
	Q_ASSERT(error);

	error->clear();

	if (response.isEmpty()) {
		// An empty body ends a paged listing but is also what a dropped connection leaves; treated
		// as "no more releases" either way.
		return 0;
	}

	QJsonParseError parseError{};
	const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		*error = QStringLiteral("%1 (at offset %2)")
					 .arg(parseError.errorString())
					 .arg(parseError.offset);
		return -1;
	}

	if (!document.isArray()) {
		// A rate limit is a valid JSON object with a "message" field; surface it.
		if (document.isObject()) {
			const QString message =
				document.object().value("message").toString();
			if (!message.isEmpty()) {
				*error = message;
				return -1;
			}
		}
		*error = QStringLiteral("the response is not a list of releases");
		return -1;
	}

	int found = 0;
	const QJsonArray array = document.array();
	for (const QJsonValue& entry : array) {
		++found;

		if (!entry.isObject()) {
			qWarning() << "Skipping a release entry that is not an object.";
			continue;
		}
		const QJsonObject object = entry.toObject();

		GitHubRelease release;
		release.id = static_cast<qint64>(object.value("id").toDouble(-1));
		release.tagName = object.value("tag_name").toString();

		if (release.id <= 0 || release.tagName.isEmpty()) {
			qWarning() << "Skipping a release with no id or no tag.";
			continue;
		}

		release.name = object.value("name").toString();
		release.createdAt = parseTimestamp(object.value("created_at"));
		release.publishedAt = parseTimestamp(object.value("published_at"));
		release.draft = object.value("draft").toBool(false);
		release.prerelease = object.value("prerelease").toBool(false);
		release.body = object.value("body").toString();
		release.version = Version(normalizeVersionTag(release.tagName));

		const QJsonArray assets = object.value("assets").toArray();
		for (const QJsonValue& assetEntry : assets) {
			if (!assetEntry.isObject())
				continue;
			const GitHubReleaseAsset asset = parseAsset(assetEntry.toObject());
			if (!asset.isValid()) {
				qWarning() << "Skipping an incomplete asset of"
						   << release.tagName << ":" << asset.name;
				continue;
			}
			release.assets.append(asset);
		}

		releases->append(release);
	}

	return found;
}

QString GitHub::releasesApiUrl(const QUrl& repositoryUrl, QString* error)
{
	Q_ASSERT(error);
	error->clear();

	if (!repositoryUrl.isValid()) {
		*error = QStringLiteral("'%1' is not a valid URL")
					 .arg(repositoryUrl.toString());
		return {};
	}

	// Only github.com: the API shape is GitHub's, so other hosts would produce unanswerable
	// requests.
	const QString host = repositoryUrl.host().toLower();
	if (host != QLatin1String("github.com") &&
		host != QLatin1String("www.github.com")) {
		*error = QStringLiteral("updating from '%1' is not supported; only "
								"github.com repositories are")
					 .arg(repositoryUrl.toString());
		return {};
	}

	QString path = repositoryUrl.path();
	if (path.endsWith(QLatin1Char('/')))
		path.chop(1);
	if (path.endsWith(QLatin1String(".git"), Qt::CaseInsensitive))
		path.chop(4);

	const QStringList segments =
		path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
	if (segments.size() < 2) {
		*error = QStringLiteral("'%1' does not name a repository")
					 .arg(repositoryUrl.toString());
		return {};
	}

	return QStringLiteral("https://api.github.com/repos/%1/%2/releases")
		.arg(segments.at(0), segments.at(1));
}

QDebug operator<<(QDebug debug, const GitHubReleaseAsset& asset)
{
	QDebugStateSaver saver(debug);
	debug.nospace() << "GitHubReleaseAsset(" << asset.name << ", id "
					<< asset.id << ", " << asset.size << " bytes, "
					<< asset.contentType << ", " << asset.downloadUrl << ")";
	return debug;
}

QDebug operator<<(QDebug debug, const GitHubRelease& release)
{
	QDebugStateSaver saver(debug);
	debug.nospace() << "GitHubRelease(" << release.tagName << ", id "
					<< release.id << ", version " << release.version.toString()
					<< (release.draft ? ", draft" : "")
					<< (release.prerelease ? ", prerelease" : "")
					<< ", published " << release.publishedAt.toString(Qt::ISODate)
					<< ", " << release.assets.size() << " assets)";
	return debug;
}
