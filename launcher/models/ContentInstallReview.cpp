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

#include "ContentInstallReview.h"

#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/ContentDownloadTask.h"
#include "modplatform/DependencyResolver.h"
#include "modplatform/ModInstallConflictAnalyzer.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "tasks/TaskWatcher.h"

ContentInstallReview::ContentInstallReview(
	const ModPlatform::SelectedMod& mod, const QString& mcVersion,
	const QString& loader, std::shared_ptr<ModFolderModel> folder,
	bool resolveDependencies, QObject* parent)
	: QAbstractListModel(parent), m_resolveFor({mod}), m_title(mod.name),
	  m_primaryCount(1), m_mcVersion(mcVersion), m_loader(loader),
	  m_folder(std::move(folder)), m_resolveDependencies(resolveDependencies)
{
	/* The project's own row: what was picked, always installed. Same
	 * field-by-field copy DownloadSummaryDialog makes. */
	Row own;
	own.item.name = mod.name;
	own.item.fileName = mod.fileName;
	own.item.downloadUrl = mod.downloadUrl;
	own.item.sha1 = mod.sha1;
	own.item.fileSize = mod.fileSize;
	own.item.isDependency = false;
	own.item.platform = mod.platform;
	own.item.projectId = mod.projectId;
	own.item.versionId = mod.versionId;
	own.item.slug = mod.slug;
	own.item.browserDownloadOnly = mod.browserDownloadOnly;
	own.provider = providerName(mod.platform);
	own.versionType = mod.versionType;
	m_rows.append(own);
}

ContentInstallReview::ContentInstallReview(
	const QList<ModPlatform::SelectedMod>& resolveFor, const QString& title,
	const QString& mcVersion, const QString& loader,
	std::shared_ptr<ModFolderModel> folder, QObject* parent)
	: QAbstractListModel(parent), m_resolveFor(resolveFor), m_title(title),
	  m_primaryCount(0), m_mcVersion(mcVersion), m_loader(loader),
	  m_folder(std::move(folder)), m_resolveDependencies(true)
{
}

ContentInstallReview* ContentInstallReview::missingDependencies(
	const QList<ModPlatform::SelectedMod>& installed, const QString& title,
	const QString& mcVersion, const QString& loader,
	std::shared_ptr<ModFolderModel> folder, QObject* parent)
{
	return new ContentInstallReview(installed, title, mcVersion, loader,
									std::move(folder), parent);
}

ContentInstallReview::~ContentInstallReview()
{
	if (m_resolver && m_resolver->isRunning()) {
		m_resolver->abort();
	}
}

QString ContentInstallReview::providerName(const QString& platform)
{
	if (platform == ModrinthApi::get().id()) {
		return ModrinthApi::get().displayName();
	}
	if (platform == FlameApi::get().id()) {
		return FlameApi::get().displayName();
	}
	return platform;
}

bool ContentInstallReview::needsReview() const
{
	return dependencyCount() > 0 || !m_unresolved.isEmpty();
}

int ContentInstallReview::dependencyCount() const
{
	return int(m_rows.size()) - m_primaryCount;
}

int ContentInstallReview::checkedCount() const
{
	int n = 0;
	for (const Row& row : m_rows) {
		if (row.checked) {
			++n;
		}
	}
	return n;
}

int ContentInstallReview::checkedDependencyCount() const
{
	int n = 0;
	for (int row = m_primaryCount; row < m_rows.size(); ++row) {
		if (m_rows.at(row).checked) {
			++n;
		}
	}
	return n;
}

QStringList ContentInstallReview::unresolved() const
{
	QStringList names;
	for (const auto& dep : m_unresolved) {
		names.append(dep.name.isEmpty() ? dep.projectId : dep.name);
	}
	return names;
}

int ContentInstallReview::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : int(m_rows.size());
}

QVariant ContentInstallReview::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
		return QVariant();
	}
	const Row& row = m_rows.at(index.row());
	switch (role) {
		case Qt::DisplayRole:
		case NameRole:
			return row.item.name;
		case FileNameRole:
			return row.item.fileName;
		case ProviderRole:
			return row.provider;
		case RequiredByRole:
			return row.requiredBy.join(QStringLiteral(", "));
		case IsDependencyRole:
			return row.item.isDependency;
		case CheckedRole:
			return row.checked;
		case MaybeInstalledRole:
			return row.maybeInstalled;
		case BrowserOnlyRole:
			return row.item.browserDownloadOnly;
		case VersionTypeRole:
			return row.versionType;
		default:
			break;
	}
	return QVariant();
}

