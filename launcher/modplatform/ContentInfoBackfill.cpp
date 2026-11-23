/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-License-Identifier: Apache-2.0
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

#include "ContentInfoBackfill.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>

#include "core/LauncherContext.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "net/Download.h"
#include "net/JsonPost.h"

namespace
{
	const QString kModrinth = QStringLiteral("modrinth");
	const QString kCurseForge = QStringLiteral("curseforge");

	/* Ids per Modrinth request: they go in the URL. */
	constexpr int kPerRequest = 100;


	/* Ids go into a URL or JSON as they are; a sidecar from another tool could
	 * hold anything, and one bad id would fail the whole batch. */
	bool plainId(const QString& id)
	{
		if (id.isEmpty()) {
			return false;
		}
		for (const QChar c : id) {
			const bool ok = (c.unicode() < 128 && c.isLetterOrNumber()) ||
							c == QLatin1Char('-') || c == QLatin1Char('_');
			if (!ok) {
				return false;
			}
		}
		return true;
	}

	QJsonArray dataArray(const QByteArray& body)
	{
		const QJsonDocument doc = QJsonDocument::fromJson(body);
		return doc.isArray() ? doc.array()
							 : doc.object().value(QStringLiteral("data")).toArray();
	}

	QByteArray curseForgeBody(const QString& key, const QStringList& ids)
	{
		QJsonArray numbers;
		for (const QString& id : ids) {
			numbers.append(id.toLongLong());
		}
		return QJsonDocument(QJsonObject{{key, numbers}})
			.toJson(QJsonDocument::Compact);
	}

	void appendUnique(QStringList& list, const QString& id)
	{
		if (!list.contains(id)) {
			list.append(id);
		}
	}
} // namespace

bool ContentInfo::firstAskThisSession(const QString& what)
{
	static QSet<QString> asked;
	if (asked.contains(what)) {
		return false;
	}
	asked.insert(what);
	return true;
}

ContentInfo::Project ContentInfo::projectFrom(const QJsonObject& object)
{
	Project project;
	const bool modrinth = object.contains(QStringLiteral("title"));
	project.name = object.value(modrinth ? QStringLiteral("title")
										 : QStringLiteral("name"))
					   .toString();
	project.slug = object.value(QStringLiteral("slug")).toString();
	project.iconUrl = modrinth ? object.value(QStringLiteral("icon_url")).toString()
							   : object.value(QStringLiteral("logo"))
									 .toObject()
									 .value(QStringLiteral("thumbnailUrl"))
									 .toString();
	project.description =
		object.value(modrinth ? QStringLiteral("description")
							  : QStringLiteral("summary"))
			.toString()
			.trimmed();
	return project;
}

QString ContentInfo::idOf(const QJsonObject& object)
{
	const QJsonValue id = object.value(QStringLiteral("id"));
	if (id.isString()) {
		return id.toString();
	}
	const qint64 number = id.toInteger();
	return number > 0 ? QString::number(number) : QString();
}

QHash<QString, ContentInfo::Project>
ContentInfo::parseProjects(const QByteArray& body)
{
	QHash<QString, Project> out;
	for (const QJsonValue& value : dataArray(body)) {
		const QJsonObject object = value.toObject();
		const QString id = idOf(object);
		if (!id.isEmpty()) {
			out.insert(id, projectFrom(object));
		}
	}
	return out;
}

QHash<QString, QString> ContentInfo::parseVersionNumbers(const QByteArray& body)
{
	QHash<QString, QString> out;
	for (const QJsonValue& value : dataArray(body)) {
		const QJsonObject object = value.toObject();
		const QString id = idOf(object);
		const QString number =
			object.contains(QStringLiteral("version_number"))
				? object.value(QStringLiteral("version_number")).toString()
				: object.value(QStringLiteral("displayName")).toString();
		if (!id.isEmpty() && !number.isEmpty()) {
			out.insert(id, number);
		}
	}
	return out;
}

bool ContentInfo::needsInfo(const ModMetadataIndex::Entry& entry)
{
	if (!entry.hasPlatformOrigin() ||
		(entry.platform != kModrinth && entry.platform != kCurseForge)) {
		return false;
	}
	const bool noVersion =
		entry.versionNumber.isEmpty() && !entry.versionId.isEmpty();
	const bool noProject = entry.iconUrl.isEmpty() && entry.description.isEmpty();
	return noVersion || noProject;
}

bool ContentInfo::fill(ModMetadataIndex::Entry& entry,
					   const QHash<QString, Project>& projects,
					   const QHash<QString, QString>& versionNumbers)
{
	bool changed = false;
	const auto take = [&changed](QString& field, const QString& value) {
		if (field.isEmpty() && !value.isEmpty()) {
			field = value;
			changed = true;
		}
	};
	const auto project = projects.constFind(entry.projectId);
	if (project != projects.constEnd()) {
		take(entry.name, project->name);
		take(entry.iconUrl, project->iconUrl);
		take(entry.description, project->description);
	}
	if (!entry.versionId.isEmpty()) {
		take(entry.versionNumber, versionNumbers.value(entry.versionId));
	}
	return changed;
}

ContentInfoBackfill::ContentInfoBackfill(std::shared_ptr<ModMetadataIndex> index,
										 QObject* parent)
	: QObject(parent), m_index(std::move(index))
{
}

ContentInfoBackfill::~ContentInfoBackfill() = default;

