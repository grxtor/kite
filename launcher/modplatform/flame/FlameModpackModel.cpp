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

#include "FlameModpackModel.h"

#include <algorithm>

#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrlQuery>

#include "BuildConfig.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "Json.h"
#include "core/LauncherContext.h"
#include "icons/IconList.h"
#include "modplatform/ContentApi.h"
#include "modplatform/ContentType.h"
#include "modplatform/VersionPicker.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/modrinth/ModrinthModpackModel.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/* CurseForge wraps its replies in {"data": ...}; the widget code
	 * accepts a bare value too, for the older API shape. */
	QJsonValue dataOf(const QJsonDocument& doc)
	{
		if (doc.isObject() && doc.object().contains(QStringLiteral("data"))) {
			return doc.object().value(QStringLiteral("data"));
		}
		return doc.isArray() ? QJsonValue(doc.array()) : QJsonValue();
	}
} // namespace

FlameModpackModel::FlameModpackModel(QObject* parent)
	: QAbstractListModel(parent), m_detail(new ModrinthModpackDetail(this))
{
	for (const auto& sorting : FlameApi::get().sortingMethods()) {
		QVariantMap entry;
		entry[QStringLiteral("id")] = sorting.apiValue;
		entry[QStringLiteral("label")] = sorting.readableName;
		m_sortOptions.append(entry);
	}
	/* Popularity rather than the first entry ("Featured"), which is
	 * CurseForge's own curated order and rarely what someone browsing
	 * wants. */
	m_sort = QStringLiteral("2");
}

FlameModpackModel::~FlameModpackModel()
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

QObject* FlameModpackModel::detail() const
{
	return m_detail;
}

QString FlameModpackModel::providerName() const
{
	return FlameApi::get().displayName();
}

bool FlameModpackModel::keyMissing() const
{
	return BuildConfig.CURSEFORGE_API_KEY.isEmpty();
}

int FlameModpackModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : int(m_packs.size());
}

QVariant FlameModpackModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_packs.size()) {
		return QVariant();
	}
	const Pack& pack = m_packs.at(index.row());
	switch (role) {
		case ProjectIdRole:
			return QString::number(pack.base.addonId);
		case SlugRole:
			return pack.slug;
		case TitleRole:
			return pack.base.name;
		case DescriptionRole:
			return pack.base.description;
		case AuthorRole:
			return pack.base.authors.isEmpty() ? QString()
											   : pack.base.authors.first().name;
		case LogoUrlRole:
			return pack.base.logoUrl;
		case DownloadsRole:
			return pack.downloads;
		case FollowsRole:
			return pack.thumbsUp;
		case UpdatedRole:
			return pack.dateModified;
		case LatestVersionRole:
			return pack.mainFileId > 0 ? QString::number(pack.mainFileId)
									   : QString();
		case GameVersionsRole:
			return QVariant::fromValue(pack.gameVersions);
		case CategoriesRole:
			return QVariant::fromValue(pack.categories);
		case GalleryUrlRole:
			return pack.screenshots.isEmpty() ? QString()
											  : pack.screenshots.first();
		case AccentColorRole:
			/* CurseForge has no brand colour per project. */
			return QVariant();
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> FlameModpackModel::roleNames() const
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

void FlameModpackModel::setQuery(const QString& query)
{
	if (m_query == query) {
		return;
	}
	m_query = query;
	emit queryChanged();
}

void FlameModpackModel::setSort(const QString& sort)
{
	if (m_sort == sort) {
		return;
	}
	m_sort = sort;
	emit sortChanged();
}

void FlameModpackModel::setGameVersion(const QString& gameVersion)
{
	if (m_gameVersion == gameVersion) {
		return;
	}
	m_gameVersion = gameVersion;
	emit gameVersionChanged();
}

void FlameModpackModel::setLoader(const QString& loader)
{
	if (m_loader == loader) {
		return;
	}
	m_loader = loader;
	emit loaderChanged();
}

void FlameModpackModel::setSearching(bool searching)
{
	if (m_searching == searching) {
		return;
	}
	m_searching = searching;
	emit searchingChanged();
}

void FlameModpackModel::setCanFetchMore(bool canFetchMore)
{
	if (m_canFetchMore == canFetchMore) {
		return;
	}
	m_canFetchMore = canFetchMore;
	emit canFetchMoreChanged();
}

void FlameModpackModel::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}

