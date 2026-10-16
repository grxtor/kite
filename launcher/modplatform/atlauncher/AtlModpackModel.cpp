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

#include "AtlModpackModel.h"

#include <algorithm>

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "BuildConfig.h"
#include "InstanceList.h"
#include "Json.h"
#include "Version.h"
#include "core/LauncherContext.h"
#include "core/UiHost.h"
#include "icons/IconList.h"
#include "meta/Version.h"
#include "models/AtlOptionalModsModel.h"
#include "modplatform/modrinth/ModrinthModpackModel.h"
#include "net/Download.h"
#include "net/HttpMetaCache.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/* Loader versions offered when a pack leaves its loader open: the
	 * host asks with one button per answer, so a handful, not hundreds. */
	constexpr int kMaxLoaderChoices = 5;

	/* ATLauncher::UserInteractionSupport for one install: asks through
	 * the model's optional-mods handler and the UiHost, or falls back to
	 * the pack's own defaults when there is nobody to ask. Lives as long
	 * as the install's watcher, which is its parent. */
	class AtlInstallQuestions : public QObject,
								public ATLauncher::UserInteractionSupport
	{
	  public:
		AtlInstallQuestions(QString packName,
							AtlModpackModel::OptionalModsHandler handler,
							QObject* parent)
			: QObject(parent), m_packName(std::move(packName)),
			  m_handler(std::move(handler))
		{
		}

		QVector<QString>
		chooseOptionalMods(QVector<ATLauncher::VersionMod> mods) override
		{
			AtlOptionalModsModel choices(mods);
			if (m_handler) {
				if (auto picked = m_handler(m_packName, choices)) {
					return *picked;
				}
			}
			/* Nobody to ask, or nothing changed: what the pack ticks. */
			return choices.selectedNames();
		}

		QString chooseVersion(Meta::VersionListPtr vlist,
							  QString minecraftVersion) override
		{
			if (!vlist) {
				return QString();
			}
			const auto choices = AtlModpackModel::versionChoices(
				vlist->versions(), minecraftVersion, kMaxLoaderChoices);
			if (choices.isEmpty()) {
				return QString();
			}
			UiHost* host = LAUNCHER ? LAUNCHER->uiHost() : nullptr;
			if (!host || choices.size() == 1) {
				return choices.first()->descriptor();
			}
			QStringList labels;
			for (const auto& version : choices) {
				labels.append(version->isRecommended()
								  ? QObject::tr("%1 (recommended)")
										.arg(version->descriptor())
								  : version->descriptor());
			}
			const int picked = host->choose(
				QObject::tr("Choose a loader version"),
				QObject::tr("%1 can run on more than one version of its "
							"loader. Pick one to continue.")
					.arg(m_packName),
				UiHost::Severity::Question, labels);
			return picked >= 0 && picked < choices.size()
					   ? choices.at(picked)->descriptor()
					   : QString();
		}

	  private:
		QString m_packName;
		AtlModpackModel::OptionalModsHandler m_handler;
	};
} // namespace

AtlModpackModel::AtlModpackModel(QObject* parent)
	: QAbstractListModel(parent), m_detail(new ModrinthModpackDetail(this))
{
	/* The widget page's three sorts. */
	const QList<QPair<QString, QString>> sorts = {
		{QStringLiteral("popularity"), tr("Sort by Popularity")},
		{QStringLiteral("gameVersion"), tr("Sort by Game Version")},
		{QStringLiteral("name"), tr("Sort by Name")},
	};
	for (const auto& sort : sorts) {
		QVariantMap entry;
		entry[QStringLiteral("id")] = sort.first;
		entry[QStringLiteral("label")] = sort.second;
		m_sortOptions.append(entry);
	}
}

AtlModpackModel::~AtlModpackModel()
{
	if (m_job) {
		m_job->abort();
	}
}

QObject* AtlModpackModel::detail() const
{
	return m_detail;
}

