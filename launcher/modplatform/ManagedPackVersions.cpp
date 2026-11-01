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

#include "ManagedPackVersions.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

#include "Json.h"

namespace ManagedPack
{

	QString releaseTypeToString(ReleaseType type)
	{
		switch (type) {
			case ReleaseType::Release:
				return QStringLiteral("Release");
			case ReleaseType::Beta:
				return QStringLiteral("Beta");
			case ReleaseType::Alpha:
				return QStringLiteral("Alpha");
			case ReleaseType::Unknown:
				break;
		}
		/* Empty, not "Unknown": label() tests for empty to skip the bracket. */
		return QString();
	}

	ReleaseType releaseTypeFromModrinth(const QString& value)
	{
		const QString normalised = value.trimmed().toLower();
		if (normalised == QLatin1String("release")) {
			return ReleaseType::Release;
		}
		if (normalised == QLatin1String("beta")) {
			return ReleaseType::Beta;
		}
		if (normalised == QLatin1String("alpha")) {
			return ReleaseType::Alpha;
		}
		return ReleaseType::Unknown;
	}

	ReleaseType releaseTypeFromCurseForge(int value)
	{
		switch (value) {
			case 1:
				return ReleaseType::Release;
			case 2:
				return ReleaseType::Beta;
			case 3:
				return ReleaseType::Alpha;
			default:
				return ReleaseType::Unknown;
		}
	}

	QString Version::label() const
	{
		const QString typeName = releaseTypeToString(releaseType);
		const QString releaseSuffix =
			typeName.isEmpty() ? QString()
							   : QStringLiteral(" [%1]").arg(typeName);

		/* Repeat the bare number only when the display name lacks it. The
		 * isEmpty() test is needed: contains("") is always true. */
		const QString numberPart =
			(versionNumber.isEmpty() || displayName.contains(versionNumber))
				? QString()
				: versionNumber;

		/* Append a game version only if none of the declared ones appears in the
		 * display name; scan all of them, so ["1.20.1", "1.20.4"] with a name
		 * mentioning 1.20.4 gains no contradictory " for 1.20.1". */
		QString gameVersionPart;
		for (const QString& mcVersion : mcVersions) {
			if (mcVersion.isEmpty()) {
				continue;
			}
			if (displayName.contains(mcVersion)) {
				gameVersionPart.clear();
				break;
			}
			if (gameVersionPart.isEmpty()) {
				gameVersionPart =
					QCoreApplication::translate("ManagedPack", " for %1")
						.arg(mcVersion);
			}
		}

		/* The separator is attached to the assembled tail, so an empty tail never
		 * leaves "Some Pack — " or a doubled space. */
		QString tail = numberPart;
		if (!releaseSuffix.isEmpty()) {
			/* releaseSuffix starts with a space: the gap before the bracket, which
			 * must not survive without a number. */
			tail += tail.isEmpty() ? releaseSuffix.trimmed() : releaseSuffix;
		}

		if (tail.isEmpty()) {
			return displayName + gameVersionPart;
		}
		return QStringLiteral("%1%2 — %3")
			.arg(displayName, gameVersionPart, tail);
	}

	VersionList parseModrinthVersions(const QByteArray& bytes, bool* ok)
	{
		VersionList versions;
		if (ok != nullptr) {
			*ok = false;
		}

		const QJsonDocument doc = QJsonDocument::fromJson(bytes);
		if (!doc.isArray()) {
			return versions;
		}

		const QJsonArray array = doc.array();
		versions.reserve(array.size());

		for (const auto& entryRaw : array) {
			const QJsonObject entry = entryRaw.toObject();

			Version version;
			version.versionId = Json::ensureString(entry, "id", "");
			version.versionNumber =
				Json::ensureString(entry, "version_number", "");

			/* Modrinth's `name` can be blank; fall back to the version number so
			 * the row does not start with " — ". */
			version.displayName = Json::ensureString(entry, "name", "");
			if (version.displayName.isEmpty()) {
				version.displayName = version.versionNumber;
			}

			version.releaseType = releaseTypeFromModrinth(
				Json::ensureString(entry, "version_type", ""));

			for (const auto& gameVersionRaw :
				 Json::ensureArray(entry, "game_versions")) {
				const QString gameVersion = gameVersionRaw.toString();
				if (!gameVersion.isEmpty()) {
					version.mcVersions.append(gameVersion);
				}
			}

			version.changelog = Json::ensureString(entry, "changelog", "");

			/* A version may ship several files (pack, server bundle); `primary`
			 * marks the pack, and is often omitted when there is one file. */
			const QJsonArray files = Json::ensureArray(entry, "files");
			for (const auto& fileRaw : files) {
				const QJsonObject file = fileRaw.toObject();
				const bool primary =
					Json::ensureBoolean(file, QStringLiteral("primary"), false);
				if (!primary && files.size() != 1) {
					continue;
				}
				version.downloadUrl = Json::ensureString(file, "url", "");
				break;
			}

			/* No id: cannot be matched against what is installed or imported. */
			if (version.versionId.isEmpty()) {
				continue;
			}
			versions.append(version);
		}

		if (ok != nullptr) {
			*ok = true;
		}
		return versions;
	}

