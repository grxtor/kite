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

#include "TechnicModpackModel.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrlQuery>

#include "BuildConfig.h"
#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "icons/IconList.h"
#include "modplatform/modrinth/ModrinthModpackModel.h"
#include "modplatform/technic/SingleZipPackInstallTask.h"
#include "modplatform/technic/SolderPackInstallTask.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/* Technic answers a missing field with JSON null as often as by
	 * leaving it out; either way it is "nothing". */
	QString stringOf(const QJsonObject& obj, const QString& key)
	{
		const QJsonValue value = obj.value(key);
		return value.isString() ? value.toString() : QString();
	}

	/* The URL of an image object ({"url": ...}) or a bare string. */
	QString imageOf(const QJsonObject& obj, const QString& key)
	{
		const QJsonValue value = obj.value(key);
		if (value.isObject()) {
			return value.toObject().value(QStringLiteral("url")).toString();
		}
		return value.isString() ? value.toString() : QString();
	}
} // namespace

TechnicModpackModel::TechnicModpackModel(QObject* parent)
	: QAbstractListModel(parent), m_detail(new ModrinthModpackDetail(this))
{
}

TechnicModpackModel::~TechnicModpackModel()
{
	if (m_searchJob) {
		m_searchJob->abort();
	}
	if (m_detailJob) {
		m_detailJob->abort();
	}
}

QObject* TechnicModpackModel::detail() const
{
	return m_detail;
}

QVariantList TechnicModpackModel::sortOptions() const
{
	/* Technic orders its own answers; there is nothing to choose. */
	QVariantMap entry;
	entry[QStringLiteral("id")] = QStringLiteral("relevance");
	entry[QStringLiteral("label")] = tr("Sort by Relevance");
	return {entry};
}

int TechnicModpackModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : int(m_packs.size());
}

QVariant TechnicModpackModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_packs.size()) {
		return QVariant();
	}
	const Pack& pack = m_packs.at(index.row());
	switch (role) {
		case ProjectIdRole:
		case SlugRole:
			return pack.slug;
		case TitleRole:
			return pack.name;
		case DescriptionRole:
			/* Search hits carry no summary; the detail fetch has one. */
			return pack.description;
		case AuthorRole:
			return pack.author;
		case LogoUrlRole:
			return pack.logoUrl;
		case DownloadsRole:
			return pack.downloads;
		case FollowsRole:
			return 0;
		case UpdatedRole:
			return QString();
		case LatestVersionRole:
			return pack.version;
		case GameVersionsRole:
			return QVariant::fromValue(
				pack.minecraftVersion.isEmpty() ? QStringList()
												: QStringList{pack.minecraftVersion});
		case CategoriesRole:
			return QVariant::fromValue(QStringList());
		case GalleryUrlRole:
			return pack.backgroundUrl;
		case AccentColorRole:
			return QVariant();
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> TechnicModpackModel::roleNames() const
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

void TechnicModpackModel::setQuery(const QString& query)
{
	if (m_query == query) {
		return;
	}
	m_query = query;
	emit queryChanged();
}

void TechnicModpackModel::setGameVersion(const QString& gameVersion)
{
	if (m_gameVersion == gameVersion) {
		return;
	}
	m_gameVersion = gameVersion;
	emit gameVersionChanged();
}

void TechnicModpackModel::setLoader(const QString& loader)
{
	if (m_loader == loader) {
		return;
	}
	m_loader = loader;
	emit loaderChanged();
}

void TechnicModpackModel::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}

void TechnicModpackModel::setSearching(bool searching)
{
	if (m_searching == searching) {
		return;
	}
	m_searching = searching;
	emit searchingChanged();
}

QUrl TechnicModpackModel::searchUrl(const QString& term)
{
	const QString trimmed = term.trimmed();
	QUrl url(BuildConfig.TECHNIC_API_BASE_URL +
			 (trimmed.isEmpty() ? QStringLiteral("trending")
								: QStringLiteral("search")));
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("build"), BuildConfig.TECHNIC_API_BUILD);
	if (!trimmed.isEmpty()) {
		/* Through QUrlQuery, unlike the widget page's string pasting, so
		 * an "&" in a search stays part of it. */
		query.addQueryItem(QStringLiteral("q"), trimmed);
	}
	url.setQuery(query);
	return url;
}

QUrl TechnicModpackModel::metadataUrl(const QString& slug)
{
	QUrl url(BuildConfig.TECHNIC_API_BASE_URL + QStringLiteral("modpack/") + slug);
	QUrlQuery query;
	query.addQueryItem(QStringLiteral("build"), BuildConfig.TECHNIC_API_BUILD);
	url.setQuery(query);
	return url;
}

