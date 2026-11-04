/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include "ShareContent.h"

#include <QHash>
#include <QVariantMap>

#include <algorithm>
#include <QJsonDocument>
#include <QJsonObject>

#include "modplatform/flame/FlameApi.h"

namespace ShareContent
{
	namespace
	{
		struct Project {
			QString name;
			QString slug;
		};

		QHash<QString, Project> projectsById(const QJsonArray& projects,
											 const char* titleKey)
		{
			QHash<QString, Project> out;
			for (const QJsonValue& value : projects) {
				const QJsonObject project = value.toObject();
				const QJsonValue id = project.value("id");
				const QString key =
					id.isString() ? id.toString() : QString::number(id.toInteger());
				out.insert(key, {project.value(titleKey).toString(),
								 project.value("slug").toString()});
			}
			return out;
		}

		/* A bare file name: the platform's answer decides where a file lands,
		 * so a name with a path in it is not taken. On Windows "C:x.jar" is a
		 * path too: QDir::filePath() hands it back as it is. */
		bool safeFileName(const QString& name)
		{
			return !name.isEmpty() && !name.contains(QLatin1Char('/')) &&
				   !name.contains(QLatin1Char('\\')) &&
				   !name.contains(QLatin1Char(':')) && name != QLatin1String(".") &&
				   name != QLatin1String("..");
		}

		/* The primary file, else the first; Modrinth marks at most one. */
		QJsonObject primaryFile(const QJsonArray& files)
		{
			for (const QJsonValue& file : files) {
				if (file.toObject().value("primary").toBool()) {
					return file.toObject();
				}
			}
			return files.isEmpty() ? QJsonObject() : files.first().toObject();
		}

		void miss(Resolved& resolved, const ShareCodes::Entry& entry,
				  const QHash<QString, Project>& names)
		{
			const QString name = names.value(entry.projectId).name;
			resolved.missing.append(entry);
			resolved.missingNames.append(name.isEmpty() ? entry.projectId : name);
		}

		ModPlatform::DownloadItem itemFor(const ShareCodes::Entry& entry,
										  const Project& project,
										  const QString& fallbackName)
		{
			ModPlatform::DownloadItem item;
			item.platform = entry.platform;
			item.projectId = entry.projectId;
			item.versionId = entry.fileId;
			item.slug = project.slug;
			item.name = !project.name.isEmpty() ? project.name : fallbackName;
			return item;
		}
	} // namespace

	QList<QList<ShareCodes::Entry>> batches(const QList<ShareCodes::Entry>& content,
											const QString& platform)
	{
		QList<QList<ShareCodes::Entry>> out;
		for (const ShareCodes::Entry& entry : content) {
			if (entry.platform != platform) {
				continue;
			}
			if (out.isEmpty() || out.last().size() >= kBatchSize) {
				out.append(QList<ShareCodes::Entry>());
			}
			out.last().append(entry);
		}
		return out;
	}

	QStringList projectIds(const QList<ShareCodes::Entry>& entries)
	{
		QStringList ids;
		for (const auto& entry : entries) {
			if (!ids.contains(entry.projectId)) {
				ids.append(entry.projectId);
			}
		}
		return ids;
	}

	QStringList fileIds(const QList<ShareCodes::Entry>& entries)
	{
		QStringList ids;
		for (const auto& entry : entries) {
			ids.append(entry.fileId);
		}
		return ids;
	}

	QByteArray curseForgeIdsBody(const QString& key, const QStringList& ids)
	{
		QJsonArray numbers;
		for (const QString& id : ids) {
			numbers.append(id.toLongLong());
		}
		return QJsonDocument(QJsonObject{{key, numbers}})
			.toJson(QJsonDocument::Compact);
	}

