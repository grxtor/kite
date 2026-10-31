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
#include <QStringList>
#include <QVariant>

#include <net/NetJob.h>

#include "modplatform/flame/FlamePackIndex.h"

class ModrinthModpackDetail;

/* CurseForge modpack search results for QML. Same shape as
 * ModrinthModpackModel, so the Discover page works on either. `detail` is
 * a ModrinthModpackDetail whose `body` here is HTML (see `bodyFormat`).
 *
 * Without an API key (BuildConfig.CURSEFORGE_API_KEY) every request is
 * refused: search() says so in `error` and `keyMissing` lets the page
 * explain it. Logos are CDN URLs for QML's Image; install() names the new
 * instance's icon from the disk cache if present, else the built-in one. */
class FlameModpackModel : public QAbstractListModel
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
	Q_PROPERTY(QString providerName READ providerName CONSTANT)
	Q_PROPERTY(QString bodyFormat READ bodyFormat CONSTANT)
	/// This build has no CurseForge API key, so nothing can be searched.
	Q_PROPERTY(bool keyMissing READ keyMissing CONSTANT)

  public:
	/* Same role names as ModrinthModpackModel, so one delegate reads both. */
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

	/* One search hit: what Flame::loadIndexedPack() reads, plus what a card
	 * shows. */
	struct Pack {
		Flame::IndexedPack base;
		QString slug;
		qint64 downloads = 0;
		int thumbsUp = 0;
		QString dateModified;
		QStringList categories;
		QStringList gameVersions;
		QStringList screenshots;
		/* The file CurseForge features; featured in the version box. */
		int mainFileId = 0;
	};

	explicit FlameModpackModel(QObject* parent = nullptr);
	~FlameModpackModel() override;

	int rowCount(const QModelIndex& parent = QModelIndex()) const override;
	QVariant data(const QModelIndex& index,
				 int role = Qt::DisplayRole) const override;
	QHash<int, QByteArray> roleNames() const override;

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

	Q_INVOKABLE void search();
	Q_INVOKABLE void fetchMore();
	/// Fetches the file list and long description into `detail`; a stale
	/// reply is dropped.
	Q_INVOKABLE void loadDetail(const QString& projectId);
	/* Builds and starts the InstanceImportTask for a pack and returns a
	 * TaskWatcher for it (parented to this model). `versionId` must be in
	 * `detail.versions` for `projectId`; nullptr otherwise. Files blocked
	 * from third-party downloads are asked about through the UiHost. */
	Q_INVOKABLE QObject* install(const QString& projectId,
								 const QString& versionId,
								 const QString& instanceName,
								 const QString& group);

	/* FlameApi::modpackSearchUrl() plus the Discover page's game version and
	 * loader. Static, for tests. */
	static QUrl searchUrl(const QString& term, int sortIndex, int offset,
						  const QString& gameVersion, const QString& loader);
	/* JSON -> rows; `totalCount` gets the pagination total, or -1. Static,
	 * for tests. */
	static QList<Pack> parseSearchResults(const QByteArray& bytes,
										  int& totalCount);
	/* JSON -> the detail's `versions`, newest first, in the shape of
	 * ModrinthModpackModel::parseVersionsJson(). A file with no API download
	 * URL gets its browser download address. Static, for tests. */
	static QVariantList parseVersionsJson(const QByteArray& bytes,
										  const QString& projectId,
										  int mainFileId);
	/* The description reply -> HTML safe for rich text (sanitizeBody()). */
	static QString parseBody(const QByteArray& bytes);
	/* @p html without what a rich-text view cannot show or should not fetch:
	 * embedded players and frames, scripts, styles and empty links. Pictures
	 * stay. */
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
