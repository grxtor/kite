/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-License-Identifier: Apache-2.0
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

#include "ContentLogoFetch.h"

#include "core/LauncherContext.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModIconCache.h"
#include "modplatform/ContentInfoBackfill.h"
#include "net/Download.h"

ContentLogoFetch::ContentLogoFetch(std::shared_ptr<ModFolderModel> folder,
								   QObject* parent)
	: QObject(parent), m_folder(std::move(folder))
{
}

ContentLogoFetch::~ContentLogoFetch() = default;

void ContentLogoFetch::start()
{
	if (m_folder && LAUNCHER) {
		const QString cacheDir = m_folder->iconCacheDir();
		const auto index = m_folder->metadataIndex();
		for (size_t row = 0; row < m_folder->size(); ++row) {
			const Mod& mod = m_folder->at(row);
			/* Unread files may still turn out to have their own logo. */
			if (!mod.isResolved() || !mod.iconKey().isEmpty()) {
				continue;
			}
			const QString key =
				ModMetadataIndex::canonicalFileName(mod.filename().fileName());
			const QString url = index->get(key).iconUrl;
			if (!url.startsWith(QLatin1String("https://")) ||
				ModIconCache::contains(cacheDir, ModIconCache::keyForUrl(url))) {
				continue;
			}
			if (m_keys.contains(url) ||
				ContentInfo::firstAskThisSession(QStringLiteral("logo:") + url)) {
				m_keys[url].append(key);
			}
		}
	}
	m_urls = m_keys.keys();
	if (m_urls.isEmpty()) {
		deleteLater();
		return;
	}
	m_bodies = QList<QByteArray>(m_urls.size());
	m_job = NetJob::Ptr(
		new NetJob(QStringLiteral("Content logos"), LAUNCHER->network()));
	for (int i = 0; i < m_urls.size(); ++i) {
		m_job->addNetAction(
			Net::Download::makeByteArray(QUrl(m_urls.at(i)), &m_bodies[i]));
	}
	connect(m_job.get(), &Task::finished, this, &ContentLogoFetch::finish);
	m_job->start();
}

void ContentLogoFetch::finish()
{
	QStringList keys;
	const QString cacheDir = m_folder->iconCacheDir();
	for (int i = 0; i < m_urls.size(); ++i) {
		/* A failed part may have left half an image behind. */
		if (m_job->at(i)->wasSuccessful() &&
			ModIconCache::store(cacheDir, ModIconCache::keyForUrl(m_urls.at(i)),
								m_bodies.at(i))) {
			keys.append(m_keys.value(m_urls.at(i)));
		}
	}
	if (!keys.isEmpty()) {
		emit stored(keys);
	}
	deleteLater();
}
