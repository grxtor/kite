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

#include "FtbModpackModel.h"

#include <algorithm>
#include <memory>

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTimeZone>

#include "BuildConfig.h"
#include "InstanceList.h"
#include "Json.h"
#include "core/LauncherContext.h"
#include "icons/IconList.h"
#include "modplatform/modpacksch/FTBPackInstallTask.h"
#include "modplatform/modrinth/ModrinthModpackModel.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/* Packs asked for per request round: enough that the list fills in
	 * quickly, few enough that one round never ties up the network. */
	constexpr int kBatchSize = 16;

	QString isoDate(qint64 secondsSinceEpoch)
	{
		return secondsSinceEpoch > 0
				   ? QDateTime::fromSecsSinceEpoch(secondsSinceEpoch, QTimeZone::UTC)
						 .toString(Qt::ISODate)
				   : QString();
	}

	const QStringList& loaderNames()
	{
		static const QStringList names = {
			QStringLiteral("forge"), QStringLiteral("neoforge"),
			QStringLiteral("fabric"), QStringLiteral("quilt")};
		return names;
	}
} // namespace

FtbModpackModel::FtbModpackModel(QObject* parent)
	: QAbstractListModel(parent), m_detail(new ModrinthModpackDetail(this))
{
	/* The widget page's three sorts, and the last update, which the
	 * other providers offer too. */
	const QList<QPair<QString, QString>> sorts = {
		{QStringLiteral("plays"), tr("Sort by Plays")},
		{QStringLiteral("installs"), tr("Sort by Installs")},
		{QStringLiteral("updated"), tr("Sort by Last Updated")},
		{QStringLiteral("name"), tr("Sort by Name")},
	};
	for (const auto& sort : sorts) {
		QVariantMap entry;
		entry[QStringLiteral("id")] = sort.first;
		entry[QStringLiteral("label")] = sort.second;
		m_sortOptions.append(entry);
	}
}

FtbModpackModel::~FtbModpackModel()
{
	if (m_job) {
		m_job->abort();
	}
}

QObject* FtbModpackModel::detail() const
{
	return m_detail;
}

int FtbModpackModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : int(m_rows.size());
}

QString FtbModpackModel::artUrl(const ModpacksCH::Modpack& pack,
								const QString& type)
{
	for (const auto& art : pack.art) {
		if (art.type == type) {
			return art.url;
		}
	}
	return QString();
}

QVariant FtbModpackModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
		return QVariant();
	}
	const ModpacksCH::Modpack& pack = m_packs.at(m_rows.at(index.row()));
	switch (role) {
		case ProjectIdRole:
			return QString::number(pack.id);
		case SlugRole:
			return QString();
		case TitleRole:
			return pack.name;
		case DescriptionRole:
			return pack.synopsis;
		case AuthorRole:
			return pack.authors.isEmpty() ? QString() : pack.authors.first().name;
		case LogoUrlRole:
			return artUrl(pack, QStringLiteral("square"));
		case DownloadsRole:
			return pack.installs;
		case FollowsRole:
			return 0;
		case UpdatedRole:
			return isoDate(pack.updated);
		case LatestVersionRole: {
			const QVariantList versions = versionEntries(pack);
			return versions.isEmpty()
					   ? QString()
					   : versions.first().toMap().value(QStringLiteral("name"));
		}
		case GameVersionsRole:
			return QVariant::fromValue(QStringList());
		case CategoriesRole: {
			QStringList tags;
			for (const auto& tag : pack.tags) {
				tags.append(tag.name);
			}
			return QVariant::fromValue(tags);
		}
		case GalleryUrlRole:
			return artUrl(pack, QStringLiteral("splash"));
		case AccentColorRole:
			return QVariant();
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> FtbModpackModel::roleNames() const
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

void FtbModpackModel::setQuery(const QString& query)
{
	if (m_query == query) {
		return;
	}
	m_query = query;
	emit queryChanged();
}

void FtbModpackModel::setSort(const QString& sort)
{
	if (m_sort == sort) {
		return;
	}
	m_sort = sort;
	emit sortChanged();
}

void FtbModpackModel::setGameVersion(const QString& gameVersion)
{
	if (m_gameVersion == gameVersion) {
		return;
	}
	m_gameVersion = gameVersion;
	emit gameVersionChanged();
}

void FtbModpackModel::setLoader(const QString& loader)
{
	if (m_loader == loader) {
		return;
	}
	m_loader = loader;
	emit loaderChanged();
}

void FtbModpackModel::setLoading(bool loading)
{
	if (m_loading == loading) {
		return;
	}
	m_loading = loading;
	emit searchingChanged();
}

void FtbModpackModel::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}

QList<int> FtbModpackModel::parseIndex(const QByteArray& bytes)
{
	QList<int> ids;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes);
	for (const auto& raw : doc.object().value(QStringLiteral("packs")).toArray()) {
		const int id = raw.toInt();
		if (id > 0) {
			ids.append(id);
		}
	}
	return ids;
}

