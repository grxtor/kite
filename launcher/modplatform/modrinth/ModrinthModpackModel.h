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

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariant>

#include <net/NetJob.h>

#include "modplatform/modrinth/ModrinthPackIndex.h"

/* The version list and long description of one modpack, fetched lazily
 * once the user picks a row - the QML-facing, widget-free counterpart of
 * what ModrinthPage.cpp keeps in `current` plus what its version combo box
 * shows. A plain QObject rather than a role on the list model itself,
 * because a detail pane binds to several fields at once and QML has no
 * convenient way to bind to "the currently selected row of some other
 * model".
 *
 * Owned by, and never outlives, the ModrinthModpackModel that exposes it
 * through its `detail` property. */
class ModrinthModpackDetail : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString projectId READ projectId NOTIFY projectIdChanged)
	Q_PROPERTY(QString title READ title NOTIFY titleChanged)
	/* The project's long description, as Modrinth serves it: Markdown,
	 * not HTML - unlike ContentProviderModel::parseBodyResponse(), which
	 * both providers it wraps do convert. Left as-is here because
	 * nothing in the core can render Markdown; that is squarely a QML
	 * concern (a Markdown-aware Text item, or a conversion done in QML/
	 * JS) and does not belong in MeshMC_core. */
	Q_PROPERTY(QString body READ body NOTIFY bodyChanged)
	/* QVariantList of maps: id, name, versionNumber, gameVersions
	 * (list), loaders (list), datePublished, downloadUrl, featured. */
	Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
	/* QVariantList of maps: url, featured, title - the project's own
	 * gallery, straight from the same project fetch body() comes from
	 * (see fetchDetailBody()). Empty when the project has no gallery. */
	Q_PROPERTY(QVariantList gallery READ gallery NOTIFY galleryChanged)
	Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  public:
	explicit ModrinthModpackDetail(QObject* parent = nullptr);

	QString projectId() const
	{
		return m_projectId;
	}
	QString title() const
	{
		return m_title;
	}
	QString body() const
	{
		return m_body;
	}
	QVariantList versions() const
	{
		return m_versions;
	}
	QVariantList gallery() const
	{
		return m_gallery;
	}
	bool loading() const
	{
		return m_loading;
	}
	QString error() const
	{
		return m_error;
	}

	/* Clears body/versions/error, sets loading, and switches to
	 * `projectId` - called by ModrinthModpackModel::loadDetail() before
	 * it starts the two fetches. */
	void reset(const QString& projectId);
	void setTitle(const QString& title);
	void setBody(const QString& body);
	void setVersions(const QVariantList& versions);
	void setGallery(const QVariantList& gallery);
	void setLoading(bool loading);
	void setError(const QString& error);

  signals:
	void projectIdChanged();
	void titleChanged();
	void bodyChanged();
	void versionsChanged();
	void galleryChanged();
	void loadingChanged();
	void errorChanged();

  private:
	QString m_projectId;
	QString m_title;
	QString m_body;
	QVariantList m_versions;
	QVariantList m_gallery;
	bool m_loading = false;
	QString m_error;
};

/* Modrinth *modpack* search results for QML - the widget-free replacement
 * for ui/pages/modplatform/modrinth/ModrinthModel.h (Modrinth::ListModel),
 * which backs ModrinthPage today.
 *
 * Deliberately its own class rather than a widened ContentProviderModel:
 * modpacks are not a ModPlatform::ContentType (see ContentApi.h - "modpacks
 * create instances rather than being installed into one"), so the two
 * families of search share a shape but not a base class. See
 * ModrinthPackIndex.h for the parsed fields this exposes.
 *
 * Icons are handled the simplest possible way for v1: `logoUrl` is
 * Modrinth's own CDN URL, and QML's `Image { source: logoUrl }` fetches
 * and caches it directly. Nothing here downloads or caches a QIcon the way
 * ContentProviderModel/Modrinth::ListModel do for their QWidget delegates -
 * that machinery has no QML consumer. install() below still has to name a
 * disk-resident *icon key* for the new instance, which it resolves from
 * the launcher's existing on-disk icon cache without downloading anything;
 * see resolveIconKey().
 */
class ModrinthModpackModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
	Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY sortChanged)
	Q_PROPERTY(QVariantList sortOptions READ sortOptions CONSTANT)
	Q_PROPERTY(QString gameVersion READ gameVersion WRITE setGameVersion
				   NOTIFY gameVersionChanged)
	/* "" = any loader; otherwise one of "fabric"/"forge"/"neoforge"/
	 * "quilt". */
	Q_PROPERTY(
		QString loader READ loader WRITE setLoader NOTIFY loaderChanged)
	Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
	Q_PROPERTY(
		bool canFetchMore READ canFetchMore NOTIFY canFetchMoreChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	Q_PROPERTY(QObject* detail READ detail CONSTANT)

  public:
	enum Roles {
		ProjectIdRole = Qt::UserRole + 1,
		SlugRole,
		TitleRole,
		DescriptionRole,
		AuthorRole,
		LogoUrlRole,
		DownloadsRole,
		FollowsRole,
		UpdatedRole,
		LatestVersionRole,
		GameVersionsRole,
		CategoriesRole,
		GalleryUrlRole,
		AccentColorRole,
	};
	Q_ENUM(Roles)

	explicit ModrinthModpackModel(QObject* parent = nullptr);
	~ModrinthModpackModel() override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index,
				 int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	/* `sort`, `canFetchMore` and `fetchMore` below are QML-facing
	 * convenience overloads (zero args, or an id string rather than a
	 * column) that happen to share a name with a QAbstractItemModel
	 * virtual of a different arity. These bring the base class's own
	 * overloads back into scope alongside them, which is what silences
	 * -Woverloaded-virtual - nothing here actually needs column
	 * sorting or index-based fetching, but a caller with a plain
	 * QAbstractItemModel* still should be able to reach them. */
	using QAbstractItemModel::canFetchMore;
	using QAbstractItemModel::fetchMore;
	using QAbstractItemModel::sort;

	QString query() const
	{
		return m_query;
	}
	void setQuery(const QString& query);

	QString sort() const
	{
		return m_sort;
	}
	void setSort(const QString& sort);

	QVariantList sortOptions() const
	{
		return m_sortOptions;
	}

	QString gameVersion() const
	{
		return m_gameVersion;
	}
	void setGameVersion(const QString& gameVersion);

	QString loader() const
	{
		return m_loader;
	}
	void setLoader(const QString& loader);

	bool searching() const
	{
		return m_searching;
	}
	bool canFetchMore() const
	{
		return m_canFetchMore;
	}
	int count() const
	{
		return m_packs.size();
	}
	QString error() const
	{
		return m_error;
	}
	QObject* detail() const
	{
		return m_detail;
	}

	/* Starts a fresh search from query()/sort()/gameVersion()/loader().
	 * Cancels a search already in flight rather than queuing behind it -
	 * matches Modrinth::ListModel::searchWithTerm() and
	 * ContentProviderModel::search(). No debounce: the caller (QML) is
	 * expected to only call this when the user is actually done typing/
	 * picking, e.g. on Enter or a filter changing. */
	Q_INVOKABLE void search();
	/* Fetches the next page of the current search. A no-op while a
	 * search is already running or canFetchMore() is false. */
	Q_INVOKABLE void fetchMore();
	/* Fetches the version list and long description for one project,
	 * filling `detail`. Safe to call again for a different project while
	 * one is already loading - the stale reply is dropped. */
	Q_INVOKABLE void loadDetail(const QString& projectId);
	/* Builds and starts the same InstanceImportTask that
	 * NewInstanceDialog::extractTask() + MainWindow::createInstanceFromDialog()
	 * build for a pack picked in the widget Modrinth browser, and returns
	 * a TaskWatcher for it (parented to this model). `versionId` must be
	 * one of the ids in `detail.versions` for `projectId` - call
	 * loadDetail() first. Returns nullptr (and logs a warning) if that
	 * version cannot be found, e.g. because loadDetail() was never
	 * called or has not finished yet. */
	Q_INVOKABLE QObject* install(const QString& projectId,
								 const QString& versionId,
								 const QString& instanceName,
								 const QString& group);

	/* JSON -> rows, pulled out as a static function so it can be unit
	 * tested with a canned response and no network. */
	static QList<Modrinth::IndexedPack>
	parseSearchResults(const QByteArray& bytes, int& totalHits);
	/* JSON -> the `versions` role of ModrinthModpackDetail, likewise
	 * network-free and unit testable on its own. Reuses
	 * Modrinth::loadIndexedPackVersions() for the actual parsing. */
	static QVariantList parseVersionsJson(const QByteArray& bytes,
										  const QString& projectId);

  signals:
	void queryChanged();
	void sortChanged();
	void gameVersionChanged();
	void loaderChanged();
	void searchingChanged();
	void canFetchMoreChanged();
	void countChanged();
	void errorChanged();

  private:
	void setSearching(bool searching);
	void setCanFetchMore(bool canFetchMore);
	void setError(const QString& error);

	int sortIndexOf(const QString& id) const;

	void restartSearch();
	void performSearch();
	void onSearchSucceeded();
	void onSearchFailed(QString reason);

	void fetchDetailBody(const QString& projectId, quint64 generation);
	void fetchDetailVersions(const QString& projectId, quint64 generation);
	void markDetailPartDone(quint64 generation, bool isBody);

	/* The icon key to give the new instance: the slug's logo if it is
	 * already sitting in the launcher's on-disk cache (the same
	 * "ModrinthPacks" bucket the widget Modrinth page has always used,
	 * so anything fetched by that page already helps here too), or the
	 * built-in "modrinth" icon otherwise. Never starts a download - see
	 * the class comment. */
	QString resolveIconKey(const QString& slug, const QString& iconUrl) const;

  private:
	QList<Modrinth::IndexedPack> m_packs;

	QString m_query;
	QString m_sort;
	QVariantList m_sortOptions;
	QString m_gameVersion;
	QString m_loader;
	bool m_searching = false;
	bool m_canFetchMore = false;
	QString m_error;

	int m_nextOffset = 0;
	bool m_restartPending = false;
	NetJob::Ptr m_searchJob;
	QByteArray m_searchResponse;

	ModrinthModpackDetail* m_detail = nullptr;
	quint64 m_detailGeneration = 0;
	bool m_bodyDone = true;
	bool m_versionsDone = true;
	NetJob::Ptr m_bodyJob;
	NetJob::Ptr m_versionsJob;
};