QList<TechnicModpackModel::Pack>
TechnicModpackModel::parseSearch(const QByteArray& bytes)
{
	QList<Pack> packs;
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Couldn't parse Technic search results:"
				   << parseError.errorString();
		return packs;
	}
	for (const auto& raw : doc.object().value(QStringLiteral("modpacks")).toArray()) {
		const QJsonObject obj = raw.toObject();
		Pack pack;
		pack.name = stringOf(obj, QStringLiteral("name"));
		pack.slug = stringOf(obj, QStringLiteral("slug"));
		/* The bare game, which the launcher installs on its own. */
		if (pack.slug.isEmpty() || pack.name.isEmpty() ||
			pack.slug == QStringLiteral("vanilla")) {
			continue;
		}
		pack.logoUrl = stringOf(obj, QStringLiteral("iconUrl"));
		if (!pack.logoUrl.isEmpty()) {
			pack.logoName = pack.logoUrl.section(QLatin1Char('/'), -1)
								.section(QLatin1Char('.'), 0, 0);
		}
		packs.append(pack);
	}
	return packs;
}

bool TechnicModpackModel::parseMetadata(const QByteArray& bytes, Pack& pack)
{
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		return false;
	}
	const QJsonObject obj = doc.object();

	/* A zip to download, or else a Solder server - TechnicPage's order. */
	const QString zip = stringOf(obj, QStringLiteral("url"));
	const QString solder = stringOf(obj, QStringLiteral("solder"));
	if (!zip.isEmpty()) {
		pack.url = zip;
		pack.isSolder = false;
	} else if (!solder.isEmpty()) {
		pack.url = solder;
		pack.isSolder = true;
	} else {
		return false;
	}

	pack.minecraftVersion = stringOf(obj, QStringLiteral("minecraft"));
	pack.version = stringOf(obj, QStringLiteral("version"));
	pack.websiteUrl = stringOf(obj, QStringLiteral("platformUrl"));
	pack.author = stringOf(obj, QStringLiteral("user"));
	pack.description = stringOf(obj, QStringLiteral("description"));
	pack.backgroundUrl = imageOf(obj, QStringLiteral("background"));
	if (pack.logoUrl.isEmpty()) {
		pack.logoUrl = imageOf(obj, QStringLiteral("icon"));
	}
	pack.downloads = qint64(obj.value(QStringLiteral("downloads")).toDouble());
	pack.runs = qint64(obj.value(QStringLiteral("runs")).toDouble());
	pack.metadataLoaded = true;
	return true;
}

QString TechnicModpackModel::solderPackUrl(const Pack& pack)
{
	QString base = pack.url;
	while (base.endsWith(QLatin1Char('/'))) {
		base.chop(1);
	}
	return base + QStringLiteral("/modpack/") + pack.slug;
}

QVariantList TechnicModpackModel::versionEntries(const Pack& pack)
{
	QVariantMap entry;
	/* Solder serves its recommended build; a zip pack is what it is. */
	entry[QStringLiteral("id")] = QStringLiteral("recommended");
	entry[QStringLiteral("name")] =
		pack.version.isEmpty() ? tr("Recommended build") : pack.version;
	entry[QStringLiteral("versionNumber")] = entry.value(QStringLiteral("name"));
	entry[QStringLiteral("gameVersions")] = QVariant::fromValue(
		pack.minecraftVersion.isEmpty() ? QStringList()
										: QStringList{pack.minecraftVersion});
	entry[QStringLiteral("loaders")] = QVariant::fromValue(QStringList());
	entry[QStringLiteral("datePublished")] = QString();
	entry[QStringLiteral("downloadUrl")] = pack.url;
	entry[QStringLiteral("featured")] = true;
	return {entry};
}

void TechnicModpackModel::search()
{
	if (m_searchJob) {
		m_restartPending = true;
		m_searchJob->abort();
		return;
	}
	startSearch();
}

void TechnicModpackModel::startSearch()
{
	beginResetModel();
	m_packs.clear();
	endResetModel();
	emit countChanged();
	setError(QString());

	m_searchResponse.clear();
	auto* job = new NetJob(QStringLiteral("Technic::Search"), LAUNCHER->network());
	job->addNetAction(
		Net::Download::makeByteArray(searchUrl(m_query), &m_searchResponse));
	m_searchJob = job;
	connect(job, &NetJob::succeeded, this, [this] {
		m_searchJob.reset();
		if (m_restartPending) {
			m_restartPending = false;
			startSearch();
			return;
		}
		const QList<Pack> packs = parseSearch(m_searchResponse);
		if (!packs.isEmpty()) {
			beginInsertRows(QModelIndex(), 0, int(packs.size()) - 1);
			m_packs = packs;
			endInsertRows();
			emit countChanged();
		}
		setSearching(false);
	});
	connect(job, &NetJob::failed, this, [this](QString reason) {
		m_searchJob.reset();
		if (m_restartPending) {
			m_restartPending = false;
			startSearch();
			return;
		}
		setError(reason);
		setSearching(false);
	});
	setSearching(true);
	job->start();
}

