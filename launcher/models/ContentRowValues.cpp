/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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

/* ContentFilterModel's per-file values: what a row shows, searches and
 * sorts by. Kept apart from the proxy plumbing in ContentFilterModel.cpp. */

#include "ContentFilterModel.h"

#include "minecraft/mod/Mod.h"
#include "minecraft/mod/ModIconCache.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/ContentApis.h"

QString ContentFilterModel::keyFor(const Mod& mod)
{
	return ModMetadataIndex::canonicalFileName(mod.filename().fileName());
}

QString ContentFilterModel::homepageFor(const Mod& mod,
										const ModMetadataIndex* index)
{
	if (index) {
		const ModMetadataIndex::Entry entry = index->get(keyFor(mod));
		if (entry.hasPlatformOrigin()) {
			const QString page =
				ModPlatform::projectPageUrlById(entry.platform, entry.projectId);
			if (!page.isEmpty()) {
				return page;
			}
		}
	}
	return mod.homeurl();
}

QString ContentFilterModel::sourceFor(const Mod& mod,
									  const ModMetadataIndex* index)
{
	if (!index) {
		return QString();
	}
	const ModMetadataIndex::Entry entry = index->get(keyFor(mod));
	if (!entry.hasPlatformOrigin()) {
		return QString();
	}
	return ModPlatform::displayNameById(entry.platform);
}

QString ContentFilterModel::nameFor(const Mod& mod,
									const ModMetadataIndex* index)
{
	const QString declared = mod.details().name;
	if (!declared.isEmpty() || !index) {
		return declared.isEmpty() ? mod.name() : declared;
	}
	const QString recorded = index->get(keyFor(mod)).name;
	return recorded.isEmpty() ? mod.name() : recorded;
}

QString ContentFilterModel::versionFor(const Mod& mod,
									   const ModMetadataIndex* index)
{
	const QString declared = mod.version();
	if (!declared.isEmpty() || !index) {
		return declared;
	}
	return index->get(keyFor(mod)).versionNumber;
}

QString ContentFilterModel::descriptionFor(const Mod& mod,
										   const ModMetadataIndex* index)
{
	const QString declared = mod.description();
	if (!declared.isEmpty() || !index) {
		return declared;
	}
	return index->get(keyFor(mod)).description;
}

QString ContentFilterModel::storedLogoUrl(const QString& url,
										  const QString& cacheDir)
{
	if (url.isEmpty()) {
		return QString();
	}
	const QString key = ModIconCache::keyForUrl(url);
	return ModIconCache::contains(cacheDir, key)
			   ? QStringLiteral("image://modicon/") + key
			   : QString();
}

bool ContentFilterModel::matches(const Mod& mod, const QString& text,
								 const ModMetadataIndex* index)
{
	if (text.isEmpty()) {
		return true;
	}
	if (nameFor(mod, index).contains(text, Qt::CaseInsensitive) ||
		descriptionFor(mod, index).contains(text, Qt::CaseInsensitive) ||
		mod.filename().fileName().contains(text, Qt::CaseInsensitive)) {
		return true;
	}
	for (const QString& author : mod.authors()) {
		if (author.contains(text, Qt::CaseInsensitive)) {
			return true;
		}
	}
	return false;
}
