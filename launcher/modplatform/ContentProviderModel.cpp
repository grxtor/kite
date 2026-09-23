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

#include "ContentProviderModel.h"

#include <QDebug>
#include <QSize>
#include <QUrl>
#include <utility>

#include "core/LauncherContext.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"

ContentProviderModel::ContentProviderModel(const ModPlatform::ContentApi& api,
										   ModPlatform::ContentType contentType,
										   ModPlatform::SearchFilters filters,
										   QObject* parent)
	: QAbstractListModel(parent), m_api(api), m_contentType(contentType),
	  m_filters(std::move(filters))
{
}

ContentProviderModel::~ContentProviderModel()
{
	/* The search job writes straight into a member buffer, so it must
	 * not outlive us. */
	if (m_searchJob) {
		m_searchJob->abort();
	}
}

int ContentProviderModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : m_visibleRows.size();
}

int ContentProviderModel::columnCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : 1;
}

QVariant ContentProviderModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid()) {
		return QVariant();
	}
	const auto* entry = projectAt(index.row());
	if (entry == nullptr) {
		return QVariant();
	}
	const auto& project = *entry;

	switch (role) {
		case Qt::DisplayRole:
			/* Left as the plain name: if a view is ever shown without
			 * ProjectItemDelegate attached, the default delegate paints
			 * this and the list stays readable. */
			return project.name;

		case Qt::ToolTipRole:
			if (project.description.length() > 100) {
				QString shortened = project.description.left(97);
				return shortened.left(shortened.lastIndexOf(" "))
					.append("...");
			}
			return project.description;

		case Qt::DecorationRole: {
			if (m_logoMap.contains(project.logoKey)) {
				return m_logoMap.value(project.logoKey);
			}
			const_cast<ContentProviderModel*>(this)->requestLogo(
				project.logoKey, project.logoUrl);
			return LAUNCHER->getThemedIcon("screenshot-placeholder");
		}

		case Qt::SizeHintRole:
			/* Fixed row height: enough for the title plus two lines of
			 * description, independent of the view's icon size. */
			return QSize(0, 58);

		case Qt::CheckStateRole:
			/* Every row gets a box, installed or not. Re-picking a row
			 * that is already on disk is how a version is changed: the
			 * conflict analyzer turns it into a replacement, or drops it
			 * when the same version was chosen. Whether it is installed
			 * only affects how the row is painted, which the delegate
			 * asks for separately through ProjectItemRole::Installed. */
			return m_selectedNames.contains(project.normalizedName)
					   ? Qt::Checked
					   : Qt::Unchecked;

		case Qt::UserRole:
			return project.projectId;

		case ProjectItemRole::Title:
			return project.name;

		case ProjectItemRole::Description:
			/* Full text, unlike the pre-truncated tooltip above: the
			 * delegate does its own wrapping and eliding to fit. */
			return project.description;

		case ProjectItemRole::Installed:
			return project.installed;

		case LogoKeyRole:
			return project.logoKey;

		case LogoUrlRole:
			return project.logoUrl;

		case AuthorRole:
			return project.author;

		default:
			break;
	}

	return QVariant();
}

QHash<int, QByteArray> ContentProviderModel::roleNames() const
{
	QHash<int, QByteArray> roles = QAbstractListModel::roleNames();
	roles.insert(Qt::UserRole, "projectId");
	roles.insert(ProjectItemRole::Title, "title");
	roles.insert(ProjectItemRole::Description, "description");
	roles.insert(ProjectItemRole::Installed, "installed");
	roles.insert(LogoKeyRole, "logoKey");
	roles.insert(LogoUrlRole, "logoUrl");
	roles.insert(AuthorRole, "author");
	return roles;
}

Qt::ItemFlags ContentProviderModel::flags(const QModelIndex& index) const
{
	/* Deliberately no Qt::ItemIsUserCheckable: this model has no
	 * setData(), and the flag would make the base delegate try to toggle
	 * through it. The checkbox is driven by the delegate's
	 * checkboxClicked() signal instead. */
	return QAbstractListModel::flags(index);
}