void AtlModpackModel::setOptionalModsHandler(OptionalModsHandler handler)
{
	m_optionalModsHandler = std::move(handler);
}

int AtlModpackModel::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : int(m_rows.size());
}

QString AtlModpackModel::logoUrl(const ATLauncher::IndexedPack& pack)
{
	return QStringLiteral("%1launcher/images/%2.png")
		.arg(BuildConfig.ATL_DOWNLOAD_SERVER_URL, pack.safeName.toLower());
}

QVariant AtlModpackModel::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
		return QVariant();
	}
	const ATLauncher::IndexedPack& pack = m_packs.at(m_rows.at(index.row()));
	switch (role) {
		case ProjectIdRole:
			return pack.safeName;
		case SlugRole:
			return pack.safeName;
		case TitleRole:
			return pack.name;
		case DescriptionRole:
			return pack.description;
		case AuthorRole:
			/* The catalogue names no author. */
			return QString();
		case LogoUrlRole:
			return logoUrl(pack);
		case DownloadsRole:
		case FollowsRole:
			return 0;
		case UpdatedRole:
			return QString();
		case LatestVersionRole:
			return pack.versions.isEmpty() ? QString()
										   : pack.versions.first().version;
		case GameVersionsRole: {
			QStringList versions;
			for (const auto& version : pack.versions) {
				if (!versions.contains(version.minecraft)) {
					versions.append(version.minecraft);
				}
			}
			return QVariant::fromValue(versions);
		}
		case CategoriesRole:
			return QVariant::fromValue(QStringList());
		case GalleryUrlRole:
		case AccentColorRole:
			return QVariant();
		default:
			return QVariant();
	}
}

QHash<int, QByteArray> AtlModpackModel::roleNames() const
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

void AtlModpackModel::setQuery(const QString& query)
{
	if (m_query == query) {
		return;
	}
	m_query = query;
	emit queryChanged();
}

void AtlModpackModel::setSort(const QString& sort)
{
	if (m_sort == sort) {
		return;
	}
	m_sort = sort;
	emit sortChanged();
}

void AtlModpackModel::setGameVersion(const QString& gameVersion)
{
	if (m_gameVersion == gameVersion) {
		return;
	}
	m_gameVersion = gameVersion;
	emit gameVersionChanged();
}

void AtlModpackModel::setLoader(const QString& loader)
{
	if (m_loader == loader) {
		return;
	}
	m_loader = loader;
	emit loaderChanged();
}

void AtlModpackModel::setLoading(bool loading)
{
	if (m_loading == loading) {
		return;
	}
	m_loading = loading;
	emit searchingChanged();
}

void AtlModpackModel::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}

QList<ATLauncher::IndexedPack>
AtlModpackModel::parseCatalogue(const QByteArray& bytes)
{
	QList<ATLauncher::IndexedPack> packs;
	QJsonParseError parseError;
	const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
	if (parseError.error != QJsonParseError::NoError) {
		qWarning() << "Error while parsing JSON response from ATLauncher at"
				   << parseError.offset << "reason:" << parseError.errorString();
		return packs;
	}
	for (const auto& raw : doc.array()) {
		QJsonObject obj = raw.toObject();
		ATLauncher::IndexedPack pack;
		try {
			ATLauncher::loadIndexedPack(pack, obj);
		} catch (const JSONValidationError& e) {
			/* One bad entry is skipped rather than losing the whole list,
			 * which is what the widget list did. */
			qWarning() << "Error while reading a pack from ATLauncher:"
					   << e.cause();
			continue;
		}
		if (pack.versions.isEmpty() ||
			pack.type != ATLauncher::PackType::Public || pack.system) {
			continue;
		}
		packs.append(pack);
	}
	return packs;
}

