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

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

namespace ModPlatform
{

	struct SelectedMod {
		QString name;
		QString projectId; // platform-specific project ID
		QString versionId; // platform-specific version/file ID
		/* Platform slug: the one name both providers usually agree on, and the
		 * packwiz sidecar's file name. */
		QString slug;
		QString fileName;
		QString downloadUrl;
		QString sha1;
		int fileSize = 0;
		QString platform; // "curseforge" or "modrinth"
		QString mcVersion;
		QString loaders;
		/* "release" / "beta" / "alpha"; empty when the provider does not say. */
		QString versionType;
		/* No API URL (the author forbade third-party downloads), so the URL is
		 * the website's; may need a manual download. */
		bool browserDownloadOnly = false;
	};

	struct DependencyInfo {
		QString projectId;
		QString versionId;
		QString name;
		QString slug;
		QString fileName;
		QString downloadUrl;
		QString sha1;
		int fileSize = 0;
		QString platform;
		QString versionType;
		bool isRequired = true;
		bool browserDownloadOnly = false;

		/* Names of what asked for this, for the review dialog's "Required by"
		 * line. A list: the dependency is downloaded once. */
		QStringList requiredBy;

		/* The project is on disk at another version (maybe the wrong Minecraft
		 * version, or damaged): offered unticked, not dropped. */
		bool maybeInstalled = false;
	};

	struct DownloadItem {
		QString name;
		QString fileName;
		QString downloadUrl;
		QString sha1;
		int fileSize = 0;
		bool isDependency = false;

		/* Provenance, needed to write the sidecar after install and for
		 * conflict/update detection. Empty for purely local installs. */
		QString platform; /* "modrinth" | "curseforge" | ""               */
		QString projectId;
		QString versionId;
		QString slug;

		/* The URL is the website's, not the API's (author opted out of
		 * third-party downloads); the download can be refused. */
		bool browserDownloadOnly = false;

		/* Replace an existing file of the same target name (update flow).
		 * Otherwise a matching SHA-1 is kept silently and a mismatch is an error. */
		bool replaceExisting = false;

		/* Set by the conflict analyzer: this file (and its sidecar) is removed
		 * before the new one is written. */
		QString replacesFileName;
	};

	struct UnresolvedDep {
		QString name;
		QString projectId;
		QString platform;
	};

	inline QString curseForgeSha1FromFileObject(const QJsonObject& fileObj)
	{
		const auto hashes = fileObj.value("hashes").toArray();
		for (const auto& hashRaw : hashes) {
			const auto hashObj = hashRaw.toObject();
			if (hashObj.value("algo").toInt() == 1) {
				return hashObj.value("value").toString();
			}
		}
		return QString();
	}

} // namespace ModPlatform

Q_DECLARE_METATYPE(ModPlatform::SelectedMod)
Q_DECLARE_METATYPE(ModPlatform::DependencyInfo)
Q_DECLARE_METATYPE(ModPlatform::DownloadItem)
Q_DECLARE_METATYPE(ModPlatform::UnresolvedDep)