int FlameModpackModel::sortIndexOf(const QString& id) const
{
	for (int i = 0; i < m_sortOptions.size(); ++i) {
		if (m_sortOptions.at(i).toMap().value(QStringLiteral("id")).toString() ==
			id) {
			return i;
		}
	}
	return 0;
}

QUrl FlameModpackModel::searchUrl(const QString& term, int sortIndex,
								  int offset, const QString& gameVersion,
								  const QString& loader)
{
	QUrl url = FlameApi::modpackSearchUrl(term, sortIndex, offset);
	QUrlQuery query(url);
	if (!gameVersion.isEmpty()) {
		query.addQueryItem(QStringLiteral("gameVersion"), gameVersion);
	}
	const int loaderType = ModPlatform::loaderToCurseForgeModLoaderType(loader);
	if (loaderType > 0) {
		query.addQueryItem(QStringLiteral("modLoaderType"),
						   QString::number(loaderType));
	}
	url.setQuery(query);
	return url;
}

void FlameModpackModel::search()
{
	if (keyMissing()) {
		beginResetModel();
		m_packs.clear();
		endResetModel();
		emit countChanged();
		setCanFetchMore(false);
		setError(tr("This build has no CurseForge API key, so CurseForge "
					"cannot be searched."));
		return;
	}
	if (m_searchJob) {
		/* Marked before abort(), which can finish synchronously - same
		 * pattern as ModrinthModpackModel::search(). */
		m_restartPending = true;
		m_searchJob->abort();
		return;
	}
	restartSearch();
}

void FlameModpackModel::restartSearch()
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

void FlameModpackModel::fetchMore()
{
	if (m_searching || !m_canFetchMore) {
		return;
	}
	performSearch();
}

void FlameModpackModel::performSearch()
{
	m_searchResponse.clear();
	auto* job = new NetJob(QStringLiteral("Flame::ModpackSearch"),
						   LAUNCHER->network());
	job->addNetAction(Net::Download::makeByteArray(
		searchUrl(m_query, sortIndexOf(m_sort), m_nextOffset, m_gameVersion,
				  m_loader),
		&m_searchResponse));
	m_searchJob = job;
	connect(job, &NetJob::succeeded, this, &FlameModpackModel::onSearchSucceeded);
	connect(job, &NetJob::failed, this, &FlameModpackModel::onSearchFailed);
	job->start();
	setSearching(true);
}

void FlameModpackModel::onSearchSucceeded()
{
	m_searchJob.reset();
	if (m_restartPending) {
		m_restartPending = false;
		restartSearch();
		return;
	}

	int totalCount = -1;
	const QList<Pack> newPacks = parseSearchResults(m_searchResponse, totalCount);
	if (!newPacks.isEmpty()) {
		const int first = int(m_packs.size());
		beginInsertRows(QModelIndex(), first, first + int(newPacks.size()) - 1);
		m_packs.append(newPacks);
		endInsertRows();
		emit countChanged();
	}

	/* A page can come back short of packs that failed validation, so the
	 * total decides when it says one; the page size only when it does
	 * not. */
	const int pageSize = FlameApi::get().searchPageSize();
	m_nextOffset += pageSize;
	const bool lastPage = totalCount >= 0 ? m_nextOffset >= totalCount
										  : newPacks.size() < pageSize;
	setCanFetchMore(!lastPage);
	setSearching(false);
}

void FlameModpackModel::onSearchFailed(QString reason)
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