void ContentInfoBackfill::start()
{
	QStringList mrProjects, mrVersions, cfProjects, cfFiles;
	if (m_index && LAUNCHER) {
		for (const ModMetadataIndex::Entry& entry : m_index->all()) {
			if (!ContentInfo::needsInfo(entry) || !plainId(entry.projectId)) {
				continue;
			}
			const QString asked = m_index->indexDir() + QLatin1Char('/') +
								  entry.fileName + QLatin1Char('@') +
								  entry.versionId;
			if (!ContentInfo::firstAskThisSession(asked)) {
				continue;
			}
			m_wanted.append(entry);

			const bool modrinth = entry.platform == kModrinth;
			appendUnique(modrinth ? mrProjects : cfProjects, entry.projectId);
			if (entry.versionNumber.isEmpty() && plainId(entry.versionId)) {
				appendUnique(modrinth ? mrVersions : cfFiles, entry.versionId);
			}
		}
	}
	if (m_wanted.isEmpty()) {
		deleteLater();
		return;
	}
	startModrinth(mrProjects, mrVersions);
	startCurseForge(cfProjects, cfFiles);
	if (m_pending == 0) {
		deleteLater();
	}
}

void ContentInfoBackfill::startModrinth(const QStringList& projects,
										const QStringList& versions)
{
	if (projects.isEmpty()) {
		return;
	}
	m_modrinthProjectParts = int((projects.size() + kPerRequest - 1) / kPerRequest);
	const int versionParts = int((versions.size() + kPerRequest - 1) / kPerRequest);
	m_modrinthBodies = QList<QByteArray>(m_modrinthProjectParts + versionParts);

	m_modrinthJob = NetJob::Ptr(
		new NetJob(QStringLiteral("Content info (Modrinth)"), LAUNCHER->network()));
	for (int part = 0; part < m_modrinthProjectParts; ++part) {
		m_modrinthJob->addNetAction(Net::Download::makeByteArray(
			ModrinthApi::projectsUrl(projects.mid(part * kPerRequest, kPerRequest)),
			&m_modrinthBodies[part]));
	}
	for (int part = 0; part < versionParts; ++part) {
		m_modrinthJob->addNetAction(Net::Download::makeByteArray(
			ModrinthApi::versionsUrl(versions.mid(part * kPerRequest, kPerRequest)),
			&m_modrinthBodies[m_modrinthProjectParts + part]));
	}
	connect(m_modrinthJob.get(), &NetJob::succeeded, this, [this] {
		for (int i = 0; i < m_modrinthBodies.size(); ++i) {
			if (i < m_modrinthProjectParts) {
				m_projects[kModrinth].insert(
					ContentInfo::parseProjects(m_modrinthBodies.at(i)));
			} else {
				m_versions[kModrinth].insert(
					ContentInfo::parseVersionNumbers(m_modrinthBodies.at(i)));
			}
		}
	});
	connect(m_modrinthJob.get(), &Task::finished, this,
			&ContentInfoBackfill::onePartDone);
	++m_pending;
	m_modrinthJob->start();
}

void ContentInfoBackfill::startCurseForge(const QStringList& projects,
										  const QStringList& files)
{
	if (projects.isEmpty()) {
		return;
	}
	m_cfProjects = std::make_unique<Net::JsonPost>(
		QStringLiteral("Content info (CurseForge)"), FlameApi::projectsUrl(),
		curseForgeBody(QStringLiteral("modIds"), projects));
	Net::JsonPost* post = m_cfProjects.get();
	connect(post, &Task::finished, this, [this, post] {
		if (post->wasSuccessful()) {
			m_projects[kCurseForge] = ContentInfo::parseProjects(post->response());
		}
		onePartDone();
	});
	++m_pending;
	post->start();

	if (files.isEmpty()) {
		return;
	}
	m_cfFiles = std::make_unique<Net::JsonPost>(
		QStringLiteral("Content info (CurseForge)"), FlameApi::filesUrl(),
		curseForgeBody(QStringLiteral("fileIds"), files));
	Net::JsonPost* filesPost = m_cfFiles.get();
	connect(filesPost, &Task::finished, this, [this, filesPost] {
		if (filesPost->wasSuccessful()) {
			m_versions[kCurseForge] =
				ContentInfo::parseVersionNumbers(filesPost->response());
		}
		onePartDone();
	});
	++m_pending;
	filesPost->start();
}

void ContentInfoBackfill::onePartDone()
{
	if (--m_pending > 0) {
		return;
	}
	apply();
	deleteLater();
}

void ContentInfoBackfill::apply()
{
	QStringList keys;
	for (const ModMetadataIndex::Entry& asked : m_wanted) {
		/* Read again: the file may have been updated or removed meanwhile, and
		 * an answer about another version must not land in its sidecar. */
		ModMetadataIndex::Entry entry = m_index->get(asked.fileName);
		if (entry.platform != asked.platform ||
			entry.projectId != asked.projectId ||
			entry.versionId != asked.versionId) {
			continue;
		}
		if (ContentInfo::fill(entry, m_projects.value(entry.platform),
							  m_versions.value(entry.platform)) &&
			m_index->put(entry)) {
			keys.append(ModMetadataIndex::canonicalFileName(entry.fileName));
		}
	}
	if (!keys.isEmpty()) {
		emit filled(keys);
	}
}