QHash<int, QByteArray> ContentInstallReview::roleNames() const
{
	return {
		{NameRole, "name"},
		{FileNameRole, "fileName"},
		{ProviderRole, "provider"},
		{RequiredByRole, "requiredBy"},
		{IsDependencyRole, "isDependency"},
		{CheckedRole, "checked"},
		{MaybeInstalledRole, "maybeInstalled"},
		{BrowserOnlyRole, "browserOnly"},
		{VersionTypeRole, "versionType"},
	};
}

void ContentInstallReview::start()
{
	if (m_resolving || m_ready || m_finished) {
		return;
	}
	if (!m_resolveDependencies) {
		applyResolution({}, {});
		return;
	}

	m_resolving = true;
	emit stateChanged();

	m_resolver = new DependencyResolver(m_resolveFor, m_mcVersion, m_loader, this);
	if (m_folder) {
		m_resolver->setInstalledIndex(m_folder->metadataIndex());
	}
	/* A failure is not the end of the install - the widget's Skip does
	 * not end it either: whatever was resolved before it is offered. An
	 * abort reports through failed() too. */
	connect(m_resolver, &Task::succeeded, this, &ContentInstallReview::onResolved);
	connect(m_resolver, &Task::failed, this, [this](QString) { onResolved(); });
	m_resolver->start();
}

void ContentInstallReview::onResolved()
{
	if (!m_resolver || m_finished) {
		return;
	}
	applyResolution(m_resolver->resolvedDependencies(),
					m_resolver->unresolvedDependencies());
}

void ContentInstallReview::applyResolution(
	const QList<ModPlatform::DependencyInfo>& dependencies,
	const QList<ModPlatform::UnresolvedDep>& unresolved)
{
	beginResetModel();
	while (m_rows.size() > m_primaryCount) {
		m_rows.removeLast();
	}
	for (const auto& dep : dependencies) {
		Row row;
		row.item.name = dep.name;
		row.item.fileName = dep.fileName;
		row.item.downloadUrl = dep.downloadUrl;
		row.item.sha1 = dep.sha1;
		row.item.fileSize = dep.fileSize;
		row.item.isDependency = true;
		row.item.platform = dep.platform;
		row.item.projectId = dep.projectId;
		row.item.versionId = dep.versionId;
		row.item.slug = dep.slug;
		row.item.browserDownloadOnly = dep.browserDownloadOnly;
		row.provider = providerName(dep.platform);
		row.requiredBy = dep.requiredBy;
		row.versionType = dep.versionType;
		row.maybeInstalled = dep.maybeInstalled;
		/* "Unticked because a version of this is already installed." */
		row.checked = !dep.maybeInstalled;
		m_rows.append(row);
	}
	m_unresolved = unresolved;
	endResetModel();

	m_resolving = false;
	m_ready = true;
	emit stateChanged();
	emit checkedChanged();
}

QList<ModPlatform::DownloadItem> ContentInstallReview::downloadItems() const
{
	QList<ModPlatform::DownloadItem> items;
	for (const Row& row : m_rows) {
		if (row.checked) {
			items.append(row.item);
		}
	}
	return items;
}

void ContentInstallReview::setChecked(int row, bool checked)
{
	/* The picked project's row is what the install is for. */
	if (row < m_primaryCount || row >= m_rows.size() ||
		m_rows[row].checked == checked) {
		return;
	}
	m_rows[row].checked = checked;
	emit dataChanged(index(row), index(row), {CheckedRole});
	emit checkedChanged();
}

void ContentInstallReview::setAllDependenciesChecked(bool checked)
{
	for (int row = m_primaryCount; row < m_rows.size(); ++row) {
		setChecked(row, checked);
	}
}

QObject* ContentInstallReview::install()
{
	const auto items = downloadItems();
	if (!m_ready || m_finished || !m_folder || items.isEmpty()) {
		return nullptr;
	}
	m_finished = true;

	auto index = m_folder->metadataIndex();
	const auto decisions = ModInstallConflictAnalyzer::analyze(items, index);
	const auto plan = ModInstallConflictAnalyzer::toDownloadPlan(decisions);

	auto* task =
		new ContentDownloadTask(plan, m_folder->dir().absolutePath());
	task->setMetadataIndex(index);

	/* Parented to the browser, not to this review, which ends below. */
	auto* watcher = new TaskWatcher(Task::Ptr(task), parent());
	watcher->setTitle(m_title);

	/* Refresh the list afterwards whatever happened - some files may
	 * have landed before the rest failed - as ModFolderPage::
	 * reviewAndInstall() does. */
	auto folder = m_folder;
	connect(watcher, &TaskWatcher::finished, watcher,
			[folder](bool) { folder->update(); });

	task->start();
	deleteLater();
	return watcher;
}

void ContentInstallReview::cancel()
{
	if (m_finished) {
		return;
	}
	m_finished = true;
	if (m_resolver && m_resolver->isRunning()) {
		m_resolver->abort();
	}
	deleteLater();
}