QList<int> AtlModpackModel::filterAndSort(
	const QList<ATLauncher::IndexedPack>& packs, const QString& query,
	const QString& gameVersion, const QString& sort)
{
	const QString term = query.trimmed();
	QList<int> rows;
	for (int i = 0; i < packs.size(); ++i) {
		const auto& pack = packs.at(i);
		if (!term.isEmpty() && !pack.name.contains(term, Qt::CaseInsensitive) &&
			!pack.description.contains(term, Qt::CaseInsensitive)) {
			continue;
		}
		if (!gameVersion.isEmpty()) {
			const bool runs = std::any_of(
				pack.versions.begin(), pack.versions.end(),
				[&gameVersion](const ATLauncher::IndexedVersion& v) {
					return v.minecraft == gameVersion;
				});
			if (!runs) {
				continue;
			}
		}
		rows.append(i);
	}

	std::stable_sort(rows.begin(), rows.end(), [&packs, &sort](int a, int b) {
		const auto& pa = packs.at(a);
		const auto& pb = packs.at(b);
		if (sort == QStringLiteral("name")) {
			return pa.name.compare(pb.name, Qt::CaseInsensitive) < 0;
		}
		if (sort == QStringLiteral("gameVersion")) {
			/* Newest Minecraft first, by each pack's latest version. */
			return Version(pa.versions.first().minecraft) >
				   Version(pb.versions.first().minecraft);
		}
		/* ATLauncher's own order: a lower position is more popular. */
		return pa.position < pb.position;
	});
	return rows;
}

QVariantList AtlModpackModel::versionEntries(const ATLauncher::IndexedPack& pack)
{
	QVariantList result;
	for (int i = 0; i < pack.versions.size(); ++i) {
		const auto& version = pack.versions.at(i);
		QVariantMap entry;
		entry[QStringLiteral("id")] = version.version;
		entry[QStringLiteral("name")] = version.version;
		entry[QStringLiteral("versionNumber")] = version.version;
		entry[QStringLiteral("gameVersions")] =
			QVariant::fromValue(QStringList{version.minecraft});
		entry[QStringLiteral("loaders")] = QVariant::fromValue(QStringList());
		entry[QStringLiteral("datePublished")] = QString();
		entry[QStringLiteral("downloadUrl")] = QString();
		/* The catalogue lists the newest first. */
		entry[QStringLiteral("featured")] = i == 0;
		result.append(entry);
	}
	return result;
}

QList<Meta::VersionPtr>
AtlModpackModel::versionChoices(const QVector<Meta::VersionPtr>& versions,
								const QString& minecraftVersion, int limit)
{
	QList<Meta::VersionPtr> fits;
	for (const auto& version : versions) {
		if (!version) {
			continue;
		}
		if (!minecraftVersion.isEmpty()) {
			const auto& reqs = version->requirements();
			const auto it = std::find_if(reqs.begin(), reqs.end(),
										 [](const Meta::Require& req) {
											 return req.uid ==
													QStringLiteral("net.minecraft");
										 });
			if (it == reqs.end() || it->equalsVersion != minecraftVersion) {
				continue;
			}
		}
		fits.append(version);
	}
	/* Recommended first, otherwise the list's own (newest-first) order. */
	std::stable_sort(fits.begin(), fits.end(),
					 [](const Meta::VersionPtr& a, const Meta::VersionPtr& b) {
						 return a->isRecommended() && !b->isRecommended();
					 });
	return fits.mid(0, limit);
}

void AtlModpackModel::search()
{
	if (m_loaded || m_loading) {
		refilter();
		return;
	}
	setError(QString());
	setLoading(true);
	m_response.clear();
	auto* job = new NetJob(QStringLiteral("Atl::Catalogue"), LAUNCHER->network());
	job->addNetAction(Net::Download::makeByteArray(
		QUrl(BuildConfig.ATL_DOWNLOAD_SERVER_URL +
			 QStringLiteral("launcher/json/packsnew.json")),
		&m_response));
	m_job = job;
	connect(job, &NetJob::succeeded, this, [this] {
		m_job.reset();
		m_packs = parseCatalogue(m_response);
		m_loaded = true;
		if (m_packs.isEmpty()) {
			setError(tr("ATLauncher sent back no modpacks."));
		}
		refilter();
		setLoading(false);
	});
	connect(job, &NetJob::failed, this, [this](QString reason) {
		m_job.reset();
		setError(reason);
		setLoading(false);
	});
	job->start();
}

