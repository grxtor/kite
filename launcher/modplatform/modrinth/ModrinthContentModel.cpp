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

#include "ModrinthContentModel.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <utility>

#include "HoeDown.h"
#include "Json.h"
#include "modplatform/modrinth/ModrinthApi.h"

namespace
{

	/* One Modrinth project object into one result. A search hit and
	 * /project/{id} share field names, except that the hit calls the id
	 * "project_id" and carries an "author". False when the object lacks
	 * something a row needs. */
	bool parseProjectObject(const QJsonObject& obj,
							ModPlatform::IndexedProject& project)
	{
		try {
			project.projectId = Json::ensureString(obj, "project_id", "");
			if (project.projectId.isEmpty()) {
				project.projectId = Json::requireString(obj, "id");
			}
			project.slug = Json::ensureString(obj, "slug", "");
			project.name = Json::requireString(obj, "title");
			project.description = Json::ensureString(obj, "description", "");
			project.author = Json::ensureString(obj, "author", "");
			project.logoUrl = Json::ensureString(obj, "icon_url", "");
			/* The slug is a stable, filesystem-safe cache key; the title is not. */
			project.logoKey =
				project.slug.isEmpty() ? project.projectId : project.slug;

			if (!project.slug.isEmpty()) {
				project.websiteUrl = ModrinthApi::siteProjectUrl(project.slug);
			}
		} catch (const JSONValidationError& e) {
			qWarning() << "Error loading Modrinth project:" << e.cause();
			return false;
		}
		return true;
	}

} // namespace

ModrinthContentModel::ModrinthContentModel(
	ModPlatform::ContentType contentType, ModPlatform::SearchFilters filters,
	QObject* parent)
	: ContentProviderModel(ModrinthApi::get(), contentType, std::move(filters),
						   parent)
{
}

QList<ModPlatform::Category>
ModrinthContentModel::parseCategoriesResponse(const QByteArray& bytes) const
{
	QList<ModPlatform::Category> categories;

	const auto doc =
		Json::tryParse(bytes, "Error parsing Modrinth category response:");
	if (!doc) {
		return categories;
	}

	/* One list covers the whole site, so other content kinds are dropped. */
	const QString wanted =
		ModPlatform::contentTypeToModrinthFacet(contentType());

	const QJsonArray entries = doc->array();
	for (const auto& entryRaw : entries) {
		const auto obj = entryRaw.toObject();
		if (Json::ensureString(obj, "project_type", "") != wanted) {
			continue;
		}
		try {
			const QString name = Json::requireString(obj, "name");
			categories.append({name, name});
		} catch (const Json::JsonException& e) {
			qWarning() << "Skipping malformed Modrinth category:" << e.what();
		}
	}

	return categories;
}

QList<ModPlatform::IndexedProject>
ModrinthContentModel::parseSearchResponse(const QByteArray& bytes,
										  int& totalHits) const
{
	QList<ModPlatform::IndexedProject> results;

	const auto doc =
		Json::tryParse(bytes, "Error parsing Modrinth search response:");
	if (!doc) {
		return results;
	}

	const auto root = doc->object();
	for (const auto& hitRaw : Json::ensureArray(root, "hits")) {
		ModPlatform::IndexedProject project;
		if (parseProjectObject(hitRaw.toObject(), project)) {
			results.append(project);
		}
	}

	totalHits = Json::ensureInteger(root, "total_hits", -1);

	return results;
}

ModPlatform::IndexedProject
ModrinthContentModel::parseProjectResponse(const QByteArray& bytes) const
{
	ModPlatform::IndexedProject project;

	const auto doc =
		Json::tryParse(bytes, "Error parsing Modrinth project response:");
	if (!doc) {
		return project;
	}
	if (!doc->isObject()) {
		return project;
	}

	if (!parseProjectObject(doc->object(), project)) {
		return ModPlatform::IndexedProject();
	}
	return project;
}

QList<ModPlatform::ContentVersion> ModrinthContentModel::parseVersionsResponse(
	const QByteArray& bytes, const ModPlatform::IndexedProject& project) const
{
	Q_UNUSED(project)

	QList<ModPlatform::ContentVersion> versions;

	const QJsonDocument doc = QJsonDocument::fromJson(bytes);
	if (!doc.isArray()) {
		return versions;
	}

	for (const auto& versionRaw : doc.array()) {
		const auto versionObj = versionRaw.toObject();

		ModPlatform::ContentVersion version;
		version.versionId = Json::ensureString(versionObj, "id", "");
		version.versionType =
			Json::ensureString(versionObj, "version_type", "");

		const QString name = Json::ensureString(versionObj, "name", "");
		const QString number =
			Json::ensureString(versionObj, "version_number", "");
		version.versionNumber = number;
		version.name =
			name.isEmpty() ? number
						   : (number.isEmpty()
								  ? name
								  : QString("%1 (%2)").arg(name, number));

		for (const auto& tag :
			 Json::ensureArray(versionObj, "game_versions")) {
			version.gameVersions.append(tag.toString());
		}
		for (const auto& tag : Json::ensureArray(versionObj, "loaders")) {
			version.loaders.append(tag.toString());
		}
		version.datePublished =
			Json::ensureString(versionObj, "date_published", "");

		const auto files = Json::ensureArray(versionObj, "files");
		for (const auto& fileRaw : files) {
			const auto fileObj = fileRaw.toObject();
			const bool primary = Json::ensureBoolean(fileObj, QStringLiteral("primary"), false);
			if (!primary && files.size() != 1) {
				continue;
			}
			version.downloadUrl = Json::ensureString(fileObj, "url", "");
			version.fileName = Json::ensureString(fileObj, "filename", "");
			version.fileSize = Json::ensureInteger(fileObj, "size", 0);
			version.sha1 = Json::ensureString(
				Json::ensureObject(fileObj, "hashes"), "sha1", "");
			break;
		}

		if (version.downloadUrl.isEmpty()) {
			continue;
		}

		versions.append(version);
	}

	return versions;
}

QString ModrinthContentModel::parseBodyResponse(const QByteArray& bytes) const
{
	/* Modrinth hands out Markdown, rendered here so the page shows both
	 * providers' descriptions in the same browser. */
	const QJsonDocument doc = QJsonDocument::fromJson(bytes);
	if (!doc.isObject()) {
		return QString();
	}

	const QString body = Json::ensureString(doc.object(), "body", "");
	if (body.isEmpty()) {
		return QString();
	}

	HoeDown renderer;
	return renderer.process(body.toUtf8());
}