	/* CurseForge's `gameVersions` mixes Minecraft versions, loader names and
	 * Java versions with no type tag. A Minecraft version starts with a
	 * digit, a loader name does not; a miss only costs an odd " for ...". */
	static bool looksLikeMinecraftVersion(const QString& value)
	{
		if (value.isEmpty()) {
			return false;
		}
		if (!value.at(0).isDigit()) {
			return false;
		}
		/* "Java 17" already fails the first-character test; this rules out other
		 * entries that merely start with a number. */
		return !value.contains(QLatin1Char(' '));
	}

	VersionList parseCurseForgeFiles(const QByteArray& bytes, bool* ok)
	{
		VersionList versions;
		if (ok != nullptr) {
			*ok = false;
		}

		const QJsonDocument doc = QJsonDocument::fromJson(bytes);
		if (!doc.isObject()) {
			return versions;
		}

		const QJsonArray array = Json::ensureArray(doc.object(), "data");
		versions.reserve(array.size());

		/* Sorted at the end, so keep the numeric id beside each entry. */
		QVector<QPair<int, Version>> withIds;
		withIds.reserve(array.size());

		for (const auto& entryRaw : array) {
			const QJsonObject entry = entryRaw.toObject();

			const int fileId = Json::ensureInteger(entry, "id", 0);
			if (fileId == 0) {
				continue;
			}

			Version version;
			version.versionId = QString::number(fileId);

			/* `displayName` is what the site shows; `fileName` is always present. */
			version.displayName = Json::ensureString(entry, "displayName", "");
			if (version.displayName.isEmpty()) {
				version.displayName = Json::ensureString(entry, "fileName", "");
			}

			/* CurseForge has no bare version number field; label() omits that part. */

			version.releaseType = releaseTypeFromCurseForge(
				Json::ensureInteger(entry, "releaseType", 0));

			for (const auto& gameVersionRaw :
				 Json::ensureArray(entry, "gameVersions")) {
				const QString gameVersion = gameVersionRaw.toString();
				if (looksLikeMinecraftVersion(gameVersion)) {
					version.mcVersions.append(gameVersion);
				}
			}

			/* Absent when the author opted out of third-party distribution; the
			 * version is still listed and isInstallable() refuses the update. */
			version.downloadUrl = Json::ensureString(entry, "downloadUrl", "");

			/* CurseForge lists no changelog; it needs a request per file, which
			 * nothing makes, so `changelog` stays empty. */

			withIds.append({ fileId, version });
		}

		/* Newest first: CurseForge promises no order, and file ids grow with age
		 * per project. */
		std::sort(withIds.begin(), withIds.end(),
				  [](const QPair<int, Version>& a,
					 const QPair<int, Version>& b) -> bool {
					  return a.first > b.first;
				  });

		for (const auto& entry : withIds) {
			versions.append(entry.second);
		}

		if (ok != nullptr) {
			*ok = true;
		}
		return versions;
	}

	int indexOfVersionId(const VersionList& versions, const QString& versionId)
	{
		const QString wanted = versionId.trimmed();
		if (wanted.isEmpty()) {
			return -1;
		}
		for (int i = 0; i < versions.size(); ++i) {
			if (versions.at(i).versionId.trimmed() == wanted) {
				return i;
			}
		}
		return -1;
	}

	int indexOfVersionName(const VersionList& versions, const QString& name)
	{
		const QString wanted = name.trimmed();
		if (wanted.isEmpty()) {
			return -1;
		}
		for (int i = 0; i < versions.size(); ++i) {
			const Version& version = versions.at(i);
			if (version.versionNumber.trimmed() == wanted ||
				version.displayName.trimmed() == wanted) {
				return i;
			}
		}
		return -1;
	}

} // namespace ManagedPack
