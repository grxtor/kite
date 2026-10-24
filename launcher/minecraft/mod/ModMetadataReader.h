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
#include <QHash>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

#include "ModDetails.h"

/* What a mod file says about itself: the metadata files of a jar, read and
 * merged into one ModDetails (nested jars included), and the readers of the
 * two formats LocalModParseTask reads on their own. */
namespace ModMetadata
{
	using DetailsPtr = std::shared_ptr<ModDetails>;
	using ZipEntries = QHash<QString, QByteArray>;
	/// Reads the named files of one jar (the outer one, or a nested one held
	/// in memory); the names it does not find are left out.
	using EntryReader = std::function<ZipEntries(const QStringList&)>;

	/// Every metadata file a jar may carry; these are the files readDetails()
	/// wants the contents of.
	extern const QStringList kMetadataEntries;

	/// Everything a jar's metadata says, from the contents of its metadata
	/// files (see kMetadataEntries) and a way to read more files out of the
	/// same jar. A jar with no metadata of its own but mods inside it comes
	/// back with only `provides` filled in. Null when it has neither.
	DetailsPtr readDetails(const ZipEntries& files, const EntryReader& read);

	/// @p path from metadata (a logo, a nested jar) the way the archive
	/// spells that entry: forward slashes, no leading "/" or "./".
	QString archivePath(QString path);

	/// mcmod.info on its own, which is all a mod folder is read for.
	std::shared_ptr<ModDetails> ReadMCModInfo(QByteArray contents);
	/// litemod.json.
	std::shared_ptr<ModDetails> ReadLiteModInfo(QByteArray contents);
} // namespace ModMetadata