bool FtbModpackModel::parsePack(const QByteArray& bytes,
								ModpacksCH::Modpack& out)
{
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		return false;
	}
	QJsonObject obj = doc.object();
	try {
		ModpacksCH::loadModpack(out, obj);
	} catch (const JSONValidationError& e) {
		qWarning() << "Error while reading a pack from FTB:" << e.cause();
		return false;
	}
	/* No guarantee a pack has a version; one without is a dud. */
	return !out.versions.isEmpty();
}

QList<int> FtbModpackModel::filterAndSort(const QList<ModpacksCH::Modpack>& packs,
										  const QString& query,
										  const QString& loader,
										  const QString& sort)
{
	const QString term = query.trimmed();
	QList<int> rows;
	for (int i = 0; i < packs.size(); ++i) {
		const auto& pack = packs.at(i);
		if (!term.isEmpty()) {
			bool hit = pack.name.contains(term, Qt::CaseInsensitive) ||
					   pack.synopsis.contains(term, Qt::CaseInsensitive);
			for (const auto& tag : pack.tags) {
				hit = hit || tag.name.contains(term, Qt::CaseInsensitive);
			}
			if (!hit) {
				continue;
			}
		}
		if (!loader.isEmpty()) {
			/* Only a pack that names a loader in its tags can be ruled
			 * out by one; plenty name none at all. */
			QSet<QString> tagged;
			for (const auto& tag : pack.tags) {
				if (loaderNames().contains(tag.name.toLower())) {
					tagged.insert(tag.name.toLower());
				}
			}
			if (!tagged.isEmpty() && !tagged.contains(loader.toLower())) {
				continue;
			}
		}
		rows.append(i);
	}

	std::stable_sort(rows.begin(), rows.end(), [&packs, &sort](int a, int b) {
		const auto& pa = packs.at(a);
		const auto& pb = packs.at(b);
		if (sort == QStringLiteral("installs")) {
			return pa.installs > pb.installs;
		}
		if (sort == QStringLiteral("updated")) {
			return pa.updated > pb.updated;
		}
		if (sort == QStringLiteral("name")) {
			return pa.name.compare(pb.name, Qt::CaseInsensitive) < 0;
		}
		return pa.plays > pb.plays;
	});
	return rows;
}

QVariantList FtbModpackModel::versionEntries(const ModpacksCH::Modpack& pack)
{
	auto versions = pack.versions;
	/* Newest first; a version id only ever grows, so it settles a tie. */
	std::stable_sort(versions.begin(), versions.end(),
					 [](const ModpacksCH::VersionInfo& a,
						const ModpacksCH::VersionInfo& b) {
						 if (a.updated != b.updated) {
							 return a.updated > b.updated;
						 }
						 return a.id > b.id;
					 });
	QVariantList result;
	bool featuredSet = false;
	for (const auto& version : versions) {
		QVariantMap entry;
		entry[QStringLiteral("id")] = QString::number(version.id);
		entry[QStringLiteral("name")] = version.name;
		entry[QStringLiteral("versionNumber")] = version.name;
		entry[QStringLiteral("gameVersions")] = QVariant::fromValue(QStringList());
		entry[QStringLiteral("loaders")] = QVariant::fromValue(QStringList());
		entry[QStringLiteral("datePublished")] = isoDate(version.updated);
		entry[QStringLiteral("downloadUrl")] = QString();
		/* The newest release is the one to start on, ahead of betas. */
		const bool release =
			version.type.compare(QStringLiteral("release"), Qt::CaseInsensitive) == 0;
		entry[QStringLiteral("featured")] = release && !featuredSet;
		featuredSet = featuredSet || release;
		result.append(entry);
	}
	return result;
}

void FtbModpackModel::search()
{
	if (!m_loaded && !m_loading) {
		startLoading();
		return;
	}
	refilter();
}

void FtbModpackModel::refilter()
{
	beginResetModel();
	m_rows = filterAndSort(m_packs, m_query, m_loader, m_sort);
	endResetModel();
	emit countChanged();
}

void FtbModpackModel::startLoading()
{
	setError(QString());
	setLoading(true);
	auto response = std::make_shared<QByteArray>();
	auto* job = new NetJob(QStringLiteral("Ftb::Index"), LAUNCHER->network());
	job->addNetAction(Net::Download::makeByteArray(
		QUrl(BuildConfig.MODPACKSCH_API_BASE_URL +
			 QStringLiteral("public/modpack/all")),
		response.get()));
	m_job = job;
	connect(job, &NetJob::succeeded, this, [this, response] {
		m_job.reset();
		m_pending = parseIndex(*response);
		if (m_pending.isEmpty()) {
			setError(tr("FTB sent back no modpacks."));
			setLoading(false);
			return;
		}
		loadNextBatch();
	});
	connect(job, &NetJob::failed, this, [this](QString reason) {
		m_job.reset();
		setError(reason);
		setLoading(false);
	});
	job->start();
}