QList<FlameModpackModel::Pack>
FlameModpackModel::parseSearchResults(const QByteArray& bytes, int& totalCount)
{
	QList<Pack> results;
	totalCount = -1;

	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Error while parsing JSON response from CurseForge at"
				   << parseError.offset << "reason:" << parseError.errorString();
		return results;
	}

	for (const QJsonValue& raw : dataOf(doc).toArray()) {
		QJsonObject obj = raw.toObject();
		Pack pack;
		try {
			/* Also drops a pack with no usable file, as the widget list
			 * did. */
			Flame::loadIndexedPack(pack.base, obj);
		} catch (const JSONValidationError& e) {
			qWarning() << "Error while loading modpack from CurseForge:"
					   << e.cause();
			continue;
		}
		pack.slug = Json::ensureString(obj, "slug", QString());
		pack.downloads = qint64(obj.value(QStringLiteral("downloadCount")).toDouble());
		pack.thumbsUp = Json::ensureInteger(obj, "thumbsUpCount", 0);
		pack.dateModified = Json::ensureString(obj, "dateModified", QString());
		pack.mainFileId = obj.contains(QStringLiteral("mainFileId"))
							  ? Json::ensureInteger(obj, "mainFileId", 0)
							  : Json::ensureInteger(obj, "defaultFileId", 0);
		for (const auto& categoryRaw : Json::ensureArray(obj, "categories")) {
			const QString name =
				Json::ensureString(categoryRaw.toObject(), "name", QString());
			if (!name.isEmpty() && !pack.categories.contains(name)) {
				pack.categories.append(name);
			}
		}
		for (const auto& fileRaw : Json::ensureArray(obj, "latestFilesIndexes")) {
			const QString version =
				Json::ensureString(fileRaw.toObject(), "gameVersion", QString());
			if (!version.isEmpty() && !pack.gameVersions.contains(version)) {
				pack.gameVersions.append(version);
			}
		}
		for (const auto& shotRaw : Json::ensureArray(obj, "screenshots")) {
			const QString url =
				Json::ensureString(shotRaw.toObject(), "url", QString());
			if (!url.isEmpty()) {
				pack.screenshots.append(url);
			}
		}
		results.append(pack);
	}

	if (doc.isObject()) {
		const QJsonObject pagination =
			doc.object().value(QStringLiteral("pagination")).toObject();
		if (pagination.contains(QStringLiteral("totalCount"))) {
			totalCount = pagination.value(QStringLiteral("totalCount")).toInt(-1);
		}
	}
	return results;
}

QVariantList FlameModpackModel::parseVersionsJson(const QByteArray& bytes,
												  const QString& projectId,
												  int mainFileId)
{
	QVariantList result;
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Error while parsing JSON response from CurseForge at"
				   << parseError.offset << "reason:" << parseError.errorString();
		return result;
	}

	struct File {
		int id = 0;
		QDateTime date;
		QVariantMap entry;
	};
	QList<File> files;
	for (const QJsonValue& raw : dataOf(doc).toArray()) {
		const QJsonObject obj = raw.toObject();
		File file;
		file.id = Json::ensureInteger(obj, "id", 0);
		if (file.id <= 0) {
			continue;
		}
		/* CurseForge mixes loaders into the game version list. */
		QStringList gameVersions;
		QStringList loaders;
		for (const auto& tagRaw : Json::ensureArray(obj, "gameVersions")) {
			const QString tag = tagRaw.toString();
			if (ModPlatform::isKnownLoaderName(tag)) {
				loaders.append(tag.toLower());
			} else if (!tag.isEmpty() && tag.at(0).isDigit()) {
				gameVersions.append(tag);
			}
		}
		const QString name = Json::ensureString(obj, "displayName", QString());
		QString downloadUrl = Json::ensureString(obj, "downloadUrl", QString());
		if (downloadUrl.isEmpty()) {
			/* The author keeps it off third-party downloads; the browser
			 * address is the sanctioned way to get it (FlamePage). */
			downloadUrl =
				FlameApi::browserDownloadUrl(projectId, QString::number(file.id));
		}
		const QString date = Json::ensureString(obj, "fileDate", QString());
		file.date = QDateTime::fromString(date, Qt::ISODate);

		file.entry[QStringLiteral("id")] = QString::number(file.id);
		file.entry[QStringLiteral("name")] = name;
		file.entry[QStringLiteral("versionNumber")] = name;
		file.entry[QStringLiteral("gameVersions")] = QVariant::fromValue(gameVersions);
		file.entry[QStringLiteral("loaders")] = QVariant::fromValue(loaders);
		file.entry[QStringLiteral("datePublished")] = date;
		file.entry[QStringLiteral("downloadUrl")] = downloadUrl;
		file.entry[QStringLiteral("fileName")] =
			Json::ensureString(obj, "fileName", QString());
		file.entry[QStringLiteral("featured")] = file.id == mainFileId;
		files.append(file);
	}

	/* Newest first: by date when both have one, by file id (which only
	 * ever grows) otherwise - Flame::loadIndexedPackVersions()'s order. */
	std::stable_sort(files.begin(), files.end(), [](const File& a, const File& b) {
		if (a.date.isValid() && b.date.isValid() && a.date != b.date) {
			return a.date > b.date;
		}
		return a.id > b.id;
	});
	for (const File& file : files) {
		result.append(file.entry);
	}
	return result;
}

