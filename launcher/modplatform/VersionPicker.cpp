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

#include "VersionPicker.h"

#include <QDateTime>
#include <QJsonValue>
#include <QStringList>

namespace
{

	/* Loader names as the two providers spell them, lower-cased. Used to
	 * tell a loader apart from a game version in CurseForge's mixed
	 * list, so anything else in there is left alone. */
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

	/* Publish dates are RFC 3339 on both providers. Parsed rather than
	 * compared as text, because an entry with a missing or malformed
	 * date has to lose to a real one instead of winning on punctuation. */
	QDateTime parseDate(const QJsonObject& obj, const QString& key)
	{
		return QDateTime::fromString(obj.value(key).toString(), Qt::ISODate);
	}

	/* Whether this entry may run on `loader`.
	 *
	 * An entry that states no loader at all is accepted: plenty of
	 * library mods do not, and refusing them would leave a dependency
	 * unresolvable for no good reason. */
	bool loaderMatches(const QStringList& entryLoaders, const QString& loader)
	{
		if (loader.isEmpty() || entryLoaders.isEmpty()) {
			return true;
		}
		return entryLoaders.contains(loader, Qt::CaseInsensitive);
	}

} // namespace

bool ModPlatform::isKnownLoaderName(const QString& tag)
{
	return knownLoaderNames().contains(tag.toLower());
}

QJsonObject ModPlatform::newestCurseForgeFile(const QJsonArray& files,
											  const QString& loader)
{
	QJsonObject best;
	QDateTime bestDate;

	for (const auto& fileRaw : files) {
		const auto file = fileRaw.toObject();
		if (file.isEmpty()) {
			continue;
		}

		/* CurseForge puts game versions and loader names in the same
		 * array, so the loader names have to be picked back out of it. */
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

		const QDateTime date = parseDate(file, QStringLiteral("fileDate"));
		if (best.isEmpty() || (date.isValid() && date > bestDate)) {
			best = file;
			bestDate = date;
		}
	}

	return best;
}

QJsonObject ModPlatform::newestModrinthVersion(const QJsonArray& versions,
											   const QString& loader)
{
	QJsonObject best;
	QDateTime bestDate;

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

		const QDateTime date =
			parseDate(version, QStringLiteral("date_published"));
		if (best.isEmpty() || (date.isValid() && date > bestDate)) {
			best = version;
			bestDate = date;
		}
	}

	return best;
}
