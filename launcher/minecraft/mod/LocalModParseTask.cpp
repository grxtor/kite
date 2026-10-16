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

#include "LocalModParseTask.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDir>
#include <QFile>
#include <toml++/toml.hpp>

#include "MMCZip.h"
#include "ModIconCache.h"

#include "settings/INIFile.h"
#include "FileSystem.h"

namespace
{

	/* Metadata names its logo relative to the archive root, but not always
	 * the way the archive spells it: mcmod.info in particular often writes
	 * "/assets/x/logo.png" or "./logo.png". */
	QString archivePath(QString path)
	{
		path = path.trimmed().replace(QLatin1Char('\\'), QLatin1Char('/'));
		while (path.startsWith(QLatin1Char('/')) ||
			   path.startsWith(QLatin1String("./"))) {
			path.remove(0, path.startsWith(QLatin1Char('/')) ? 1 : 2);
		}
		return path;
	}

	/* fabric.mod.json / quilt.mod.json "icon": one path, or a map of pixel
	 * size -> path. Takes the largest no bigger than what the cache keeps,
	 * or failing that the smallest one there is. */
	QString pickIcon(const QJsonValue& value)
	{
		if (value.isString()) {
			return value.toString();
		}
		const QJsonObject sizes = value.toObject();
		QString best;
		int bestSize = 0;
		QString smallest;
		int smallestSize = 0;
		for (auto it = sizes.constBegin(); it != sizes.constEnd(); ++it) {
			bool ok = false;
			const int size = it.key().toInt(&ok);
			const QString path = it.value().toString();
			if (!ok || size <= 0 || path.isEmpty()) {
				continue;
			}
			if (size <= ModIconCache::kMaxExtent && size > bestSize) {
				best = path;
				bestSize = size;
			}
			if (smallestSize == 0 || size < smallestSize) {
				smallest = path;
				smallestSize = size;
			}
		}
		return best.isEmpty() ? smallest : best;
	}

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

	// https://github.com/MinecraftForge/Documentation/blob/5ab4ba6cf9abc0ac4c0abd96ad187461aefd72af/docs/gettingstarted/structuring.md
	std::shared_ptr<ModDetails> ReadMCModTOML(QByteArray contents)
	{
		std::shared_ptr<ModDetails> details = std::make_shared<ModDetails>();

		toml::table tomlData;
		try {
			tomlData = toml::parse(
				std::string_view(contents.constData(), contents.size()));
		} catch (const toml::parse_error&) {
			return nullptr;
		}

		// array defined by [[mods]]
		auto* tomlModsArr = tomlData["mods"].as_array();
		if (!tomlModsArr) {
			qWarning() << "Corrupted mods.toml? Couldn't find [[mods]] array!";
			return nullptr;
		}

		// we only really care about the first element, since multiple mods in
		// one file is not supported by us at the moment
		auto* tomlModsTable0 = tomlModsArr->get_as<toml::table>(0);
		if (!tomlModsTable0) {
			qWarning() << "Corrupted mods.toml? [[mods]] didn't have an "
						  "element at index 0!";
			return nullptr;
		}

		// mandatory properties - always in [[mods]]
		if (auto val = (*tomlModsTable0)["modId"].value<std::string>()) {
			details->mod_id = QString::fromStdString(*val);
		}
		if (auto val = (*tomlModsTable0)["version"].value<std::string>()) {
			details->version = QString::fromStdString(*val);
		}
		if (auto val = (*tomlModsTable0)["displayName"].value<std::string>()) {
			details->name = QString::fromStdString(*val);
		}
		if (auto val = (*tomlModsTable0)["description"].value<std::string>()) {
			details->description = QString::fromStdString(*val);
		}

		// optional properties - can be in the root table or [[mods]]
		QString authors;
		if (auto val = tomlData["authors"].value<std::string>()) {
			authors = QString::fromStdString(*val);
		} else if (auto val =
					   (*tomlModsTable0)["authors"].value<std::string>()) {
			authors = QString::fromStdString(*val);
		}
		if (!authors.isEmpty()) {
			// author information is stored as a string now, not a list
			details->authors.append(authors);
		}
		// is credits even used anywhere? including this for completion/parity
		// with old data version
		QString credits;
		if (auto val = tomlData["credits"].value<std::string>()) {
			credits = QString::fromStdString(*val);
		} else if (auto val =
					   (*tomlModsTable0)["credits"].value<std::string>()) {
			credits = QString::fromStdString(*val);
		}
		details->credits = credits;
		QString homeurl;
		if (auto val = tomlData["displayURL"].value<std::string>()) {
			homeurl = QString::fromStdString(*val);
		} else if (auto val =
					   (*tomlModsTable0)["displayURL"].value<std::string>()) {
			homeurl = QString::fromStdString(*val);
		}
		if (!homeurl.isEmpty()) {
			// fix up url.
			if (!homeurl.startsWith("http://") &&
				!homeurl.startsWith("https://") &&
				!homeurl.startsWith("ftp://")) {
				homeurl.prepend("http://");
			}
		}
		details->homeurl = homeurl;

		// Per mod in [[mods]]; some older mods.toml files set it once at
		// the root instead.
		if (auto val = (*tomlModsTable0)["logoFile"].value<std::string>()) {
			details->icon = QString::fromStdString(*val);
		} else if (auto val = tomlData["logoFile"].value<std::string>()) {
			details->icon = QString::fromStdString(*val);
		}

		return details;
	}