QString FlameModpackModel::parseBody(const QByteArray& bytes)
{
	const QJsonDocument doc = QJsonDocument::fromJson(bytes);
	return sanitizeBody(dataOf(doc).toString());
}

QString FlameModpackModel::sanitizeBody(const QString& html)
{
	const auto options = QRegularExpression::CaseInsensitiveOption |
						 QRegularExpression::DotMatchesEverythingOption;
	QString out = html;
	/* Elements whose content goes with them. */
	for (const char* tag : {"script", "style", "iframe", "video", "audio", "object"}) {
		const QString name = QString::fromLatin1(tag);
		out.remove(QRegularExpression(
			QStringLiteral("<%1\\b[^>]*>.*?</%1\\s*>").arg(name), options));
		out.remove(QRegularExpression(
			QStringLiteral("<%1\\b[^>]*/?>").arg(name), options));
	}
	/* Empty elements. Not img: RichBody lays pictures out as blocks of
	 * their own and loads only http(s) ones. */
	for (const char* tag : {"embed", "source"}) {
		out.remove(QRegularExpression(
			QStringLiteral("<%1\\b[^>]*>").arg(QString::fromLatin1(tag)), options));
	}
	/* Links that only held an embed. */
	out.remove(QRegularExpression(QStringLiteral("<a\\b[^>]*>\\s*</a\\s*>"), options));
	return out.trimmed();
}

const FlameModpackModel::Pack*
FlameModpackModel::packFor(const QString& projectId) const
{
	for (const Pack& pack : m_packs) {
		if (QString::number(pack.base.addonId) == projectId) {
			return &pack;
		}
	}
	return nullptr;
}

void FlameModpackModel::loadDetail(const QString& projectId)
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
	int mainFileId = 0;
	if (const Pack* pack = packFor(projectId)) {
		m_detail->setTitle(pack->base.name);
		mainFileId = pack->mainFileId;
		/* The search hit already carries the screenshots. */
		QVariantList gallery;
		for (const QString& url : pack->screenshots) {
			QVariantMap entry;
			entry[QStringLiteral("url")] = url;
			entry[QStringLiteral("featured")] = gallery.isEmpty();
			entry[QStringLiteral("title")] = QString();
			gallery.append(entry);
		}
		m_detail->setGallery(gallery);
	}

	if (keyMissing()) {
		m_detail->setError(tr("This build has no CurseForge API key."));
		m_detail->setLoading(false);
		return;
	}
	fetchDetailBody(projectId, generation);
	fetchDetailVersions(projectId, mainFileId, generation);
}

