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

#include "ModrinthModpackModel.h"

#include <algorithm>

#include <QColor>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "Json.h"
#include "core/LauncherContext.h"
#include "icons/IconList.h"
#include "modplatform/ContentApi.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "tasks/TaskWatcher.h"

/* ---------------------------------------------------------------- */
/* ModrinthModpackDetail                                             */
/* ---------------------------------------------------------------- */

ModrinthModpackDetail::ModrinthModpackDetail(QObject* parent) : QObject(parent)
{
}

void ModrinthModpackDetail::reset(const QString& projectId)
{
	if (m_projectId != projectId) {
		m_projectId = projectId;
		emit projectIdChanged();
	}
	setTitle(QString());
	setBody(QString());
	setVersions(QVariantList());
	setGallery(QVariantList());
	setError(QString());
	setLoading(true);
}

void ModrinthModpackDetail::setTitle(const QString& title)
{
	if (m_title == title) {
		return;
	}
	m_title = title;
	emit titleChanged();
}

void ModrinthModpackDetail::setBody(const QString& body)
{
	if (m_body == body) {
		return;
	}
	m_body = body;
	emit bodyChanged();
}

void ModrinthModpackDetail::setVersions(const QVariantList& versions)
{
	m_versions = versions;
	emit versionsChanged();
}

void ModrinthModpackDetail::setGallery(const QVariantList& gallery)
{
	m_gallery = gallery;
	emit galleryChanged();
}

void ModrinthModpackDetail::setLoading(bool loading)
{
	if (m_loading == loading) {
		return;
	}
	m_loading = loading;
	emit loadingChanged();
}

void ModrinthModpackDetail::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}

/* ---------------------------------------------------------------- */
/* ModrinthModpackModel                                               */
/* ---------------------------------------------------------------- */

ModrinthModpackModel::ModrinthModpackModel(QObject* parent)
	: QAbstractListModel(parent), m_detail(new ModrinthModpackDetail(this))
{
	for (const auto& sorting : ModrinthApi::get().sortingMethods()) {
		QVariantMap entry;
		entry[QStringLiteral("id")] = sorting.apiValue;
		entry[QStringLiteral("label")] = sorting.readableName;
		m_sortOptions.append(entry);
	}
	if (!m_sortOptions.isEmpty()) {
		m_sort = m_sortOptions.first().toMap().value(QStringLiteral("id"))
					 .toString();
	}
}

ModrinthModpackModel::~ModrinthModpackModel()
{
	if (m_searchJob) {
		m_searchJob->abort();
	}
	if (m_bodyJob) {
		m_bodyJob->abort();
	}
	if (m_versionsJob) {
		m_versionsJob->abort();
	}
}

int ModrinthModpackModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : m_packs.size();
}