	// https://fabricmc.net/wiki/documentation:fabric_mod_json
	std::shared_ptr<ModDetails> ReadFabricModInfo(QByteArray contents)
	{
		QJsonParseError jsonError;
		QJsonDocument jsonDoc = QJsonDocument::fromJson(contents, &jsonError);
		auto object = jsonDoc.object();
		auto schemaVersion = object.contains("schemaVersion")
								 ? object.value("schemaVersion").toInt(0)
								 : 0;

		std::shared_ptr<ModDetails> details = std::make_shared<ModDetails>();

		details->mod_id = object.value("id").toString();
		details->version = object.value("version").toString();

		details->name = object.contains("name")
							? object.value("name").toString()
							: details->mod_id;
		details->description = object.value("description").toString();
		details->icon = pickIcon(object.value("icon"));

		if (schemaVersion >= 1) {
			QJsonArray authors = object.value("authors").toArray();
			for (auto author : authors) {
				if (author.isObject()) {
					details->authors.append(
						author.toObject().value("name").toString());
				} else {
					details->authors.append(author.toString());
				}
			}

			if (object.contains("contact")) {
				QJsonObject contact = object.value("contact").toObject();

				if (contact.contains("homepage")) {
					details->homeurl = contact.value("homepage").toString();
				}
			}
		}
		return details;
	}

	// https://github.com/QuiltMC/rfcs/blob/main/specification/0002-quilt.mod.json.md
	std::shared_ptr<ModDetails> ReadQuiltModInfo(QByteArray contents)
	{
		const QJsonObject loader = QJsonDocument::fromJson(contents)
									   .object()
									   .value("quilt_loader")
									   .toObject();
		std::shared_ptr<ModDetails> details = std::make_shared<ModDetails>();
		details->mod_id = loader.value("id").toString();
		details->version = loader.value("version").toString();

		const QJsonObject metadata = loader.value("metadata").toObject();
		details->name = metadata.contains("name")
							? metadata.value("name").toString()
							: details->mod_id;
		details->description = metadata.value("description").toString();
		details->icon = pickIcon(metadata.value("icon"));
		return details;
	}

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
		return details;
	}

} // namespace

LocalModParseTask::LocalModParseTask(int token, Mod::ModType type,
									 const QFileInfo& modFile,
									 const QString& iconCacheDir)
	: m_token(token), m_type(type), m_modFile(modFile), m_result(new Result()),
	  m_iconCacheDir(iconCacheDir)
{
}

