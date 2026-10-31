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

#include "PerformanceSetupRules.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include "minecraft/mod/ModMetadataIndex.h"

namespace
{
	/* "1.16" is the first release Sodium and friends are built for. */
	constexpr int kFirstSupportedMinor = 16;
	/* From "26.1" on, versions are numbered by year: the first part is
	 * no longer 1. */
	constexpr int kFirstNumberedMajor = 2;
	const QString kProjectSeparator = QStringLiteral(",");
	/* What ModMetadataIndex records for a file installed from Modrinth
	 * (ModrinthApi::id()). */
	const QString kModrinthPlatform = QStringLiteral("modrinth");
	/* Between the Minecraft version and the loader in a match key. */
	const QString kMatchKeySeparator = QStringLiteral(" ");
	/* Between the fields of a missing record: not in a match key (a version, a
	 * space, a loader) and not in a project id. */
	const QString kRecordSeparator = QStringLiteral("|");
	constexpr int kRecordFields = 3;
	constexpr int kRecordKeyField = 0;
	constexpr int kRecordTimeField = 1;
	constexpr int kRecordProjectsField = 2;
	/* How long a mod Modrinth has no build of is left alone before it is
	 * asked about again. */
	constexpr qint64 kMissingForSecs = 24 * 60 * 60;

	bool fromModrinth(const ModMetadataIndex::Entry& entry)
	{
		return entry.platform == kModrinthPlatform && !entry.projectId.isEmpty();
	}

	QString sha1Of(const QString& path)
	{
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return QString();
		}
		QCryptographicHash hash(QCryptographicHash::Sha1);
		hash.addData(&file);
		return QString::fromLatin1(hash.result().toHex());
	}
} // namespace

namespace PerformanceSetup
{
	QStringList wantedProjects(const QStringList& defaults,
							   const QStringList& excluded,
							   const QStringList& installed)
	{
		QStringList wanted;
		for (const QString& project : defaults) {
			if (!project.isEmpty() && !excluded.contains(project) &&
				!installed.contains(project) && !wanted.contains(project)) {
				wanted.append(project);
			}
		}
		return wanted;
	}

	bool hasLoader(const QString& loader)
	{
		return !loader.isEmpty();
	}

	bool supportsVersion(const QString& mcVersion)
	{
		static const QRegularExpression release(
			QStringLiteral("^(\\d+)\\.(\\d+)(\\.\\d+)?$"));
		const QRegularExpressionMatch match = release.match(mcVersion);
		if (!match.hasMatch()) {
			return false;
		}
		const int major = match.captured(1).toInt();
		if (major >= kFirstNumberedMajor) {
			return true;
		}
		return major == 1 && match.captured(2).toInt() >= kFirstSupportedMinor;
	}

	QStringList jarsNotFromModrinth(const QDir& folder,
									const ModMetadataIndex& index)
	{
		QStringList unknown;
		const QStringList jars = folder.entryList(
			{QStringLiteral("*.jar"), QStringLiteral("*.jar.disabled")},
			QDir::Files | QDir::Readable, QDir::Name);
		for (const QString& jar : jars) {
			if (!fromModrinth(index.get(jar))) {
				unknown.append(jar);
			}
		}
		return unknown;
	}

	QStringList sha1sOf(const QDir& folder, const QStringList& files)
	{
		QStringList hashes;
		for (const QString& file : files) {
			const QString hash = sha1Of(folder.absoluteFilePath(file));
			if (!hash.isEmpty()) {
				hashes.append(hash);
			}
		}
		return hashes;
	}

	QByteArray hashLookupBody(const QStringList& sha1s)
	{
		QJsonObject body;
		body["hashes"] = QJsonArray::fromStringList(sha1s);
		body["algorithm"] = QStringLiteral("sha1");
		return QJsonDocument(body).toJson(QJsonDocument::Compact);
	}

	QStringList projectsInHashReply(const QByteArray& reply)
	{
		QStringList projects;
		const QJsonObject versions = QJsonDocument::fromJson(reply).object();
		for (const QJsonValue& version : versions) {
			const QString project =
				version.toObject().value("project_id").toString();
			if (!project.isEmpty() && !projects.contains(project)) {
				projects.append(project);
			}
		}
		return projects;
	}

	QStringList parseProjects(const QString& stored)
	{
		return stored.split(kProjectSeparator, Qt::SkipEmptyParts);
	}

	QString joinProjects(const QStringList& projects)
	{
		return projects.join(kProjectSeparator);
	}

	QStringList defaultProjects(const QVariantList& catalogMods)
	{
		QStringList defaults;
		for (const QVariant& mod : catalogMods) {
			const QVariantMap fields = mod.toMap();
			if (fields.value("isDefault").toBool()) {
				defaults.append(fields.value("project").toString());
			}
		}
		return defaults;
	}

	QString matchKey(const QString& mcVersion, const QString& loader)
	{
		return mcVersion + kMatchKeySeparator + loader;
	}

	QHash<QString, QString>
	filesToRematch(const QList<ModMetadataIndex::Entry>& entries,
				   const QStringList& catalogProjects)
	{
		QHash<QString, QString> files;
		for (const ModMetadataIndex::Entry& entry : entries) {
			const bool fromCatalog = catalogProjects.contains(entry.projectId);
			if (fromModrinth(entry) && (fromCatalog || entry.isDependency)) {
				files.insert(entry.projectId, entry.fileName);
			}
		}
		return files;
	}

	QStringList withRemoved(const QStringList& excluded,
							const QStringList& removed,
							const QStringList& defaults)
	{
		QStringList result = excluded;
		for (const QString& project : removed) {
			if (defaults.contains(project)) {
				result.append(project);
			}
		}
		result.removeDuplicates();
		return result;
	}

	QStringList withoutRemoved(const QStringList& included,
							   const QStringList& removed)
	{
		QStringList kept;
		for (const QString& project : included) {
			if (!removed.contains(project)) {
				kept.append(project);
			}
		}
		return kept;
	}

	QStringList commonProjects(const QStringList& projects,
							   const QStringList& among)
	{
		QStringList common;
		for (const QString& project : projects) {
			if (among.contains(project)) {
				common.append(project);
			}
		}
		return common;
	}

	QStringList installedNames(const ModMetadataIndex& index,
							   const QStringList& projects)
	{
		QStringList names;
		for (const QString& project : projects) {
			const ModMetadataIndex::Entry entry =
				index.findByPlatformProject(kModrinthPlatform, project);
			if (entry.isValid()) {
				names.append(entry.name.isEmpty() ? entry.fileName : entry.name);
			}
		}
		return names;
	}

	QString missingRecord(const QString& matchKey, const QDateTime& when,
						  const QStringList& projects)
	{
		return QStringList{matchKey, QString::number(when.toSecsSinceEpoch()),
						   joinProjects(projects)}
			.join(kRecordSeparator);
	}

	QStringList recentlyMissing(const QString& record, const QString& matchKey,
								const QDateTime& now)
	{
		const QStringList fields = record.split(kRecordSeparator);
		if (fields.size() != kRecordFields ||
			fields.at(kRecordKeyField) != matchKey) {
			return {};
		}
		bool isTime = false;
		const qint64 when = fields.at(kRecordTimeField).toLongLong(&isTime);
		const qint64 age = now.toSecsSinceEpoch() - when;
		/* A negative age is a clock that was set back since: that record
		 * cannot be trusted to be recent. */
		if (!isTime || age < 0 || age >= kMissingForSecs) {
			return {};
		}
		return parseProjects(fields.at(kRecordProjectsField));
	}
} // namespace PerformanceSetup