void FlameModpackModel::markDetailPartDone(quint64 generation, bool isBody)
{
	if (generation != m_detailGeneration) {
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

void FlameModpackModel::fetchDetailBody(const QString& projectId,
										quint64 generation)
{
	auto response = std::make_shared<QByteArray>();
	auto* job = new NetJob(QStringLiteral("Flame::Description(%1)").arg(projectId),
						   LAUNCHER->network());
	job->addNetAction(Net::Download::makeByteArray(
		FlameApi::get().projectBodyUrl(projectId), response.get()));
	m_bodyJob = job;
	connect(job, &NetJob::succeeded, this, [this, job, response, generation] {
		job->deleteLater();
		if (generation == m_detailGeneration) {
			m_bodyJob.reset();
			m_detail->setBody(parseBody(*response), RichBody::Format::Html);
		}
		markDetailPartDone(generation, true);
	});
	connect(job, &NetJob::failed, this, [this, job, generation](QString reason) {
		job->deleteLater();
		if (generation == m_detailGeneration) {
			m_bodyJob.reset();
			m_detail->setError(reason);
		}
		markDetailPartDone(generation, true);
	});
	job->start();
}

void FlameModpackModel::fetchDetailVersions(const QString& projectId,
											int mainFileId, quint64 generation)
{
	auto response = std::make_shared<QByteArray>();
	auto* job = new NetJob(QStringLiteral("Flame::PackVersions(%1)").arg(projectId),
						   LAUNCHER->network());
	/* Every file in one request - the default page would show some
	 * arbitrary window of a long-lived pack's history. No loader filter:
	 * a modpack ships its own. */
	ModPlatform::VersionQuery query;
	query.projectId = projectId;
	job->addNetAction(Net::Download::makeByteArray(
		FlameApi::get().projectVersionsUrl(query), response.get()));
	m_versionsJob = job;
	connect(job, &NetJob::succeeded, this,
			[this, job, response, projectId, mainFileId, generation] {
				job->deleteLater();
				if (generation == m_detailGeneration) {
					m_versionsJob.reset();
					m_detail->setVersions(
						parseVersionsJson(*response, projectId, mainFileId));
				}
				markDetailPartDone(generation, false);
			});
	connect(job, &NetJob::failed, this, [this, job, generation](QString reason) {
		job->deleteLater();
		if (generation == m_detailGeneration) {
			m_versionsJob.reset();
			m_detail->setError(reason);
		}
		markDetailPartDone(generation, false);
	});
	job->start();
}

QString FlameModpackModel::resolveIconKey(const Flame::IndexedPack& pack) const
{
	/* The bucket and key the widget CurseForge page (FlameModel.cpp,
	 * FlamePage::suggestCurrent()) has always used, so a logo fetched
	 * there already helps here. A disk-only lookup; never downloads. */
	const QString logo = pack.logoName.section(QLatin1Char('.'), 0, 0);
	if (!logo.isEmpty() && !pack.logoUrl.isEmpty()) {
		MetaEntryPtr entry = LAUNCHER->metacache()->resolveEntry(
			QStringLiteral("FlamePacks"), QStringLiteral("logos/%1").arg(logo));
		if (entry && !entry->isStale()) {
			const QString key = QStringLiteral("curseforge_") + logo;
			LAUNCHER->icons()->installIcon(entry->getFullPath(), key);
			return key;
		}
	}
	return QStringLiteral("flame");
}

QObject* FlameModpackModel::install(const QString& projectId,
									const QString& versionId,
									const QString& instanceName,
									const QString& group)
{
	Flame::IndexedPack base;
	if (const Pack* pack = packFor(projectId)) {
		base = pack->base;
	}

	QString downloadUrl;
	QString versionLabel;
	if (m_detail->projectId() == projectId) {
		if (base.name.isEmpty()) {
			base.name = m_detail->title();
		}
		for (const QVariant& raw : m_detail->versions()) {
			const QVariantMap version = raw.toMap();
			if (version.value(QStringLiteral("id")).toString() == versionId) {
				downloadUrl = version.value(QStringLiteral("downloadUrl")).toString();
				versionLabel = version.value(QStringLiteral("name")).toString();
				break;
			}
		}
	}
	if (downloadUrl.isEmpty()) {
		qWarning() << "FlameModpackModel::install: no download for file"
				   << versionId << "of project" << projectId
				   << "- was loadDetail() called and finished first?";
		return nullptr;
	}

	/* What FlamePage::suggestCurrent() builds for the widget dialog, and
	 * NewInstanceDialog/MainWindow then name, place and start. The
	 * import reads the pack's manifest, resolves its files (asking about
	 * any the authors keep off third-party downloads through the UiHost)
	 * and installs its loader. */
	auto* importTask = new InstanceImportTask(QUrl(downloadUrl));
	importTask->setTrustedSource(true);

	InstanceImportTask::PackSourceHint hint;
	hint.provider = QStringLiteral("curseforge");
	hint.packId = projectId;
	hint.packName = base.name;
	hint.versionId = versionId;
	hint.versionLabel = versionLabel;
	hint.iconUrl = base.logoUrl;
	hint.sourceUrl = base.websiteUrl;
	importTask->setPackSourceHint(hint);

	const QString effectiveName = instanceName.isEmpty() ? base.name : instanceName;
	importTask->setName(effectiveName);
	importTask->setGroup(group);
	importTask->setIcon(resolveIconKey(base));
	importTask->setTargetDir(LAUNCHER->instances()->primaryDir());

	Task* wrapped = LAUNCHER->instances()->wrapInstanceTask(importTask);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), this);
	watcher->setTitle(effectiveName);
	wrapped->start();
	return watcher;
}