bool ContentProviderModel::canFetchMore(const QModelIndex& parent) const
{
	return !parent.isValid() && m_searchState == CanPossiblyFetchMore;
}

void ContentProviderModel::fetchMore(const QModelIndex& parent)
{
	if (parent.isValid() || m_nextSearchOffset == 0) {
		return;
	}
	performPaginatedSearch();
}

QString ContentProviderModel::platformId() const
{
	return m_api.id();
}

QString ContentProviderModel::platformDisplayName() const
{
	return m_api.displayName();
}

QList<ModPlatform::SortingMethod> ContentProviderModel::sortingMethods() const
{
	return m_api.sortingMethods();
}

bool ContentProviderModel::supportsExtendedFilters() const
{
	return m_api.supportsExtendedFilters();
}

void ContentProviderModel::loadCategories()
{
	if (m_categoriesRequested) {
		return;
	}
	m_categoriesRequested = true;

	auto response = std::make_shared<QByteArray>();
	auto* job = new NetJob(QString("%1::Categories").arg(m_api.id()),
						   LAUNCHER->network());
	job->addNetAction(Net::Download::makeByteArray(
		m_api.categoriesUrl(m_contentType), response.get()));

	connect(job, &NetJob::succeeded, this, [this, job, response] {
		job->deleteLater();
		const QList<ModPlatform::Category> categories =
			parseCategoriesResponse(*response);
		if (!categories.isEmpty()) {
			emit categoriesLoaded(categories);
		}
	});
	connect(job, &NetJob::failed, this, [job, response](QString reason) {
		job->deleteLater();
		/* Not worth bothering the user with: the panel just goes
		 * without its category group. */
		qWarning() << "Could not load categories:" << reason;
	});

	job->start();
}

const ModPlatform::IndexedProject*
ContentProviderModel::projectAt(int row) const
{
	if (row < 0 || row >= m_visibleRows.size()) {
		return nullptr;
	}
	return &m_projects.at(m_visibleRows.at(row));
}

bool ContentProviderModel::passesRowFilter(
	const ModPlatform::IndexedProject& project) const
{
	return !m_hideInstalled || !project.installed;
}

int ContentProviderModel::viewRowOfStorage(int storageIndex) const
{
	return int(m_visibleRows.indexOf(storageIndex));
}

void ContentProviderModel::rebuildVisibleRows()
{
	beginResetModel();
	m_visibleRows.clear();
	for (int i = 0; i < m_projects.size(); ++i) {
		if (passesRowFilter(m_projects.at(i))) {
			m_visibleRows.append(i);
		}
	}
	endResetModel();
}

void ContentProviderModel::setHideInstalled(bool hide)
{
	if (m_hideInstalled == hide) {
		return;
	}
	m_hideInstalled = hide;
	rebuildVisibleRows();
}

void ContentProviderModel::setSearchFilters(
	const ModPlatform::SearchFilters& filters)
{
	if (m_filters == filters) {
		return;
	}

	m_filters = filters;

	/* Nothing has been asked for yet, so there is nothing to redo - the
	 * first search will pick the new filters up by itself. */
	if (!m_searched) {
		return;
	}

	if (m_searchJob) {
		m_searchState = ResetRequested;
		m_searchJob->abort();
		return;
	}

	restartSearch();
}

void ContentProviderModel::search(const QString& term, int sortIndex)
{
	/* A repeat of the query that is already on screen is ignored, but
	 * only when the rows really are the answer to a search: coming back
	 * from a single-project lookup has to run the search even though the
	 * term did not change. */
	if (m_searched && m_projectLookupId.isEmpty() && m_searchTerm == term &&
		m_sortIndex == sortIndex) {
		return;
	}

	m_searched = true;
	m_searchTerm = term;
	m_sortIndex = sortIndex;
	m_projectLookupId.clear();

	if (m_searchJob) {
		/* A reply for the previous query is still on its way. Mark the
		 * intent first, then abort: aborting can complete synchronously,
		 * and the handler needs to see the new state. */
		m_searchState = ResetRequested;
		m_searchJob->abort();
		return;
	}

	restartSearch();
}

