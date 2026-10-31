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

#include <QString>
#include <QUrl>
#include <QVector>

namespace Modrinth
{

	struct File {
		QString path;
		QUrl downloadUrl;
		QString sha1;
		QString sha512;
		int fileSize = 0;

		/* What the manifest's `env` block says about this file for a client. Both
		 * default to permissive since `env` is optional.
		 *
		 * `required` false: the pack offers the file, installed turned off.
		 * `clientSupported` false: not for clients at all, not installed. */
		bool required = true;
		bool clientSupported = true;
	};

	struct Dependency {
		QString versionId;
		QString projectId;
		QString fileName;
	};

	struct Manifest {
		int formatVersion = 0;
		QString game;
		QString versionId;
		QString name;
		QString summary;
		QVector<Modrinth::File> files;

		QString minecraftVersion;
		QString forgeVersion;
		QString fabricVersion;
		QString quiltVersion;
		QString neoForgeVersion;
	};

	void loadManifest(Modrinth::Manifest& m, const QString& filepath);

} // namespace Modrinth
