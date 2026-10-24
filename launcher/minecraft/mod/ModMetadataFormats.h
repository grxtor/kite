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
#include <QJsonObject>
#include <QString>

#include <algorithm>
#include <memory>

#include "ModDetails.h"

/* The readers of the individual metadata formats, and what more than one of
 * them needs. Private to the ModMetadata*.cpp files: ModMetadataReader.h is
 * what the rest of the program sees. */
namespace ModMetadata::detail
{
	// What ModDetails::loaders and ModDependency::loader call each loader.
	inline const QString kLoaderFabric = QStringLiteral("fabric");
	inline const QString kLoaderQuilt = QStringLiteral("quilt");
	inline const QString kLoaderForge = QStringLiteral("forge");
	// Also the mod id a mods.toml requires to say it runs on NeoForge.
	inline const QString kLoaderNeoForge = QStringLiteral("neoforge");
	inline const QString kLoaderLiteLoader = QStringLiteral("liteloader");

	/* Another id a file answers to. One id for one file: the first
	 * declaration wins, and a mod listed twice is still one mod. */
	inline void addProvided(ModDetails& details, const ProvidedMod& mod)
	{
		if (mod.id.isEmpty()) {
			return;
		}
		const auto sameId = [&mod](const ProvidedMod& other) {
			return other.id == mod.id;
		};
		if (std::none_of(details.provides.cbegin(), details.provides.cend(),
						 sameId)) {
			details.provides.append(mod);
		}
	}

	// mods.toml and neoforge.mods.toml.
	std::shared_ptr<ModDetails> ReadMCModTOML(QByteArray contents,
											  bool neoforgeFile);
	// fabric.mod.json and quilt.mod.json.
	std::shared_ptr<ModDetails> ReadFabricModInfo(QByteArray contents);
	std::shared_ptr<ModDetails> ReadQuiltModInfo(QByteArray contents);
	/// The "quilt_loader" object of a quilt.mod.json; empty if it has none.
	QJsonObject quiltLoaderObject(const QByteArray& contents);
	// forgeversion.properties.
	std::shared_ptr<ModDetails> ReadForgeInfo(QByteArray contents);
} // namespace ModMetadata::detail