void ContentProviderModel::searchProject(const QString& projectId)
{
	if (projectId.isEmpty()) {
		return;
	}

	m_searched = true;
	/* No term to speak of, and clearing it means a later search() for
	 * whatever is in the search box is never mistaken for a repeat. */
	m_searchTerm.clear();
	m_projectLookupId = projectId;

	if (m_searchJob) {
		m_searchState = ResetRequested;
		m_searchJob->abort();
		return;
	}

	restartSearch();
}

void ContentProviderModel::restartSearch()
{
	beginResetModel();
	m_projects.clear();
	m_visibleRows.clear();
	endResetModel();

	m_generation++;
	m_searchState = None;
	m_nextSearchOffset = 0;
	m_lastError.clear();
	performPaginatedSearch();
}

void ContentProviderModel::performPaginatedSearch()
{
	m_searchResponse.clear();

	QUrl url;
	QString jobName;
	if (m_projectLookupId.isEmpty()) {
		ModPlatform::SearchQuery query;
		query.contentType = m_contentType;
		query.term = m_searchTerm;
		query.filters = m_filters;
		query.sortIndex = m_sortIndex;
		query.offset = m_nextSearchOffset;

		url = m_api.searchUrl(query);
		jobName = QString("%1::Search").arg(m_api.id());
	} else {
		url = m_api.projectUrl(m_projectLookupId);
		jobName = QString("%1::Project(%2)")
					  .arg(m_api.id(), m_projectLookupId);
	}

	auto* job = new NetJob(jobName, LAUNCHER->network());
	job->addNetAction(
		Net::Download::makeByteArray(url, &m_searchResponse));

	m_searchJob = job;
	connect(job, &NetJob::succeeded, this,
			&ContentProviderModel::searchRequestFinished);
	connect(job, &NetJob::failed, this, [this](QString reason) {
		m_lastError = reason;
		searchRequestFailed();
	});

	job->start();
	emit searchStateChanged();
}

void ContentProviderModel::searchRequestFinished()
{
	m_searchJob.reset();

	if (m_searchState == ResetRequested) {
		/* These results answer a query the user has already moved on
		 * from. Throw them away and ask again with the current one. */
		searchRequestFailed();
		return;
	}

	m_lastError.clear();

	int totalHits = -1;
	QList<ModPlatform::IndexedProject> newList;
	if (m_projectLookupId.isEmpty()) {
		newList = parseSearchResponse(m_searchResponse, totalHits);
	} else {
		/* One project, or none if the reply held nothing usable. Either
		 * way this is the whole answer, so the size below - being under
		 * a page - settles the paging state as Finished. */
		auto project = parseProjectResponse(m_searchResponse);
		if (!project.projectId.isEmpty()) {
			newList.append(project);
		}
		totalHits = newList.size();
	}

	for (auto& project : newList) {
		project.normalizedName = ModMetadataIndex::normalizeName(project.name);
		applyInstalledState(project);
	}

	if (!newList.isEmpty()) {
		/* Work out which of the new results the filter lets through
		 * before touching the model, so the inserted range is right. */
		QList<int> added;
		const int firstStorage = m_projects.size();
		for (int i = 0; i < newList.size(); ++i) {
			if (passesRowFilter(newList.at(i))) {
				added.append(firstStorage + i);
			}
		}

		if (added.isEmpty()) {
			m_projects.append(newList);
		} else {
			const int firstRow = m_visibleRows.size();
			beginInsertRows(QModelIndex(), firstRow,
							firstRow + added.size() - 1);
			m_projects.append(newList);
			m_visibleRows.append(added);
			endInsertRows();
		}
	}

	const int pageSize = m_api.searchPageSize();
	const bool lastPage =
		newList.size() < pageSize ||
		(totalHits >= 0 && (m_nextSearchOffset + newList.size()) >= totalHits);

	if (lastPage) {
		m_searchState = Finished;
	} else {
		m_nextSearchOffset += pageSize;
		m_searchState = CanPossiblyFetchMore;
	}

	emit searchStateChanged();
}