void FtbModpackModel::loadNextBatch()
{
	const int take = std::min<int>(kBatchSize, int(m_pending.size()));
	const QList<int> ids = m_pending.mid(0, take);
	m_pending.remove(0, take);

	auto* job = new NetJob(QStringLiteral("Ftb::Packs"), LAUNCHER->network());
	auto responses = std::make_shared<QList<std::shared_ptr<QByteArray>>>();
	for (const int id : ids) {
		auto response = std::make_shared<QByteArray>();
		responses->append(response);
		job->addNetAction(Net::Download::makeByteArray(
			QUrl(BuildConfig.MODPACKSCH_API_BASE_URL +
				 QStringLiteral("public/modpack/%1").arg(id)),
			response.get()));
	}
	m_job = job;

	/* A pack that failed to arrive is simply missing from the list, as
	 * it was from the widget page; the rest of the round still counts. */
	auto finish = [this, responses] {
		m_job.reset();
		const bool firstRound = m_packs.isEmpty();
		for (const auto& response : *responses) {
			ModpacksCH::Modpack pack;
			if (parsePack(*response, pack)) {
				m_packs.append(pack);
			}
		}
		const bool done = m_pending.isEmpty();
		/* Shown after the first round, so the page is not empty for long,
		 * and once everything is in; re-sorting on every round would
		 * shuffle the grid under the user's pointer. */
		if (firstRound || done) {
			refilter();
		}
		if (done) {
			m_loaded = true;
			setLoading(false);
		} else {
			loadNextBatch();
		}
	};
	connect(job, &NetJob::succeeded, this, finish);
	connect(job, &NetJob::failed, this, [finish](QString) { finish(); });
	job->start();
}

const ModpacksCH::Modpack* FtbModpackModel::packFor(const QString& projectId) const
{
	for (const auto& pack : m_packs) {
		if (QString::number(pack.id) == projectId) {
			return &pack;
		}
	}
	return nullptr;
}

void FtbModpackModel::loadDetail(const QString& projectId)
{
	m_detail->reset(projectId);
	const ModpacksCH::Modpack* pack = packFor(projectId);
	if (!pack) {
		m_detail->setError(tr("This modpack is not in the loaded list."));
		m_detail->setLoading(false);
		return;
	}
	m_detail->setTitle(pack->name);
	m_detail->setBody(pack->description);
	m_detail->setVersions(versionEntries(*pack));
	QVariantList gallery;
	for (const auto& art : pack->art) {
		if (art.type == QStringLiteral("square") || art.url.isEmpty()) {
			continue;
		}
		QVariantMap entry;
		entry[QStringLiteral("url")] = art.url;
		entry[QStringLiteral("featured")] = art.type == QStringLiteral("splash");
		entry[QStringLiteral("title")] = QString();
		gallery.append(entry);
	}
	m_detail->setGallery(gallery);
	m_detail->setLoading(false);
}

QString FtbModpackModel::resolveIconKey(const ModpacksCH::Modpack& pack) const
{
	/* Where FtbPage's list keeps logos (FtbListModel::requestLogo()),
	 * keyed by pack name, with a squared ".small" copy beside it. A
	 * disk-only lookup; never downloads. */
	MetaEntryPtr entry = LAUNCHER->metacache()->resolveEntry(
		QStringLiteral("ModpacksCHPacks"),
		QStringLiteral("logos/%1").arg(pack.name.section(QLatin1Char('.'), 0, 0)));
	if (entry && !entry->isStale()) {
		const QString small = entry->getFullPath() + QStringLiteral(".small");
		const QString key = QStringLiteral("ftb_%1").arg(pack.id);
		LAUNCHER->icons()->installIcon(
			QFileInfo::exists(small) ? small : entry->getFullPath(), key);
		return key;
	}
	return QStringLiteral("ftb_logo");
}

QObject* FtbModpackModel::install(const QString& projectId,
								  const QString& versionId,
								  const QString& instanceName,
								  const QString& group)
{
	const ModpacksCH::Modpack* pack = packFor(projectId);
	if (!pack) {
		qWarning() << "FtbModpackModel::install: pack" << projectId
				   << "is not loaded";
		return nullptr;
	}
	QString versionName;
	for (const auto& version : pack->versions) {
		if (QString::number(version.id) == versionId) {
			versionName = version.name;
			break;
		}
	}
	if (versionName.isEmpty()) {
		qWarning() << "FtbModpackModel::install: no version" << versionId
				   << "in pack" << projectId;
		return nullptr;
	}

	/* What FtbPage::suggestCurrent() hands the New instance dialog, named
	 * and placed the way NewInstanceDialog/MainWindow do. The task
	 * fetches the version's manifest and files itself. */
	auto* task = new ModpacksCH::PackInstallTask(*pack, versionName);
	const QString effectiveName = instanceName.isEmpty()
									  ? pack->name + QLatin1Char(' ') + versionName
									  : instanceName;
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
