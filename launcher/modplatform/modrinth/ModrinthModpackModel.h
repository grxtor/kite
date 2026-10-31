/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileCopyrightText: 2026 grxtor
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
#include "models/RichBody.h"

/* Version list and long description of one modpack, fetched lazily when a
 * row is picked. A QObject so a detail pane can bind to several fields at
 * once. Owned by the ModrinthModpackModel that exposes it as `detail`. */
class ModrinthModpackDetail : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString projectId READ projectId NOTIFY projectIdChanged)
	Q_PROPERTY(QString title READ title NOTIFY titleChanged)
	/* The long description as the provider serves it: Markdown from
	 * Modrinth, FTB and ATLauncher, HTML from CurseForge and Technic (see
	 * each model's `bodyFormat`). */
	Q_PROPERTY(QString body READ body NOTIFY bodyChanged)
	/* The same description as one HTML page (RichBody::toArticleHtml()).
	 * Changes with `body`. */
	Q_PROPERTY(QString bodyHtml READ bodyHtml NOTIFY bodyChanged)
	/* QVariantList of maps: id, name, versionNumber, gameVersions (list),
	 * loaders (list), datePublished, downloadUrl, featured. */
	Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
	/* QVariantList of maps: url, featured, title; empty when the project
	 * has no gallery. */
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
	QString bodyHtml() const
	{
		return m_bodyHtml;
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

	/* Clears body/versions/error, sets loading and switches to `projectId`. */
	void reset(const QString& projectId);
	void setTitle(const QString& title);
	/* @p format: how the provider writes the body (see `body`). */
	void setBody(const QString& body,
				 RichBody::Format format = RichBody::Format::Markdown);
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
	QString m_bodyHtml;
	QVariantList m_versions;
	QVariantList m_gallery;
	bool m_loading = false;
	QString m_error;
};

/* Modrinth modpack search results for QML. Not a ContentProviderModel:
 * modpacks are not a ModPlatform::ContentType (they create instances).
 *
 * `logoUrl` is Modrinth's CDN URL, loaded and cached by QML's Image.
 * install() still needs an icon key for the new instance; see
 * resolveIconKey(). */
class ModrinthModpackModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
	Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY sortChanged)
	Q_PROPERTY(QVariantList sortOptions READ sortOptions CONSTANT)
	Q_PROPERTY(QString gameVersion READ gameVersion WRITE setGameVersion
				   NOTIFY gameVersionChanged)
	/* "" = any loader; otherwise "fabric"/"forge"/"neoforge"/"quilt". */
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

	/* Brings the base overloads back into scope so the QML-facing overloads
	 * of the same name do not trigger -Woverloaded-virtual. */
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

	/* Starts a fresh search from query()/sort()/gameVersion()/loader(),
	 * cancelling one in flight. No debounce: call it when the user is done
	 * typing or picking. */
	Q_INVOKABLE void search();
	/* Fetches the next page; no-op while searching or when canFetchMore()
	 * is false. */
	Q_INVOKABLE void fetchMore();
	/* Fetches versions and long description into `detail`. A stale reply
	 * from an earlier project is dropped. */
	Q_INVOKABLE void loadDetail(const QString& projectId);
	/* Builds and starts the InstanceImportTask for a pack and returns a
	 * TaskWatcher for it (parented to this model). `versionId` must be in
	 * `detail.versions` for `projectId`, so call loadDetail() first. Returns
	 * nullptr and logs a warning if the version is not found. */
	Q_INVOKABLE QObject* install(const QString& projectId,
								 const QString& versionId,
								 const QString& instanceName,
								 const QString& group);

	/* JSON -> rows; static so tests can feed a canned response. */
	static QList<Modrinth::IndexedPack>
	parseSearchResults(const QByteArray& bytes, int& totalHits);
	/* JSON -> the `versions` role of ModrinthModpackDetail; network-free,
	 * reuses Modrinth::loadIndexedPackVersions(). */
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

	/* Icon key for the new instance: the slug's logo if already in the
	 * on-disk "ModrinthPacks" cache, else the built-in "modrinth" icon.
	 * Never downloads. */
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
