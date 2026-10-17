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
#include <QStringList>
#include <QVariant>

#include <net/NetJob.h>

#include "modplatform/flame/FlamePackIndex.h"

class ModrinthModpackDetail;

/* CurseForge *modpack* search results for QML - the widget-free
 * replacement for ui/pages/modplatform/flame/FlameModel.h
 * (Flame::ListModel), which backs FlamePage.
 *
 * Deliberately the same shape as ModrinthModpackModel - the same
 * properties, the same invokables and the same role names - so the
 * Discover page and its ModpackCard/ModpackDetail work on either through
 * one set of bindings: a provider switch only swaps which model they look
 * at. `detail` is a ModrinthModpackDetail, which despite its name is a
 * plain holder for one pack's title, body, versions and gallery; its
 * `body` here is CurseForge's HTML rather than Modrinth's Markdown (see
 * `bodyFormat`).
 *
 * CurseForge needs an API key (BuildConfig.CURSEFORGE_API_KEY); without
 * one every request is refused, so search() says so in `error` instead of
 * asking, and `keyMissing` lets the page explain it up front.
 *
 * Logos are plain CDN URLs for QML's Image to fetch; nothing here caches
 * a QIcon. install() names the new instance's icon from the launcher's
 * disk cache when the widget page has already fetched the logo there,
 * the built-in CurseForge icon otherwise.
 */
class FlameModpackModel : public QAbstractListModel
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
	/// "CurseForge", for copy that names the provider.
	Q_PROPERTY(QString providerName READ providerName CONSTANT)
	/// "html": `detail.body` is HTML (ModrinthModpackModel's is Markdown).
	Q_PROPERTY(QString bodyFormat READ bodyFormat CONSTANT)
	/// This build has no CurseForge API key, so nothing can be searched.
	Q_PROPERTY(bool keyMissing READ keyMissing CONSTANT)

  public:
	/* The same role names ModrinthModpackModel has, so one delegate
	 * reads either. */
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

	/* One search hit: what Flame::loadIndexedPack() reads (and its check
	 * that the pack has a usable file), plus what a card shows that the
	 * widget list never needed. */
	struct Pack {
		Flame::IndexedPack base;
		QString slug;
		qint64 downloads = 0;
		int thumbsUp = 0;
		QString dateModified;
		QStringList categories;
		/* Minecraft versions of its latest files, newest first. */
		QStringList gameVersions;
		/* Screenshot URLs, as the gallery shows them. */
		QStringList screenshots;
		/* The file CurseForge features: featured in the version box. */
		int mainFileId = 0;
	};

	explicit FlameModpackModel(QObject* parent = nullptr);
	~FlameModpackModel() override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index,
				 int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

	/* See ModrinthModpackModel: QML-facing overloads sharing a name with
	 * QAbstractItemModel virtuals of another arity. */
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
		return int(m_packs.size());
	}
	QString error() const
	{
		return m_error;
	}
	QObject* detail() const;
	QString providerName() const;
	QString bodyFormat() const
	{
		return QStringLiteral("html");
	}
	bool keyMissing() const;

	/// Starts a fresh search; cancels one already in flight.
	Q_INVOKABLE void search();
	/// The next page of the current search, when there is one.
	Q_INVOKABLE void fetchMore();
	/// Fetches the file list and long description of one pack into
	/// `detail`; a newer call drops a stale reply.
	Q_INVOKABLE void loadDetail(const QString& projectId);
	/* The InstanceImportTask FlamePage::suggestCurrent() builds for a
	 * pack picked in the widget browser, started, with a TaskWatcher for
	 * it (parented to this model). `versionId` must be one of the ids in
	 * `detail.versions` for `projectId`; nullptr otherwise. Files the
	 * author keeps off third-party downloads are asked about through the
	 * UiHost (its blockedMods question) as the import runs. */
	Q_INVOKABLE QObject* install(const QString& projectId,
								 const QString& versionId,
								 const QString& instanceName,
								 const QString& group);

	/* The search URL: FlameApi::modpackSearchUrl() plus the game version
	 * and loader the Discover page filters by. Static, so it can be
	 * tested without the network. */
	static QUrl searchUrl(const QString& term, int sortIndex, int offset,
						  const QString& gameVersion, const QString& loader);
	/* JSON -> rows; `totalCount` gets CurseForge's pagination total, or
	 * -1 when the reply does not say. Static, for tests. */
	static QList<Pack> parseSearchResults(const QByteArray& bytes,
										  int& totalCount);
	/* JSON -> the `versions` of the detail, newest first, in the shape
	 * ModrinthModpackModel::parseVersionsJson() gives (id, name,
	 * versionNumber, gameVersions, loaders, datePublished, downloadUrl,
	 * featured). A file with no API download URL gets its browser
	 * download address instead, as FlamePage did. Static, for tests. */
	static QVariantList parseVersionsJson(const QByteArray& bytes,
										  const QString& projectId,
										  int mainFileId);
	/* The description endpoint's reply -> its HTML, made safe to show
	 * as rich text (sanitizeBody()). */
	static QString parseBody(const QByteArray& bytes);
	/* @p html without what a rich-text view cannot show and should not
	 * fetch: embedded players and frames, scripts and styles, and links
	 * left with nothing in them. Pictures stay -- CurseForge descriptions
	 * are mostly pictures -- and RichBody lays them out; a video embed
	 * would only be fetched as a broken image. */
	static QString sanitizeBody(const QString& html);

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
	void fetchDetailVersions(const QString& projectId, int mainFileId,
							 quint64 generation);
	void markDetailPartDone(quint64 generation, bool isBody);
	const Pack* packFor(const QString& projectId) const;
	QString resolveIconKey(const Flame::IndexedPack& pack) const;

  private:
	QList<Pack> m_packs;

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