QVariant ModrinthModpackModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 ||
		index.row() >= m_packs.size()) {
		return QVariant();
	}
	const Modrinth::IndexedPack& pack = m_packs.at(index.row());

	switch (role) {
		case ProjectIdRole:
			return pack.projectId;
		case SlugRole:
			return pack.slug;
		case TitleRole:
			return pack.name;
		case DescriptionRole:
			return pack.description;
		case AuthorRole:
			return pack.author;
		case LogoUrlRole:
			return pack.iconUrl;
		case DownloadsRole:
			return pack.downloads;
		case FollowsRole:
			return pack.follows;
		case UpdatedRole:
			return pack.dateModified;
		case LatestVersionRole:
			return pack.latestVersion;
		case GameVersionsRole:
			return QVariant::fromValue(pack.gameVersions);
		case CategoriesRole:
			/* display_categories when the hit had it - the curated
			 * set Modrinth means for a card - falling back to the
			 * full technical list otherwise. */
			return QVariant::fromValue(pack.displayCategories.isEmpty()
										   ? pack.categories
										   : pack.displayCategories);
		case GalleryUrlRole:
			if (!pack.featuredGalleryUrl.isEmpty()) {
				return pack.featuredGalleryUrl;
			}
			return pack.galleryUrls.isEmpty() ? QString()
											  : pack.galleryUrls.first();
		case AccentColorRole:
			if (pack.color < 0) {
				return QVariant();
			}
			return QVariant::fromValue(
				QColor::fromRgb((pack.color >> 16) & 0xFF,
								(pack.color >> 8) & 0xFF, pack.color & 0xFF));
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> ModrinthModpackModel::roleNames() const
{
	return {
		{ProjectIdRole, "projectId"},
		{SlugRole, "slug"},
		{TitleRole, "title"},
		{DescriptionRole, "description"},
		{AuthorRole, "author"},
		{LogoUrlRole, "logoUrl"},
		{DownloadsRole, "downloads"},
		{FollowsRole, "follows"},
		{UpdatedRole, "updated"},
		{LatestVersionRole, "latestVersion"},
		{GameVersionsRole, "gameVersions"},
		{CategoriesRole, "categories"},
		{GalleryUrlRole, "galleryUrl"},
		{AccentColorRole, "accentColor"},
	};
}

void ModrinthModpackModel::setQuery(const QString& query)
{
	if (m_query == query) {
		return;
	}
	m_query = query;
	emit queryChanged();
}

void ModrinthModpackModel::setSort(const QString& sort)
{
	if (m_sort == sort) {
		return;
	}
	m_sort = sort;
	emit sortChanged();
}

void ModrinthModpackModel::setGameVersion(const QString& gameVersion)
{
	if (m_gameVersion == gameVersion) {
		return;
	}
	m_gameVersion = gameVersion;
	emit gameVersionChanged();
}

void ModrinthModpackModel::setLoader(const QString& loader)
{
	if (m_loader == loader) {
		return;
	}
	m_loader = loader;
	emit loaderChanged();
}

void ModrinthModpackModel::setSearching(bool searching)
{
	if (m_searching == searching) {
		return;
	}
	m_searching = searching;
	emit searchingChanged();
}

void ModrinthModpackModel::setCanFetchMore(bool canFetchMore)
{
	if (m_canFetchMore == canFetchMore) {
		return;
	}
	m_canFetchMore = canFetchMore;
	emit canFetchMoreChanged();
}

void ModrinthModpackModel::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}

int ModrinthModpackModel::sortIndexOf(const QString& id) const
{
	for (int i = 0; i < m_sortOptions.size(); ++i) {
		if (m_sortOptions.at(i).toMap().value(QStringLiteral("id")).toString() ==
			id) {
			return i;
		}
	}
	return 0;
}

void ModrinthModpackModel::search()
{
	if (m_searchJob) {
		/* A reply for the previous query may still be on its way.
		 * Marked before abort() is called: aborting can complete
		 * synchronously, and the failure handler needs to already see
		 * the new intent when that happens - same pattern as
		 * ContentProviderModel::search() / Modrinth::ListModel::
		 * searchWithTerm(). */
		m_restartPending = true;
		m_searchJob->abort();
		return;
	}
	restartSearch();
}

void ModrinthModpackModel::restartSearch()
{
	beginResetModel();
	m_packs.clear();
	endResetModel();
	emit countChanged();

	m_nextOffset = 0;
	setCanFetchMore(false);
	setError(QString());
	performSearch();
}

void ModrinthModpackModel::fetchMore()
{
	if (m_searching || !m_canFetchMore) {
		return;
	}
	performSearch();
}

void ModrinthModpackModel::performSearch()
{
	m_searchResponse.clear();

	auto* job =
		new NetJob(QStringLiteral("Modrinth::ModpackSearch"), LAUNCHER->network());
	job->addNetAction(Net::Download::makeByteArray(
		ModrinthApi::modpackSearchUrl(m_query, sortIndexOf(m_sort),
									  m_nextOffset, m_gameVersion,
									  ModPlatform::singleLoaderList(m_loader)),
		&m_searchResponse));

	m_searchJob = job;
	connect(job, &NetJob::succeeded, this,
			&ModrinthModpackModel::onSearchSucceeded);
	connect(job, &NetJob::failed, this,
			&ModrinthModpackModel::onSearchFailed);

	job->start();
	setSearching(true);
}

