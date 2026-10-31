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

#include "ModrinthPackManifest.h"

#include "Json.h"

static void loadFile(Modrinth::File& f, QJsonObject& fileObj)
{
	f.path = Json::requireString(fileObj, "path");

	auto downloads = Json::requireArray(fileObj, "downloads");
	if (!downloads.isEmpty()) {
		f.downloadUrl = QUrl(downloads.first().toString());
	}

	auto hashes = Json::ensureObject(fileObj, "hashes");
	f.sha1 = Json::ensureString(hashes, "sha1", "");
	f.sha512 = Json::ensureString(hashes, "sha512", "");

	f.fileSize = Json::ensureInteger(fileObj, "fileSize", 0);

	/* The `env` block, client side only. An absent block, or an absent
	 * `client` in a present one (undefined by the spec), means "required":
	 * a mod the user can turn off is a smaller mistake than one silently
	 * missing. */
	auto env = Json::ensureObject(fileObj, "env");
	if (!env.isEmpty()) {
		const QString clientSupport =
			Json::ensureString(env, "client", "required");
		if (clientSupport == QLatin1String("unsupported")) {
			f.clientSupported = false;
		} else if (clientSupport == QLatin1String("optional")) {
			f.required = false;
		}
	}
}

static void loadDependencies(Modrinth::Manifest& m, QJsonObject& deps)
{
	m.minecraftVersion = Json::ensureString(deps, "minecraft", "");
	m.forgeVersion = Json::ensureString(deps, "forge", "");
	m.fabricVersion = Json::ensureString(deps, "fabric-loader", "");
	m.quiltVersion = Json::ensureString(deps, "quilt-loader", "");
	m.neoForgeVersion = Json::ensureString(deps, "neoforge", "");
}

void Modrinth::loadManifest(Modrinth::Manifest& m, const QString& filepath)
{
	auto doc = Json::requireDocument(filepath);
	auto obj = Json::requireObject(doc);

	m.formatVersion = Json::requireInteger(obj, "formatVersion");
	if (m.formatVersion != 1) {
		throw JSONValidationError(
			QString("Unsupported Modrinth modpack format version: %1")
				.arg(m.formatVersion));
	}

	m.game = Json::requireString(obj, "game");
	if (m.game != "minecraft") {
		throw JSONValidationError(
			QString("Unsupported game in Modrinth modpack: %1").arg(m.game));
	}

	m.versionId = Json::ensureString(obj, "versionId", "");
	m.name = Json::ensureString(obj, "name", "Unnamed");
	m.summary = Json::ensureString(obj, "summary", "");

	auto files = Json::requireArray(obj, "files");
	for (auto fileRaw : files) {
		auto fileObj = Json::requireObject(fileRaw);
		Modrinth::File file;
		loadFile(file, fileObj);
		m.files.append(file);
	}

	auto deps = Json::ensureObject(obj, "dependencies");
	if (!deps.isEmpty()) {
		loadDependencies(m, deps);
	}
}