TechnicModpackModel::Pack* TechnicModpackModel::packFor(const QString& slug)
{
	for (Pack& pack : m_packs) {
		if (pack.slug == slug) {
			return &pack;
		}
	}
	return nullptr;
}

void TechnicModpackModel::applyDetail(const Pack& pack)
{
	m_detail->setTitle(pack.name);
	m_detail->setBody(pack.description, RichBody::Format::Html);
	m_detail->setVersions(versionEntries(pack));
	QVariantList gallery;
	if (!pack.backgroundUrl.isEmpty()) {
		QVariantMap entry;
		entry[QStringLiteral("url")] = pack.backgroundUrl;
		entry[QStringLiteral("featured")] = true;
		entry[QStringLiteral("title")] = QString();
		gallery.append(entry);
	}
	m_detail->setGallery(gallery);
	m_detail->setLoading(false);
}

void TechnicModpackModel::loadDetail(const QString& projectId)
{
	if (m_detailJob) {
		m_detailJob->abort();
		m_detailJob.reset();
	}
	++m_detailGeneration;
	const quint64 generation = m_detailGeneration;
	m_detail->reset(projectId);

	Pack* pack = packFor(projectId);
	if (!pack) {
		m_detail->setError(tr("This modpack is not in the loaded list."));
		m_detail->setLoading(false);
		return;
	}
	m_detail->setTitle(pack->name);
	if (pack->metadataLoaded) {
		applyDetail(*pack);
		return;
	}

	auto response = std::make_shared<QByteArray>();
	auto* job = new NetJob(QStringLiteral("Technic::PackMeta(%1)").arg(projectId),
						   LAUNCHER->network());
	job->addNetAction(
		Net::Download::makeByteArray(metadataUrl(projectId), response.get()));
	m_detailJob = job;
	connect(job, &NetJob::succeeded, this, [this, response, projectId, generation] {
		if (generation != m_detailGeneration) {
			return;
		}
		m_detailJob.reset();
		Pack* pack = packFor(projectId);
		if (!pack) {
			m_detail->setLoading(false);
			return;
		}
		if (!parseMetadata(*response, *pack)) {
			m_detail->setError(tr("Technic has no download for this modpack."));
			m_detail->setLoading(false);
			return;
		}
		/* The card can show more now: author, summary, background. */
		const int row = int(pack - m_packs.data());
		emit dataChanged(index(row), index(row));
		applyDetail(*pack);
	});
	connect(job, &NetJob::failed, this, [this, generation](QString reason) {
		if (generation != m_detailGeneration) {
			return;
		}
		m_detailJob.reset();
		m_detail->setError(reason);
		m_detail->setLoading(false);
	});
	job->start();
}

QString TechnicModpackModel::resolveIconKey(const Pack& pack) const
{
	/* Where TechnicPage's list keeps logos (TechnicModel::requestLogo()).
	 * A disk-only lookup; never downloads. */
	if (!pack.logoName.isEmpty()) {
		MetaEntryPtr entry = LAUNCHER->metacache()->resolveEntry(
			QStringLiteral("TechnicPacks"),
			QStringLiteral("logos/%1").arg(pack.logoName));
		if (entry && !entry->isStale()) {
			const QString key = QStringLiteral("technic_") + pack.logoName;
			LAUNCHER->icons()->installIcon(entry->getFullPath(), key);
			return key;
		}
	}
	/* No built-in Technic instance icon; the plain block. */
	return QStringLiteral("grass");
}

QObject* TechnicModpackModel::install(const QString& projectId,
									  const QString& versionId,
									  const QString& instanceName,
									  const QString& group)
{
	Q_UNUSED(versionId)
	Pack* pack = packFor(projectId);
	if (!pack || !pack->metadataLoaded) {
		qWarning() << "TechnicModpackModel::install: no metadata for"
				   << projectId << "- was loadDetail() called and finished first?";
		return nullptr;
	}

	/* What TechnicPage::metadataLoaded() hands the New instance dialog,
	 * named and placed the way NewInstanceDialog/MainWindow do. */
	InstanceTask* task = nullptr;
	if (pack->isSolder) {
		task = new Technic::SolderPackInstallTask(
			LAUNCHER->network(), QUrl(solderPackUrl(*pack)), pack->minecraftVersion);
	} else {
		task = new Technic::SingleZipPackInstallTask(QUrl(pack->url),
													 pack->minecraftVersion);
	}
	const QString effectiveName = instanceName.isEmpty() ? pack->name : instanceName;
	task->setName(effectiveName);
	task->setGroup(group);
	task->setIcon(resolveIconKey(*pack));
	task->setTargetDir(LAUNCHER->instances()->primaryDir());

	Task* wrapped = LAUNCHER->instances()->wrapInstanceTask(task);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), this);
	watcher->setTitle(effectiveName);
	wrapped->start();
	return watcher;
}