void ModrinthModpackModel::onSearchSucceeded()
{
	m_searchJob.reset();

	if (m_restartPending) {
		m_restartPending = false;
		restartSearch();
		return;
	}

	int totalHits = -1;
	const QList<Modrinth::IndexedPack> newPacks =
		parseSearchResults(m_searchResponse, totalHits);

	if (!newPacks.isEmpty()) {
		const int first = m_packs.size();
		beginInsertRows(QModelIndex(), first, first + newPacks.size() - 1);
		m_packs.append(newPacks);
		endInsertRows();
		emit countChanged();
	}

	const int pageSize = ModrinthApi::get().searchPageSize();
	const bool lastPage =
		newPacks.size() < pageSize ||
		(totalHits >= 0 && (m_nextOffset + newPacks.size()) >= totalHits);
	if (lastPage) {
		setCanFetchMore(false);
	} else {
		m_nextOffset += pageSize;
		setCanFetchMore(true);
	}
	setSearching(false);
}

void ModrinthModpackModel::onSearchFailed(QString reason)
{
	m_searchJob.reset();

	if (m_restartPending) {
		m_restartPending = false;
		restartSearch();
		return;
	}

	setCanFetchMore(false);
	setSearching(false);
	setError(reason);
}

QList<Modrinth::IndexedPack>
ModrinthModpackModel::parseSearchResults(const QByteArray& bytes,
										 int& totalHits)
{
	QList<Modrinth::IndexedPack> results;
	totalHits = -1;

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Error while parsing JSON response from Modrinth at"
				   << parseError.offset
				   << "reason:" << parseError.errorString();
		return results;
	}

	const QJsonObject obj = doc.object();
	const QJsonArray hits = Json::ensureArray(obj, "hits");
	for (const QJsonValue& hitRaw : hits) {
		QJsonObject hitObj = hitRaw.toObject();
		Modrinth::IndexedPack pack;
		try {
			Modrinth::loadIndexedPack(pack, hitObj);
			results.append(pack);
		} catch (const JSONValidationError& e) {
			qWarning() << "Error while loading modpack from Modrinth:"
					   << e.cause();
		}
	}

	totalHits = Json::ensureInteger(obj, "total_hits", 0);
	return results;
}

void ModrinthModpackModel::loadDetail(const QString& projectId)
{
	if (projectId.isEmpty()) {
		return;
	}

	if (m_bodyJob) {
		m_bodyJob->abort();
		m_bodyJob.reset();
	}
	if (m_versionsJob) {
		m_versionsJob->abort();
		m_versionsJob.reset();
	}

	++m_detailGeneration;
	const quint64 generation = m_detailGeneration;
	m_bodyDone = false;
	m_versionsDone = false;

	m_detail->reset(projectId);
	for (const auto& pack : m_packs) {
		if (pack.projectId == projectId) {
			m_detail->setTitle(pack.name);
			break;
		}
	}

	fetchDetailBody(projectId, generation);
	fetchDetailVersions(projectId, generation);
}

void ModrinthModpackModel::markDetailPartDone(quint64 generation, bool isBody)
{
	if (generation != m_detailGeneration) {
		/* A newer loadDetail() call has already moved on. */
		return;
	}
	if (isBody) {
		m_bodyDone = true;
	} else {
		m_versionsDone = true;
	}
	if (m_bodyDone && m_versionsDone) {
		m_detail->setLoading(false);
	}
}

