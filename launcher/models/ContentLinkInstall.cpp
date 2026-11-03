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

#include "ContentLinkInstall.h"

#include <QVariantMap>

#include "minecraft/MinecraftInstance.h"
#include "models/ContentBrowser.h"
#include "models/ContentInstallReview.h"
#include "modplatform/ContentProviderModel.h"
#include "tasks/TaskWatcher.h"

ContentLinkInstall::ContentLinkInstall(MinecraftInstance* instance,
									   DroppedContent::ProjectLink link,
									   QObject* parent)
	: QObject(parent), m_instance(instance), m_link(std::move(link))
{
}

void ContentLinkInstall::finish(bool ok, const QString& text)
{
	if (m_done) {
		return;
	}
	m_done = true;
	emit finished(ok, text);
	deleteLater();
}

void ContentLinkInstall::start()
{
	m_browser = new ContentBrowser(m_instance, this);
	m_browser->setProvider(m_link.provider);
	m_browser->setContentType(m_link.contentType);

	if (m_link.provider == QLatin1String("curseforge") &&
		m_browser->curseForgeKeyMissing()) {
		finish(false, tr("This build cannot look projects up on CurseForge."));
		return;
	}
	if (!m_browser->changesAllowed()) {
		finish(false, tr("The game is running. Content can be added once it "
						 "closes."));
		return;
	}
	if (!m_browser->installTargetAvailable()) {
		finish(false, tr("This instance's shared data pack folder is turned "
						 "off."));
		return;
	}

	m_model = qobject_cast<ContentProviderModel*>(m_browser->results());
	if (!m_model) {
		finish(false, tr("The project could not be looked up."));
		return;
	}
	connect(m_model, &ContentProviderModel::searchStateChanged, this,
			&ContentLinkInstall::onSearchState);
	connect(m_browser, &ContentBrowser::versionsChanged, this,
			&ContentLinkInstall::onVersions);

	if (m_link.provider == QLatin1String("modrinth")) {
		// The project endpoint takes a slug as well as an id.
		m_model->searchProject(m_link.slug);
	} else {
		// CurseForge's project endpoint wants a number the link does not carry.
		m_browser->setQuery(m_link.slug);
		m_browser->search();
	}
}

void ContentLinkInstall::onSearchState()
{
	if (m_searched || !m_model || m_model->isSearching()) {
		return;
	}
	m_searched = true;
	for (int row = 0; row < m_model->rowCount(QModelIndex()); ++row) {
		const auto* project = m_model->projectAt(row);
		if (project &&
			project->slug.compare(m_link.slug, Qt::CaseInsensitive) == 0) {
			m_row = row;
			m_name = project->name;
			break;
		}
	}
	if (m_row < 0) {
		finish(false, m_model->lastError().isEmpty()
						  ? tr("“%1” was not found for this instance.")
								.arg(m_link.slug)
						  : tr("“%1” could not be looked up. Are you online?")
								.arg(m_link.slug));
		return;
	}
	m_browser->loadVersions(m_row);
}

void ContentLinkInstall::onVersions()
{
	if (m_versionsHandled || m_row < 0 || m_browser->versionsLoading()) {
		return;
	}
	m_versionsHandled = true;

	QString versionId;
	for (const QVariant& entry : m_browser->versions()) {
		const QVariantMap version = entry.toMap();
		if (version.value(QStringLiteral("isCompatible")).toBool()) {
			versionId = version.value(QStringLiteral("id")).toString();
			break;
		}
	}
	if (versionId.isEmpty()) {
		const QString loader = m_browser->instanceLoader();
		finish(false, loader.isEmpty()
						  ? tr("“%1” has no version for Minecraft %2.")
								.arg(m_name, m_browser->instanceGameVersion())
						  : tr("“%1” has no version for Minecraft %2 with %3.")
								.arg(m_name, m_browser->instanceGameVersion(),
									 loader));
		return;
	}

	m_review = qobject_cast<ContentInstallReview*>(
		m_browser->review(m_row, versionId));
	if (!m_review) {
		finish(false, tr("“%1” cannot be installed here.").arg(m_name));
		return;
	}
	connect(m_review, &ContentInstallReview::stateChanged, this,
			&ContentLinkInstall::onReviewState);
	onReviewState();
}

void ContentLinkInstall::onReviewState()
{
	if (m_done || !m_review || !m_review->ready()) {
		return;
	}
	// Dependencies are settled with their defaults; the browse view is the
	// place to pick them by hand.
	auto* watcher = qobject_cast<TaskWatcher*>(m_review->install());
	m_review.clear();
	if (!watcher) {
		finish(false, tr("“%1” cannot be installed here.").arg(m_name));
		return;
	}
	connect(watcher, &TaskWatcher::finished, this, [this](bool ok) {
		finish(ok, ok ? tr("Added “%1”.").arg(m_name)
					  : tr("“%1” could not be installed.").arg(m_name));
	});
}