	void addModrinth(Resolved& resolved, const QList<ShareCodes::Entry>& wanted,
					 const QJsonArray& versions, const QJsonArray& projects)
	{
		const auto names = projectsById(projects, "title");
		QHash<QString, QJsonObject> byId;
		for (const QJsonValue& value : versions) {
			byId.insert(value.toObject().value("id").toString(), value.toObject());
		}
		for (const ShareCodes::Entry& entry : wanted) {
			const QJsonObject version = byId.value(entry.fileId);
			const QJsonObject file = primaryFile(version.value("files").toArray());
			const QString url = file.value("url").toString();
			/* The version must be the project's the code names. */
			if (url.isEmpty() || !safeFileName(file.value("filename").toString()) ||
				version.value("project_id").toString() != entry.projectId) {
				miss(resolved, entry, names);
				continue;
			}
			auto item = itemFor(entry, names.value(entry.projectId),
								version.value("name").toString());
			item.fileName = file.value("filename").toString();
			item.downloadUrl = url;
			item.sha1 = file.value("hashes").toObject().value("sha1").toString();
			item.fileSize = file.value("size").toInt();
			resolved.items.append({entry.kind, item});
		}
	}

	QVariantList contentNames(const QList<ShareCodes::Entry>& content,
							  const QJsonArray& modrinthProjects,
							  const QJsonArray& curseForgeMods)
	{
		static const QStringList kinds = {ShareCodes::Kind::Mod, ShareCodes::Kind::ResourcePack,
										  ShareCodes::Kind::ShaderPack, ShareCodes::Kind::DataPack};
		const auto modrinth = projectsById(modrinthProjects, "title");
		const auto curseForge = projectsById(curseForgeMods, "name");
		QList<QVariantMap> named;
		for (const ShareCodes::Entry& entry : content) {
			const auto& names =
				entry.platform == ShareCodes::Platform::Modrinth ? modrinth : curseForge;
			const QString name = names.value(entry.projectId).name;
			named.append({{"kind", entry.kind},
						  {"platform", entry.platform},
						  {"name", name.isEmpty() ? entry.projectId : name}});
		}
		std::stable_sort(named.begin(), named.end(), [](const QVariantMap& a, const QVariantMap& b) {
			const auto kindA = kinds.indexOf(a.value("kind").toString());
			const auto kindB = kinds.indexOf(b.value("kind").toString());
			return kindA != kindB ? kindA < kindB
								  : a.value("name").toString().compare(b.value("name").toString(),
																	   Qt::CaseInsensitive) < 0;
		});
		QVariantList out;
		for (const QVariantMap& one : named) {
			out.append(one);
		}
		return out;
	}

	void addCurseForge(Resolved& resolved,
					   const QList<ShareCodes::Entry>& wanted,
					   const QJsonArray& files, const QJsonArray& mods)
	{
		const auto names = projectsById(mods, "name");
		QHash<QString, QJsonObject> byId;
		for (const QJsonValue& value : files) {
			byId.insert(QString::number(value.toObject().value("id").toInteger()),
						value.toObject());
		}
		for (const ShareCodes::Entry& entry : wanted) {
			const QJsonObject file = byId.value(entry.fileId);
			const QString modId = QString::number(file.value("modId").toInteger());
			if (file.isEmpty() || modId != entry.projectId ||
				!safeFileName(file.value("fileName").toString())) {
				miss(resolved, entry, names);
				continue;
			}
			auto item = itemFor(entry, names.value(entry.projectId),
								file.value("displayName").toString());
			item.fileName = file.value("fileName").toString();
			item.downloadUrl = file.value("downloadUrl").toString();
			if (item.downloadUrl.isEmpty()) {
				item.downloadUrl =
					FlameApi::browserDownloadUrl(entry.projectId, entry.fileId);
				item.browserDownloadOnly = true;
			}
			item.sha1 = ModPlatform::curseForgeSha1FromFileObject(file);
			item.fileSize = file.value("fileLength").toInt();
			resolved.items.append({entry.kind, item});
		}
	}
} // namespace ShareContent