void ContentProviderModel::searchRequestFailed()
{
	m_searchJob.reset();

	if (m_searchState == ResetRequested) {
		restartSearch();
		return;
	}

	/* Deliberately not the reference launcher's behaviour for a
	 * single-project lookup that 404s: it flips the model back into a
	 * plain search and asks the provider for the literal string "#<id>",
	 * which returns whatever that happens to match. There is nothing to
	 * show for a project that is gone, so the list stays empty and the
	 * page says so. */
	m_searchState = Finished;
	emit searchStateChanged();
}

int ContentProviderModel::storageIndexOf(const QString& projectId) const
{
	for (int i = 0; i < m_projects.size(); ++i) {
		if (m_projects.at(i).projectId == projectId) {
			return i;
		}
	}
	return -1;
}

void ContentProviderModel::loadEntry(int viewRow)
{
	if (viewRow < 0 || viewRow >= m_visibleRows.size()) {
		return;
	}
	const int row = m_visibleRows.at(viewRow);

	const QString projectId = m_projects.at(row).projectId;
	const quint64 generation = m_generation;

	if (!m_projects.at(row).versionsLoaded &&
		!m_projects.at(row).versionsLoading) {
		m_projects[row].versionsLoading = true;

		ModPlatform::VersionQuery query;
		query.projectId = projectId;
		query.contentType = m_contentType;
		query.mcVersions = m_filters.mcVersions;
		query.loaders = m_filters.loaders;

		/* Held by both handlers rather than deleted by hand in each: only
		 * one of them runs, and if the job is destroyed without either
		 * firing the buffer still goes with it. */
		auto response = std::make_shared<QByteArray>();
		auto* job = new NetJob(QString("%1::Versions(%2)")
								   .arg(m_api.id(), projectId),
							   LAUNCHER->network());
		job->addNetAction(Net::Download::makeByteArray(
			m_api.projectVersionsUrl(query), response.get()));

		connect(job, &NetJob::succeeded, this,
				[this, job, response, generation, projectId] {
					job->deleteLater();
					applyVersions(generation, projectId, *response);
				});
		/* `response` is captured here too, unused, so that the buffer is
		 * plainly owned by the job's handlers whichever one runs. */
		connect(job, &NetJob::failed, this,
				[this, job, response, generation, projectId](QString) {
					job->deleteLater();
					/* Empty payload: the row is marked as loaded with no
					 * versions, which the page shows as "no valid
					 * version found" rather than spinning forever. */
					applyVersions(generation, projectId, QByteArray());
				});
		job->start();
	}

	if (!m_projects.at(row).bodyLoaded && !m_projects.at(row).bodyLoading) {
		m_projects[row].bodyLoading = true;

		auto response = std::make_shared<QByteArray>();
		auto* job = new NetJob(
			QString("%1::Description(%2)").arg(m_api.id(), projectId),
			LAUNCHER->network());
		job->addNetAction(Net::Download::makeByteArray(
			m_api.projectBodyUrl(projectId), response.get()));

		connect(job, &NetJob::succeeded, this,
				[this, job, response, generation, projectId] {
					job->deleteLater();
					applyBody(generation, projectId, *response);
				});
		connect(job, &NetJob::failed, this,
				[this, job, response, generation, projectId](QString) {
					job->deleteLater();
					applyBody(generation, projectId, QByteArray());
				});
		job->start();
	}
}

void ContentProviderModel::applyVersions(quint64 generation,
										 const QString& projectId,
										 const QByteArray& bytes)
{
	/* The list may have been replaced, or the row may have moved, while
	 * this was in flight. */
	if (generation != m_generation) {
		return;
	}
	const int storage = storageIndexOf(projectId);
	if (storage < 0) {
		return;
	}

	auto& project = m_projects[storage];
	project.versionsLoading = false;
	project.versionsLoaded = true;
	if (!bytes.isEmpty()) {
		project.versions = parseVersionsResponse(bytes, project);
	}

	const int row = viewRowOfStorage(storage);
	if (row < 0) {
		/* Filtered out of the list while we were waiting; the data is
		 * kept, but nobody is looking at it. */
		return;
	}

	emit dataChanged(index(row), index(row));
	emit entryUpdated(row);
}