void AtlModpackModel::refilter()
{
	beginResetModel();
	m_rows = filterAndSort(m_packs, m_query, m_gameVersion, m_sort);
	endResetModel();
	emit countChanged();
}

const ATLauncher::IndexedPack*
AtlModpackModel::packFor(const QString& projectId) const
{
	for (const auto& pack : m_packs) {
		if (pack.safeName == projectId) {
			return &pack;
		}
	}
	return nullptr;
}

void AtlModpackModel::loadDetail(const QString& projectId)
{
	m_detail->reset(projectId);
	const ATLauncher::IndexedPack* pack = packFor(projectId);
	if (!pack) {
		m_detail->setError(tr("This modpack is not in the loaded list."));
		m_detail->setLoading(false);
		return;
	}
	m_detail->setTitle(pack->name);
	m_detail->setBody(pack->description);
	m_detail->setVersions(versionEntries(*pack));
	m_detail->setLoading(false);
}

QString AtlModpackModel::resolveIconKey(const ATLauncher::IndexedPack& pack) const
{
	/* Where AtlPage's list keeps logos (AtlListModel::requestLogo()). A
	 * disk-only lookup; never downloads. */
	MetaEntryPtr entry = LAUNCHER->metacache()->resolveEntry(
		QStringLiteral("ATLauncherPacks"),
		QStringLiteral("logos/%1").arg(pack.safeName.section(QLatin1Char('.'), 0, 0)));
	if (entry && !entry->isStale()) {
		const QString key = QStringLiteral("atl_") + pack.safeName.toLower();
		LAUNCHER->icons()->installIcon(entry->getFullPath(), key);
		return key;
	}
	/* No built-in ATLauncher instance icon; the plain block. */
	return QStringLiteral("grass");
}

QObject* AtlModpackModel::install(const QString& projectId,
								  const QString& versionId,
								  const QString& instanceName,
								  const QString& group)
{
	const ATLauncher::IndexedPack* pack = packFor(projectId);
	if (!pack) {
		qWarning() << "AtlModpackModel::install: pack" << projectId
				   << "is not loaded";
		return nullptr;
	}
	const bool known = std::any_of(pack->versions.begin(), pack->versions.end(),
								   [&versionId](const ATLauncher::IndexedVersion& v) {
									   return v.version == versionId;
								   });
	if (!known) {
		qWarning() << "AtlModpackModel::install: no version" << versionId
				   << "in pack" << projectId;
		return nullptr;
	}

	/* What AtlPage::suggestCurrent() hands the New instance dialog, named
	 * and placed the way NewInstanceDialog/MainWindow do. The questions
	 * object outlives the task: both end with the watcher. */
	const QString effectiveName =
		instanceName.isEmpty() ? pack->name + QLatin1Char(' ') + versionId
							   : instanceName;
	auto* questions = new AtlInstallQuestions(pack->name, m_optionalModsHandler,
											  nullptr);
	auto* task = new ATLauncher::PackInstallTask(questions, pack->safeName,
												 versionId);
	task->setName(effectiveName);
	task->setGroup(group);
	task->setIcon(resolveIconKey(*pack));
	task->setTargetDir(LAUNCHER->instances()->primaryDir());

	Task* wrapped = LAUNCHER->instances()->wrapInstanceTask(task);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), this);
	questions->setParent(watcher);
	watcher->setTitle(effectiveName);
	wrapped->start();
	return watcher;
}
