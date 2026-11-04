/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include <QByteArray>
#include <QJsonArray>
#include <QList>
#include <QString>
#include <QStringList>

#include "ShareCodes.h"
#include "modplatform/ModDownloadTypes.h"

/*
 * Turning a share code's content back into files: the exact Modrinth version
 * or CurseForge file each entry names, as the download items the content
 * install path takes (ContentDownloadTask). Only the platforms' own answers
 * give a file's address; the manifest never does.
 */
namespace ShareContent
{
	/// Ids per Modrinth GET and CurseForge POST.
	constexpr int kBatchSize = 100;

	struct Item {
		/// ShareCodes::Kind: which folder the file goes in.
		QString kind;
		ModPlatform::DownloadItem download;
	};

	struct Resolved {
		QList<Item> items;
		/// The entries no answer had a file for, and what to call each: the
		/// project's name when the platform still knows the project, else its id.
		QList<ShareCodes::Entry> missing;
		QStringList missingNames;
	};

	/// The entries of @p content on @p platform, in batches of kBatchSize.
	QList<QList<ShareCodes::Entry>> batches(const QList<ShareCodes::Entry>& content,
											const QString& platform);
	QStringList projectIds(const QList<ShareCodes::Entry>& entries);
	QStringList fileIds(const QList<ShareCodes::Entry>& entries);
	/// {"fileIds": [...]} or {"modIds": [...]} for CurseForge, as numbers.
	QByteArray curseForgeIdsBody(const QString& key, const QStringList& ids);

	/// Adds one item per entry of @p wanted that @p versions (GET
	/// /v2/versions) has, its primary file; @p projects (GET /v2/projects)
	/// gives names and slugs. The rest goes to `missing`.
	void addModrinth(Resolved& resolved, const QList<ShareCodes::Entry>& wanted,
					 const QJsonArray& versions, const QJsonArray& projects);
	/// The same for CurseForge: @p files is `data` of POST /v1/mods/files,
	/// @p mods `data` of POST /v1/mods. A file whose author allows no
	/// third-party download keeps the website's address (browserDownloadOnly).
	void addCurseForge(Resolved& resolved,
					   const QList<ShareCodes::Entry>& wanted,
					   const QJsonArray& files, const QJsonArray& mods);
} // namespace ShareContent