void ContentProviderModel::applyBody(quint64 generation,
									 const QString& projectId,
									 const QByteArray& bytes)
{
	if (generation != m_generation) {
		return;
	}
	const int storage = storageIndexOf(projectId);
	if (storage < 0) {
		return;
	}

	auto& project = m_projects[storage];
	project.bodyLoading = false;
	project.bodyLoaded = true;
	if (!bytes.isEmpty()) {
		project.bodyHtml = parseBodyResponse(bytes);
	}

	const int row = viewRowOfStorage(storage);
	if (row >= 0) {
		emit entryUpdated(row);
	}
}

void ContentProviderModel::setInstalledIndex(
	std::shared_ptr<ModMetadataIndex> index)
{
	m_installedIndex = std::move(index);
	for (auto& project : m_projects) {
		applyInstalledState(project);
	}

	if (m_hideInstalled) {
		/* Which rows are visible just changed under us. */
		rebuildVisibleRows();
		return;
	}
	emitRowStateChanged();
}

void ContentProviderModel::setSelectedNames(const QSet<QString>& names)
{
	if (m_selectedNames == names) {
		return;
	}
	m_selectedNames = names;
	emitRowStateChanged();
}

void ContentProviderModel::applyInstalledState(
	ModPlatform::IndexedProject& project) const
{
	project.installed = false;
	if (!m_installedIndex) {
		return;
	}

	/* Two-step lookup: the exact project first, then the name key -
	 * which also catches the same mod installed from the other site. */
	if (!project.projectId.isEmpty()) {
		const auto byProject =
			m_installedIndex->findByPlatformProject(m_api.id(),
													project.projectId);
		if (byProject.isValid()) {
			project.installed = true;
			return;
		}
	}

	if (!project.normalizedName.isEmpty()) {
		project.installed =
			m_installedIndex->findByNormalizedName(project.normalizedName)
				.isValid();
	}
}

void ContentProviderModel::emitRowStateChanged()
{
	if (m_visibleRows.isEmpty()) {
		return;
	}
	emit dataChanged(index(0), index(m_visibleRows.size() - 1),
					 {Qt::CheckStateRole, ProjectItemRole::Installed});
}

void ContentProviderModel::requestLogo(const QString& key, const QString& url)
{
	if (key.isEmpty() || url.isEmpty() || m_loadingLogos.contains(key) ||
		m_failedLogos.contains(key)) {
		return;
	}

	MetaEntryPtr entry = LAUNCHER->metacache()->resolveEntry(
		iconCacheName(), QString("logos/%1").arg(key.section(".", 0, 0)));

	auto* job = new NetJob(QString("%1 Icon %2").arg(m_api.id(), key),
						   LAUNCHER->network());
	job->addNetAction(Net::Download::makeCached(QUrl(url), entry));

	const QString fullPath = entry->getFullPath();
	connect(job, &NetJob::succeeded, this, [this, job, key, fullPath] {
		job->deleteLater();
		logoLoaded(key, QIcon(fullPath));
	});
	connect(job, &NetJob::failed, this, [this, job, key](QString) {
		job->deleteLater();
		logoFailed(key);
	});

	job->start();
	m_loadingLogos.append(key);
}

void ContentProviderModel::logoLoaded(const QString& key, const QIcon& icon)
{
	m_loadingLogos.removeAll(key);
	m_logoMap.insert(key, icon);

	for (int row = 0; row < m_visibleRows.size(); ++row) {
		if (m_projects.at(m_visibleRows.at(row)).logoKey == key) {
			emit dataChanged(index(row), index(row), {Qt::DecorationRole});
		}
	}
}

void ContentProviderModel::logoFailed(const QString& key)
{
	m_loadingLogos.removeAll(key);
	m_failedLogos.append(key);
}
