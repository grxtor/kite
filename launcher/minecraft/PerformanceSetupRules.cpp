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
	const QString kExcludedSeparator = QStringLiteral(",");

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

	QStringList looseJars(const QDir& folder, const ModMetadataIndex& index)
	{
		QStringList loose;
		const QStringList jars = folder.entryList(
			{QStringLiteral("*.jar"), QStringLiteral("*.jar.disabled")},
			QDir::Files | QDir::Readable, QDir::Name);
		for (const QString& jar : jars) {
			if (!index.contains(jar)) {
				loose.append(jar);
			}
		}
		return loose;
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

	QStringList parseExcluded(const QString& stored)
	{
		return stored.split(kExcludedSeparator, Qt::SkipEmptyParts);
	}

	QString joinExcluded(const QStringList& projects)
	{
		return projects.join(kExcludedSeparator);
	}
} // namespace PerformanceSetup
