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

#include "PackManifest.h"
#include "Json.h"

static void loadFileV1(Flame::File& f, QJsonObject& file)
{
	f.projectId = Json::requireInteger(file, "projectID");
	f.fileId = Json::requireInteger(file, "fileID");
	f.required = Json::ensureBoolean(file, QString("required"), true);
}

static void loadModloaderV1(Flame::Modloader& m, QJsonObject& modLoader)
{
	m.id = Json::requireString(modLoader, "id");
	m.primary = Json::ensureBoolean(modLoader, QString("primary"), false);
}

static void loadMinecraftV1(Flame::Minecraft& m, QJsonObject& minecraft)
{
	m.version = Json::requireString(minecraft, "version");
	// extra libraries: apparently only used by the 1.2.5 FTB retro pack; the
	// manifest says nothing
	m.libraries =
		Json::ensureString(minecraft, QString("libraries"), QString());
	auto arr = Json::ensureArray(minecraft, "modLoaders", QJsonArray());
	for (QJsonValueRef item : arr) {
		auto obj = Json::requireObject(item);
		Flame::Modloader loader;
		loadModloaderV1(loader, obj);
		m.modLoaders.append(loader);
	}
}

static void loadManifestV1(Flame::Manifest& m, QJsonObject& manifest)
{
	auto mc = Json::requireObject(manifest, "minecraft");
	loadMinecraftV1(m.minecraft, mc);
	m.name = Json::ensureString(manifest, QString("name"), "Unnamed");
	m.version = Json::ensureString(manifest, QString("version"), QString());
	m.author =
		Json::ensureString(manifest, QString("author"), "Anonymous Coward");
	auto arr = Json::ensureArray(manifest, "files", QJsonArray());
	for (QJsonValueRef item : arr) {
		auto obj = Json::requireObject(item);
		Flame::File file;
		loadFileV1(file, obj);
		m.files.append(file);
	}
	m.overrides = Json::ensureString(manifest, "overrides", "overrides");
}

void Flame::loadManifest(Flame::Manifest& m, const QString& filepath)
{
	auto doc = Json::requireDocument(filepath);
	auto obj = Json::requireObject(doc);
	m.manifestType = Json::requireString(obj, "manifestType");
	if (m.manifestType != "minecraftModpack") {
		throw JSONValidationError("Not a modpack manifest!");
	}
	m.manifestVersion = Json::requireInteger(obj, "manifestVersion");
	if (m.manifestVersion != 1) {
		throw JSONValidationError(
			QString("Unknown manifest version (%1)").arg(m.manifestVersion));
	}
	loadManifestV1(m, obj);
}

bool Flame::File::parseFromBytes(const QByteArray& bytes)
{
	auto doc = Json::requireDocument(bytes);
	auto obj = Json::requireObject(doc);
	// A result code signifies true failure.
	if (obj.contains("code")) {
		qCritical() << "Resolving of" << projectId << fileId
					<< "failed because of a negative result:";
		qCritical() << bytes;
		return false;
	}
	// API v1 wraps the file object in "data"
	if (obj.contains("data")) {
		obj = Json::requireObject(obj, "data");
	}
	// Both old cursemeta (FileNameOnDisk) and API v1 (fileName) field names.
	fileName = Json::ensureString(obj, "fileName", QString());
	if (fileName.isEmpty()) {
		fileName = Json::requireString(obj, "FileNameOnDisk");
	}
	/* One digest per algorithm, tagged with a numeric "algo": 1 is SHA-1, 2 is
	 * MD5. SHA-1 is kept, as the launcher hashes installed files with it;
	 * other algorithms are ignored, since a digest compared under the wrong
	 * algorithm reports every file as changed. */
	fileSize = static_cast<qint64>(Json::ensureDouble(obj, "fileLength", 0.0));
	for (const auto& hashRaw : Json::ensureArray(obj, "hashes")) {
		const QJsonObject hashObj = hashRaw.toObject();
		if (Json::ensureInteger(hashObj, "algo", 0) == 1) {
			sha1 = Json::ensureString(hashObj, "value", QString());
			break;
		}
	}

	QString rawUrl = Json::ensureString(obj, "downloadUrl", QString());
	if (rawUrl.isEmpty()) {
		rawUrl = Json::ensureString(obj, "DownloadURL", QString());
	}
	if (rawUrl.isEmpty()) {
		// Third-party downloads are disabled; handled via a browser download.
		qWarning() << "Mod" << projectId << fileId << "(" << fileName
				   << ") has no download URL (restricted)."
				   << "Will require browser download.";
		resolved = false;
		return true;
	}
	url = QUrl(rawUrl, QUrl::TolerantMode);
	if (!url.isValid()) {
		throw JSONValidationError(QString("Invalid URL: %1").arg(rawUrl));
	}
	// A piece of a Flame project JSON pulled into the file metadata for
	// convenience; optional.
	QJsonObject projObj = Json::ensureObject(obj, "_Project", {});
	if (!projObj.isEmpty()) {
		QString strType =
			Json::ensureString(projObj, "PackageType", "mod").toLower();
		if (strType == "singlefile") {
			type = File::Type::SingleFile;
		} else if (strType == "ctoc") {
			type = File::Type::Ctoc;
		} else if (strType == "cmod2") {
			type = File::Type::Cmod2;
		} else if (strType == "mod") {
			type = File::Type::Mod;
		} else if (strType == "folder") {
			type = File::Type::Folder;
		} else if (strType == "modpack") {
			type = File::Type::Modpack;
		} else {
			qCritical() << "Resolving of" << projectId << fileId
						<< "failed because of unknown file type:" << strType;
			type = File::Type::Unknown;
			return false;
		}
		targetFolder = Json::ensureString(projObj, "Path", "mods");
	}
	resolved = true;
	return true;
}
