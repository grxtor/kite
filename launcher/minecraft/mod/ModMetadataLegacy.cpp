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

#include "ModMetadataReader.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>

#include "ModMetadataFormats.h"
#include "settings/INIFile.h"

using ModMetadata::detail::kLoaderForge;
using ModMetadata::detail::kLoaderLiteLoader;

namespace ModMetadata
{

	// NEW format
	// https://github.com/MinecraftForge/FML/wiki/FML-mod-information-file/6f62b37cea040daf350dc253eae6326dd9c822c3

	// OLD format:
	// https://github.com/MinecraftForge/FML/wiki/FML-mod-information-file/5bf6a2d05145ec79387acc0d45c958642fb049fc
	std::shared_ptr<ModDetails> ReadMCModInfo(QByteArray contents)
	{
		auto getInfoFromArray =
			[&](QJsonArray arr) -> std::shared_ptr<ModDetails> {
			if (!arr.at(0).isObject()) {
				return nullptr;
			}
			std::shared_ptr<ModDetails> details =
				std::make_shared<ModDetails>();
			auto firstObj = arr.at(0).toObject();
			details->mod_id = firstObj.value("modid").toString();
			auto name = firstObj.value("name").toString();
			// NOTE: ignore stupid example mods copies where the author didn't
			// even bother to change the name
			if (name != "Example Mod") {
				details->name = name;
			}
			details->version = firstObj.value("version").toString();
			details->updateurl = firstObj.value("updateUrl").toString();
			auto homeurl = firstObj.value("url").toString().trimmed();
			if (!homeurl.isEmpty()) {
				// fix up url.
				if (!homeurl.startsWith("http://") &&
					!homeurl.startsWith("https://") &&
					!homeurl.startsWith("ftp://")) {
					homeurl.prepend("http://");
				}
			}
			details->homeurl = homeurl;
			details->description = firstObj.value("description").toString();
			QJsonArray authors = firstObj.value("authorList").toArray();
			if (authors.size() == 0) {
				// FIXME: what is the format of this? is there any?
				authors = firstObj.value("authors").toArray();
			}

			for (auto author : authors) {
				details->authors.append(author.toString());
			}
			details->credits = firstObj.value("credits").toString();
			details->icon = firstObj.value("logoFile").toString();
			details->loaders = {kLoaderForge};
			return details;
		};
		QJsonParseError jsonError;
		QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
		// this is the very old format that had just the array
		if (jsonDoc.isArray()) {
			return getInfoFromArray(jsonDoc.array());
		} else if (jsonDoc.isObject()) {
			auto val = jsonDoc.object().value("modinfoversion");
			if (val.isUndefined()) {
				val = jsonDoc.object().value("modListVersion");
			}
			int version = val.toDouble();
			if (version != 2) {
				qCritical() << "BAD stuff happened to mod json:";
				qCritical() << contents;
				return nullptr;
			}
			auto arrVal = jsonDoc.object().value("modlist");
			if (arrVal.isUndefined()) {
				arrVal = jsonDoc.object().value("modList");
			}
			if (arrVal.isArray()) {
				return getInfoFromArray(arrVal.toArray());
			}
		}
		return nullptr;
	}

	std::shared_ptr<ModDetails> ReadLiteModInfo(QByteArray contents)
	{
		std::shared_ptr<ModDetails> details = std::make_shared<ModDetails>();
		QJsonParseError jsonError;
		QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
		auto object = jsonDoc.object();
		if (object.contains("name")) {
			details->mod_id = details->name = object.value("name").toString();
		}
		if (object.contains("version")) {
			details->version = object.value("version").toString("");
		} else {
			details->version = object.value("revision").toString("");
		}
		details->mcversion = object.value("mcversion").toString();
		auto author = object.value("author").toString();
		if (!author.isEmpty()) {
			details->authors.append(author);
		}
		details->description = object.value("description").toString();
		details->homeurl = object.value("url").toString();
		details->loaders = {kLoaderLiteLoader};
		return details;
	}

} // namespace ModMetadata

namespace ModMetadata::detail
{

	std::shared_ptr<ModDetails> ReadForgeInfo(QByteArray contents)
	{
		std::shared_ptr<ModDetails> details = std::make_shared<ModDetails>();
		// Read the data
		details->name = "Minecraft Forge";
		details->mod_id = "Forge";
		details->homeurl = "http://www.minecraftforge.net/forum/";
		INIFile ini;
		if (!ini.loadFile(contents))
			return details;

		QString major = ini.get("forge.major.number", "0").toString();
		QString minor = ini.get("forge.minor.number", "0").toString();
		QString revision = ini.get("forge.revision.number", "0").toString();
		QString build = ini.get("forge.build.number", "0").toString();

		details->version = major + "." + minor + "." + revision + "." + build;
		return details;
	}

} // namespace ModMetadata::detail
