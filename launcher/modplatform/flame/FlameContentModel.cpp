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

#include "FlameContentModel.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

#include "Json.h"
#include "modplatform/ModDownloadTypes.h"
#include "modplatform/VersionPicker.h"
#include "modplatform/flame/FlameApi.h"

namespace
{

	/* CurseForge's releaseType, as used by the version box. */
	QString releaseTypeName(int releaseType)
	{
		switch (releaseType) {
			case 1:
				return QStringLiteral("release");
			case 2:
				return QStringLiteral("beta");
			case 3:
				return QStringLiteral("alpha");
			default:
				return QString();
		}
	}

	/* One CurseForge mod object into one result.
	 *
	 * Shared by the search and the single-project lookup on purpose:
	 * /mods/search and /mods/{id} hand out the same object, and when
	 * these were two copies of the same twenty lines only one of them
	 * learned about the "links" wrapper. Returns false when the object
	 * is missing something a row cannot do without. */
	bool parseProjectObject(const QJsonObject& obj,
							ModPlatform::IndexedProject& project)
	{
		try {
			project.projectId =
				QString::number(Json::requireInteger(obj, "id"));
			project.slug = Json::ensureString(obj, "slug", "");
			project.name = Json::requireString(obj, "name");
			project.description = Json::ensureString(obj, "summary", "");

			if (obj.value("links").isObject()) {
				project.websiteUrl = Json::ensureString(
					obj.value("links").toObject(), "websiteUrl", "");
			} else {
				project.websiteUrl = Json::ensureString(obj, "websiteUrl", "");
			}

			if (obj.value("logo").isObject()) {
				const auto logoObj = obj.value("logo").toObject();
				project.logoKey =
					Json::ensureString(logoObj, "title", project.name);
				project.logoUrl =
					Json::ensureString(logoObj, "thumbnailUrl", "");
			} else {
				project.logoKey = project.name;
			}

			QStringList authorNames;
			for (const auto& authorRaw : Json::ensureArray(obj, "authors")) {
				authorNames.append(
					Json::ensureString(authorRaw.toObject(), "name", ""));
			}
			project.author = authorNames.join(", ");
		} catch (const JSONValidationError& e) {
			qWarning() << "Error loading CurseForge project:" << e.cause();
			return false;
		}
		return true;
	}

} // namespace

FlameContentModel::FlameContentModel(ModPlatform::ContentType contentType,
									 ModPlatform::SearchFilters filters,
									 QObject* parent)
	: ContentProviderModel(FlameApi::get(), contentType, std::move(filters),
						   parent)
{
}

QString FlameContentModel::iconCacheName() const
{
	return QStringLiteral("FlameModIcons");
}

QList<ModPlatform::Category>
FlameContentModel::parseCategoriesResponse(const QByteArray& bytes) const
{
	QList<ModPlatform::Category> categories;

	QJsonParseError parseError{};
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Error parsing CurseForge category response:"
				   << parseError.errorString();
		return categories;
	}

	const QJsonArray entries = doc.object().value("data").toArray();
	for (const auto& entryRaw : entries) {
		const auto obj = entryRaw.toObject();
		try {
			/* Numeric here, and the search wants it back as a bare
			 * number, so it is kept as the string form of the id. */
			categories.append(
				{Json::requireString(obj, "name"),
				 QString::number(Json::requireInteger(obj, "id"))});
		} catch (const Json::JsonException& e) {
			qWarning() << "Skipping malformed CurseForge category:"
					   << e.what();
		}
	}

	return categories;
}

