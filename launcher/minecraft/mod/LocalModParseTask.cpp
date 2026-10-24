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

#include "LocalModParseTask.h"

#include <QDir>
#include <QFile>

#include "MMCZip.h"
#include "ModIconCache.h"
#include "ModMetadataReader.h"

#include "FileSystem.h"

LocalModParseTask::LocalModParseTask(int token, Mod::ModType type,
									 const QFileInfo& modFile,
									 const QString& iconCacheDir)
	: m_token(token), m_type(type), m_modFile(modFile), m_result(new Result()),
	  m_iconCacheDir(iconCacheDir)
{
}

void LocalModParseTask::processAsZip()
{
	const QString zipPath = m_modFile.filePath();
	const ModMetadata::EntryReader read = [&zipPath](const QStringList& names) {
		return MMCZip::readFilesFromZip(zipPath, names);
	};
	m_result->details = ModMetadata::readDetails(
		read(ModMetadata::kMetadataEntries), read);
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
		m_result->details = ModMetadata::ReadMCModInfo(data);
	}
}

void LocalModParseTask::processAsLitemod()
{
	QByteArray litemodJson =
		MMCZip::readFileFromZip(m_modFile.filePath(), "litemod.json");
	if (!litemodJson.isEmpty()) {
		m_result->details = ModMetadata::ReadLiteModInfo(litemodJson);
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
		candidates << ModMetadata::archivePath(m_result->details->icon);
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