void ModrinthModpackModel::fetchDetailBody(const QString& projectId,
										   quint64 generation)
{
	auto response = std::make_shared<QByteArray>();
	auto* job = new NetJob(QStringLiteral("Modrinth::Project(%1)").arg(projectId),
						  LAUNCHER->network());
	job->addNetAction(Net::Download::makeByteArray(
		ModrinthApi::get().projectBodyUrl(projectId), response.get()));

	m_bodyJob = job;
	connect(job, &NetJob::succeeded, this,
			[this, job, response, generation] {
				job->deleteLater();
				if (generation == m_detailGeneration) {
					m_bodyJob.reset();
					QJsonParseError parseError;
					const QJsonDocument doc =
						QJsonDocument::fromJson(*response, &parseError);
					if (parseError.error == QJsonParseError::NoError) {
						const QJsonObject obj = doc.object();
						m_detail->setBody(
							Json::ensureString(obj, "body", QString()));
						const QString title =
							Json::ensureString(obj, "title", QString());
						if (!title.isEmpty()) {
							m_detail->setTitle(title);
						}

						/* The full project object carries a richer
						 * "gallery" than a search hit does: objects
						 * with their own title/featured flag rather
						 * than bare URLs (see loadIndexedPack() for
						 * the search-hit shape). Only entries with a
						 * URL are kept; featured images are sorted
						 * first so the detail header always prefers
						 * one, same as Modrinth's own project page. */
						QVariantList gallery;
						for (const auto& imageRaw :
							 Json::ensureArray(obj, "gallery")) {
							const QJsonObject imageObj = imageRaw.toObject();
							const QString url =
								Json::ensureString(imageObj, "url", QString());
							if (url.isEmpty()) {
								continue;
							}
							QVariantMap entry;
							entry[QStringLiteral("url")] = url;
							entry[QStringLiteral("featured")] =
								Json::ensureBoolean(imageObj, "featured", false);
							entry[QStringLiteral("title")] =
								Json::ensureString(imageObj, "title", QString());
							gallery.append(entry);
						}
						std::stable_sort(
							gallery.begin(), gallery.end(),
							[](const QVariant& a, const QVariant& b) {
								return a.toMap().value(QStringLiteral("featured")).toBool() &&
									   !b.toMap().value(QStringLiteral("featured")).toBool();
							});
						m_detail->setGallery(gallery);
					}
				}
				markDetailPartDone(generation, true);
			});
	connect(job, &NetJob::failed, this,
			[this, job, generation](QString reason) {
				job->deleteLater();
				if (generation == m_detailGeneration) {
					m_bodyJob.reset();
					m_detail->setError(reason);
				}
				markDetailPartDone(generation, true);
			});
	job->start();
}

void ModrinthModpackModel::fetchDetailVersions(const QString& projectId,
											   quint64 generation)
{
	auto response = std::make_shared<QByteArray>();
	auto* job = new NetJob(
		QStringLiteral("Modrinth::PackVersions(%1)").arg(projectId),
		LAUNCHER->network());
	/* A modpack ships its own loader, so accept any of them here rather
	 * than filtering to whatever instance the user might currently have
	 * selected - same reasoning, and the same call, as
	 * ModrinthPage::onSelectionChanged(). */
	job->addNetAction(Net::Download::makeByteArray(
		ModrinthApi::projectVersionsUrlForLoaders(
			projectId, {QStringLiteral("forge"), QStringLiteral("fabric"),
						QStringLiteral("quilt"), QStringLiteral("neoforge")}),
		response.get()));

	m_versionsJob = job;
	connect(job, &NetJob::succeeded, this,
			[this, job, response, projectId, generation] {
				job->deleteLater();
				if (generation == m_detailGeneration) {
					m_versionsJob.reset();
					m_detail->setVersions(
						parseVersionsJson(*response, projectId));
				}
				markDetailPartDone(generation, false);
			});
	connect(job, &NetJob::failed, this,
			[this, job, generation](QString reason) {
				job->deleteLater();
				if (generation == m_detailGeneration) {
					m_versionsJob.reset();
					m_detail->setError(reason);
				}
				markDetailPartDone(generation, false);
			});
	job->start();
}

QVariantList
ModrinthModpackModel::parseVersionsJson(const QByteArray& bytes,
										const QString& projectId)
{
	QVariantList result;

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Error while parsing JSON response from Modrinth at"
				   << parseError.offset
				   << "reason:" << parseError.errorString();
		return result;
	}

	QJsonArray arr = doc.array();
	Modrinth::IndexedPack tempPack;
	tempPack.projectId = projectId;
	try {
		Modrinth::loadIndexedPackVersions(tempPack, arr);
	} catch (const JSONValidationError& e) {
		qWarning() << "Error while reading Modrinth modpack versions:"
				   << e.cause();
		return result;
	}

	for (const auto& version : tempPack.versions) {
		QVariantMap entry;
		entry[QStringLiteral("id")] = version.id;
		entry[QStringLiteral("name")] = version.name;
		entry[QStringLiteral("versionNumber")] = version.versionNumber;
		entry[QStringLiteral("gameVersions")] =
			QVariant::fromValue(version.gameVersions);
		entry[QStringLiteral("loaders")] =
			QVariant::fromValue(version.loaderList);
		entry[QStringLiteral("datePublished")] = version.datePublished;
		entry[QStringLiteral("downloadUrl")] = version.downloadUrl;
		entry[QStringLiteral("featured")] = version.featured;
		result.append(entry);
	}
	return result;
}