void LocalModParseTask::processAsZip()
{
	QString zipPath = m_modFile.filePath();

	QByteArray modsToml =
		MMCZip::readFileFromZip(zipPath, "META-INF/mods.toml");
	if (!modsToml.isEmpty()) {
		m_result->details = ReadMCModTOML(modsToml);

		// to replace ${file.jarVersion} with the actual version, as needed
		if (m_result->details &&
			m_result->details->version == "${file.jarVersion}") {
			QByteArray manifestData =
				MMCZip::readFileFromZip(zipPath, "META-INF/MANIFEST.MF");
			if (!manifestData.isEmpty()) {
				// quick and dirty line-by-line parser
				auto manifestLines = manifestData.split('\n');
				QString manifestVersion = "";
				for (auto& line : manifestLines) {
					if (QString(line).startsWith("Implementation-Version: ")) {
						manifestVersion =
							QString(line).remove("Implementation-Version: ");
						break;
					}
				}

				// some mods use ${projectversion} in their build.gradle,
				// causing this mess to show up in MANIFEST.MF also keep with
				// forge's behavior of setting the version to "NONE" if none is
				// found
				if (manifestVersion.contains(
						"task ':jar' property 'archiveVersion'") ||
					manifestVersion == "") {
					manifestVersion = "NONE";
				}

				m_result->details->version = manifestVersion;
			}
		}
		return;
	}

	QByteArray mcmodInfo = MMCZip::readFileFromZip(zipPath, "mcmod.info");
	if (!mcmodInfo.isEmpty()) {
		m_result->details = ReadMCModInfo(mcmodInfo);
		return;
	}

	QByteArray fabricModJson =
		MMCZip::readFileFromZip(zipPath, "fabric.mod.json");
	if (!fabricModJson.isEmpty()) {
		m_result->details = ReadFabricModInfo(fabricModJson);
		return;
	}

	QByteArray quiltModJson =
		MMCZip::readFileFromZip(zipPath, "quilt.mod.json");
	if (!quiltModJson.isEmpty()) {
		m_result->details = ReadQuiltModInfo(quiltModJson);
		return;
	}

	QByteArray forgeVersionProps =
		MMCZip::readFileFromZip(zipPath, "forgeversion.properties");
	if (!forgeVersionProps.isEmpty()) {
		m_result->details = ReadForgeInfo(forgeVersionProps);
		return;
	}
}

void LocalModParseTask::processAsFolder()
{
	QFileInfo mcmod_info(FS::PathCombine(m_modFile.filePath(), "mcmod.info"));
	if (mcmod_info.isFile()) {
		QFile mcmod(mcmod_info.filePath());
		if (!mcmod.open(QIODevice::ReadOnly))
			return;
		auto data = mcmod.readAll();
		if (data.isEmpty() || data.isNull())
			return;
		m_result->details = ReadMCModInfo(data);
	}
}

void LocalModParseTask::processAsLitemod()
{
	QByteArray litemodJson =
		MMCZip::readFileFromZip(m_modFile.filePath(), "litemod.json");
	if (!litemodJson.isEmpty()) {
		m_result->details = ReadLiteModInfo(litemodJson);
	}
}

void LocalModParseTask::cacheIcon(
	const std::function<QByteArray(const QString&)>& read)
{
	if (m_iconCacheDir.isEmpty()) {
		return;
	}
	const QString key = ModIconCache::keyFor(m_modFile);
	if (ModIconCache::contains(m_iconCacheDir, key)) {
		m_result->iconKey = key;
		return;
	}

	// The logo the metadata declares, then pack.png: what Minecraft itself
	// shows for a resource or data pack, and what a jar that ships its own
	// resources usually carries as well.
	QStringList candidates;
	if (m_result->details && !m_result->details->icon.isEmpty()) {
		candidates << archivePath(m_result->details->icon);
	}
	candidates << QStringLiteral("pack.png");
	for (const QString& entry : candidates) {
		if (entry.isEmpty()) {
			continue;
		}
		if (ModIconCache::store(m_iconCacheDir, key, read(entry))) {
			m_result->iconKey = key;
			return;
		}
	}
}

void LocalModParseTask::run()
{
	switch (m_type) {
		case Mod::MOD_ZIPFILE:
			processAsZip();
			cacheIcon([this](const QString& entry) {
				return MMCZip::readFileFromZip(m_modFile.filePath(), entry);
			});
			break;
		case Mod::MOD_FOLDER:
			processAsFolder();
			cacheIcon([this](const QString& entry) -> QByteArray {
				// Metadata is the mod author's text: keep "../" from
				// reaching outside the mod's own folder.
				const QString root = m_modFile.canonicalFilePath();
				const QString path =
					QFileInfo(QDir(root).filePath(entry)).canonicalFilePath();
				if (root.isEmpty() ||
					!path.startsWith(root + QLatin1Char('/'))) {
					return {};
				}
				QFile file(path);
				if (file.size() > ModIconCache::kMaxEncodedBytes ||
					!file.open(QIODevice::ReadOnly)) {
					return {};
				}
				return file.readAll();
			});
			break;
		case Mod::MOD_LITEMOD:
			processAsLitemod();
			break;
		default:
			break;
	}
	emit finished(m_token);
}