QList<ModPlatform::IndexedProject>
FlameContentModel::parseSearchResponse(const QByteArray& bytes,
									   int& totalHits) const
{
	QList<ModPlatform::IndexedProject> results;

	QJsonParseError parseError{};
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Error parsing CurseForge search response:"
				   << parseError.errorString();
		return results;
	}

	QJsonArray entries;
	if (doc.isObject() && doc.object().contains("data")) {
		entries = doc.object().value("data").toArray();
	} else {
		entries = doc.array();
	}

	for (const auto& entryRaw : entries) {
		ModPlatform::IndexedProject project;
		if (parseProjectObject(entryRaw.toObject(), project)) {
			results.append(project);
		}
	}

	const auto pagination = doc.object().value("pagination").toObject();
	totalHits = Json::ensureInteger(pagination, "totalCount", -1);

	return results;
}

ModPlatform::IndexedProject
FlameContentModel::parseProjectResponse(const QByteArray& bytes) const
{
	ModPlatform::IndexedProject project;

	QJsonParseError parseError{};
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Error parsing CurseForge project response:"
				   << parseError.errorString();
		return project;
	}

	/* /mods/{id} wraps the mod in "data"; take a bare object too, since
	 * that is what a cached or proxied reply sometimes is. */
	const auto root = doc.object();
	const QJsonObject modObj = root.value("data").isObject()
								   ? root.value("data").toObject()
								   : root;

	if (!parseProjectObject(modObj, project)) {
		return ModPlatform::IndexedProject();
	}
	return project;
}

QList<ModPlatform::ContentVersion> FlameContentModel::parseVersionsResponse(
	const QByteArray& bytes, const ModPlatform::IndexedProject& project) const
{
	QList<ModPlatform::ContentVersion> versions;

	const QJsonDocument doc = QJsonDocument::fromJson(bytes);
	QJsonArray files;
	if (doc.isObject() && doc.object().contains("data")) {
		files = doc.object().value("data").toArray();
	} else {
		files = doc.array();
	}

	for (const auto& fileRaw : files) {
		const auto fileObj = fileRaw.toObject();

		ModPlatform::ContentVersion version;
		version.name = Json::ensureString(fileObj, "displayName", "");
		version.fileName = Json::ensureString(fileObj, "fileName", "");
		version.versionId =
			QString::number(Json::ensureInteger(fileObj, "id", 0));
		version.downloadUrl = Json::ensureString(fileObj, "downloadUrl", "");
		version.sha1 = ModPlatform::curseForgeSha1FromFileObject(fileObj);
		version.fileSize = Json::ensureInteger(fileObj, "fileLength", 0);
		version.versionType =
			releaseTypeName(Json::ensureInteger(fileObj, "releaseType", 0));
		version.datePublished = Json::ensureString(fileObj, "fileDate", "");

		/* CurseForge mixes Minecraft versions and loader names into one
		 * "gameVersions" array - see ModPlatform::newestCurseForgeFile(),
		 * which has to untangle the same thing for the same reason. */
		for (const auto& tagRaw : Json::ensureArray(fileObj, "gameVersions")) {
			const QString tag = tagRaw.toString();
			if (ModPlatform::isKnownLoaderName(tag)) {
				version.loaders.append(tag);
			} else {
				version.gameVersions.append(tag);
			}
		}

		/* Authors can forbid third-party downloads, in which case the
		 * API hands out no URL at all. The site still serves the file,
		 * so point at that instead of dropping the version. */
		if (version.downloadUrl.isEmpty() && !version.versionId.isEmpty() &&
			!version.fileName.isEmpty()) {
			version.downloadUrl = FlameApi::browserDownloadUrl(
				project.projectId, version.versionId);
			version.browserDownloadOnly = true;
		}

		if (version.name.isEmpty()) {
			version.name = version.fileName;
		}

		versions.append(version);
	}

	return versions;
}

QString FlameContentModel::parseBodyResponse(const QByteArray& bytes) const
{
	/* CurseForge serves the description as ready-made HTML. */
	const QJsonDocument doc = QJsonDocument::fromJson(bytes);
	if (!doc.isObject()) {
		return QString();
	}
	return Json::ensureString(doc.object(), "data", "");
}