QString ModrinthModpackModel::resolveIconKey(const QString& slug,
											 const QString& iconUrl) const
{
	if (!slug.isEmpty() && !iconUrl.isEmpty()) {
		/* Same bucket the widget Modrinth page
		 * (ui/pages/modplatform/modrinth/ModrinthModel.cpp) has always
		 * cached these icons under, so a pack browsed there before
		 * already has a hit waiting here. resolveEntry() is a disk-only
		 * lookup - it never starts a download - and only comes back
		 * non-stale once it has verified the file is actually there. */
		MetaEntryPtr entry = LAUNCHER->metacache()->resolveEntry(
			QStringLiteral("ModrinthPacks"),
			QStringLiteral("logos/%1").arg(slug));
		if (entry && !entry->isStale()) {
			const QString key = QStringLiteral("modrinth_") + slug;
			LAUNCHER->icons()->installIcon(entry->getFullPath(), key);
			return key;
		}
	}
	return QStringLiteral("modrinth");
}

QObject* ModrinthModpackModel::install(const QString& projectId,
									   const QString& versionId,
									   const QString& instanceName,
									   const QString& group)
{
	QString packTitle;
	QString packSlug;
	QString iconUrl;
	for (const auto& pack : m_packs) {
		if (pack.projectId == projectId) {
			packTitle = pack.name;
			packSlug = pack.slug;
			iconUrl = pack.iconUrl;
			break;
		}
	}

	QString downloadUrl;
	QString versionLabel;
	if (m_detail->projectId() == projectId) {
		if (packTitle.isEmpty()) {
			packTitle = m_detail->title();
		}
		for (const QVariant& versionVariant : m_detail->versions()) {
			const QVariantMap versionMap = versionVariant.toMap();
			if (versionMap.value(QStringLiteral("id")).toString() ==
				versionId) {
				downloadUrl =
					versionMap.value(QStringLiteral("downloadUrl")).toString();
				versionLabel =
					versionMap.value(QStringLiteral("versionNumber"))
						.toString();
				break;
			}
		}
	}

	if (downloadUrl.isEmpty()) {
		qWarning() << "ModrinthModpackModel::install: no download URL for "
					  "version"
				   << versionId << "of project" << projectId
				   << "- was loadDetail() called and finished first?";
		return nullptr;
	}

	/* Builds exactly what ModrinthPage::suggestCurrent() builds today
	 * (ui/pages/modplatform/modrinth/ModrinthPage.cpp), and what
	 * NewInstanceDialog::extractTask() +
	 * MainWindow::createInstanceFromDialog() then do to it - the four
	 * setters below and wrapInstanceTask() are that same funnel. The
	 * only real difference is where the name/group/icon/target
	 * directory come from: the dialog reads its own widgets, this reads
	 * the caller's arguments (and the primary instance folder, since a
	 * headless install has no folder picker). */
	auto* importTask = new InstanceImportTask(QUrl(downloadUrl));
	importTask->setTrustedSource(true);

	InstanceImportTask::PackSourceHint hint;
	hint.provider = QStringLiteral("modrinth");
	hint.packId = projectId;
	hint.packSlug = packSlug;
	hint.versionId = versionId;
	hint.versionLabel = versionLabel;
	hint.iconUrl = iconUrl;
	if (!packSlug.isEmpty()) {
		hint.sourceUrl =
			QStringLiteral("https://modrinth.com/modpack/%1").arg(packSlug);
	}
	importTask->setPackSourceHint(hint);

	const QString effectiveName =
		instanceName.isEmpty() ? packTitle : instanceName;
	importTask->setName(effectiveName);
	importTask->setGroup(group);
	importTask->setIcon(resolveIconKey(packSlug, iconUrl));
	importTask->setTargetDir(LAUNCHER->instances()->primaryDir());

	Task* wrapped = LAUNCHER->instances()->wrapInstanceTask(importTask);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), this);
	watcher->setTitle(effectiveName);
	wrapped->start();
	return watcher;
}
