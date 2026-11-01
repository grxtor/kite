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

#include "VersionPicker.h"

#include <QDateTime>
#include <QJsonValue>
#include <QStringList>

#include "modplatform/ManagedPackVersions.h"

namespace
{

	/* Loader names as the two providers spell them, lower-cased, to tell a
	 * loader from a game version in CurseForge's mixed list. */
	const QStringList& knownLoaderNames()
	{
		static const QStringList names = {
			QStringLiteral("neoforge"),      QStringLiteral("forge"),
			QStringLiteral("fabric"),        QStringLiteral("quilt"),
			QStringLiteral("liteloader"),    QStringLiteral("babric"),
			QStringLiteral("bta-babric"),    QStringLiteral("legacy-fabric"),
			QStringLiteral("ornithe"),       QStringLiteral("rift"),
		};
		return names;
	}

	/* Publish dates are RFC 3339. Parsed, not compared as text, so a missing
	 * or malformed date loses to a real one. */
	QDateTime parseDate(const QJsonObject& obj, const QString& key)
	{
		return QDateTime::fromString(obj.value(key).toString(), Qt::ISODate);
	}

	/* Whether this entry may run on `loader`. An entry stating no loader is
	 * accepted: many library mods state none. */
	bool loaderMatches(const QStringList& entryLoaders, const QString& loader)
	{
		if (loader.isEmpty() || entryLoaders.isEmpty()) {
			return true;
		}
		return entryLoaders.contains(loader, Qt::CaseInsensitive);
	}

	/* The best entry seen so far, for both providers: the newest by publish
	 * date and, when asked to prefer releases, a release over any alpha or
	 * beta, however much newer that is. */
	class Newest
	{
	  public:
		explicit Newest(bool preferRelease) : m_preferRelease(preferRelease) {}

		void offer(const QJsonObject& entry, const QDateTime& date,
				   bool release)
		{
			if (!m_best.isEmpty()) {
				if (m_preferRelease && release != m_bestRelease) {
					if (!release) {
						return;
					}
				} else if (!(date.isValid() && date > m_bestDate)) {
					return;
				}
			}
			m_best = entry;
			m_bestDate = date;
			m_bestRelease = release;
		}

		QJsonObject best() const
		{
			return m_best;
		}

	  private:
		bool m_preferRelease;
		QJsonObject m_best;
		QDateTime m_bestDate;
		bool m_bestRelease = false;
	};

} // namespace

bool ModPlatform::isKnownLoaderName(const QString& tag)
{
	return knownLoaderNames().contains(tag.toLower());
}

QJsonObject ModPlatform::newestCurseForgeFile(const QJsonArray& files,
											  const QString& loader,
											  bool preferRelease)
{
	Newest newest(preferRelease);

	for (const auto& fileRaw : files) {
		const auto file = fileRaw.toObject();
		if (file.isEmpty()) {
			continue;
		}

		/* CurseForge mixes loader names into the game version array. */
		QStringList entryLoaders;
		for (const auto& tagRaw : file.value("gameVersions").toArray()) {
			const QString tag = tagRaw.toString();
			if (knownLoaderNames().contains(tag.toLower())) {
				entryLoaders.append(tag);
			}
		}
		if (!loaderMatches(entryLoaders, loader)) {
			continue;
		}

		const bool release = ManagedPack::releaseTypeFromCurseForge(
								 file.value("releaseType").toInt()) ==
							 ManagedPack::ReleaseType::Release;
		newest.offer(file, parseDate(file, QStringLiteral("fileDate")), release);
	}

	return newest.best();
}

QJsonObject ModPlatform::newestModrinthVersion(const QJsonArray& versions,
											   const QString& loader,
											   bool preferRelease)
{
	Newest newest(preferRelease);

	for (const auto& versionRaw : versions) {
		const auto version = versionRaw.toObject();
		if (version.isEmpty()) {
			continue;
		}

		QStringList entryLoaders;
		for (const auto& loaderRaw : version.value("loaders").toArray()) {
			entryLoaders.append(loaderRaw.toString());
		}
		if (!loaderMatches(entryLoaders, loader)) {
			continue;
		}

		const bool release = ManagedPack::releaseTypeFromModrinth(
								 version.value("version_type").toString()) ==
							 ManagedPack::ReleaseType::Release;
		newest.offer(version,
					 parseDate(version, QStringLiteral("date_published")),
					 release);
	}

	return newest.best();
}
