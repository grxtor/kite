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

#include "modplatform/modpacksch/FTBPackManifest.h"

class ModrinthModpackDetail;

/* Feed The Beast (modpacks.ch) modpacks for QML. Same shape as the other
 * modpack models.
 *
 * modpacks.ch has no real search endpoint: this fetches every public pack
 * id, then the packs in batches so rows appear as they arrive, and
 * searches and sorts locally. `searching` is true while the catalogue
 * loads. A pack's detail is already fetched, so loadDetail() makes no
 * request. install() starts ModpacksCH::PackInstallTask. */
class FtbModpackModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
	Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY sortChanged)
	Q_PROPERTY(QVariantList sortOptions READ sortOptions CONSTANT)
	/* Kept for the shared bindings; modpacks.ch lists no Minecraft version
	 * per pack, so it does not filter. */
	Q_PROPERTY(QString gameVersion READ gameVersion WRITE setGameVersion
				   NOTIFY gameVersionChanged)
	/* "" = any loader; otherwise matched against the pack's tags. */
	Q_PROPERTY(
		QString loader READ loader WRITE setLoader NOTIFY loaderChanged)
	Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
	Q_PROPERTY(
		bool canFetchMore READ canFetchMore NOTIFY canFetchMoreChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	Q_PROPERTY(QObject* detail READ detail CONSTANT)
	Q_PROPERTY(QString providerName READ providerName CONSTANT)
	/// "markdown": a pack's description is Markdown.
	Q_PROPERTY(QString bodyFormat READ bodyFormat CONSTANT)
	/// Always false; modpacks.ch needs no key.
	Q_PROPERTY(bool keyMissing READ keyMissing CONSTANT)

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

	explicit FtbModpackModel(QObject* parent = nullptr);
	~FtbModpackModel() override;

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
		return m_loading;
	}
	bool canFetchMore() const
	{
		return false;
	}
	int count() const
	{
		return int(m_rows.size());
	}
	QString error() const
	{
		return m_error;
	}
	QObject* detail() const;
	QString providerName() const
	{
		return QStringLiteral("FTB");
	}
	QString bodyFormat() const
	{
		return QStringLiteral("markdown");
	}
	bool keyMissing() const
	{
		return false;
	}

	/// Loads the catalogue the first time; filters and sorts every time.
	Q_INVOKABLE void search();
	Q_INVOKABLE void fetchMore() {}
	Q_INVOKABLE void loadDetail(const QString& projectId);
	/* Starts ModpacksCH::PackInstallTask for version `versionId` (in
	 * `detail.versions`) of pack `projectId` and returns a TaskWatcher for it
	 * (parented to this model); nullptr when either names nothing loaded. */
	Q_INVOKABLE QObject* install(const QString& projectId,
								 const QString& versionId,
								 const QString& instanceName,
								 const QString& group);

	static QList<int> parseIndex(const QByteArray& bytes);
	/* One pack's reply -> `out`; false when it does not parse or has no
	 * version. Static, for tests. */
	static bool parsePack(const QByteArray& bytes, ModpacksCH::Modpack& out);
	/* Positions in @p packs matching @p query and @p loader, in @p sort order
	 * ("plays", "installs", "name", "updated"). Static, for tests. */
	static QList<int> filterAndSort(const QList<ModpacksCH::Modpack>& packs,
									const QString& query, const QString& loader,
									const QString& sort);
	static QVariantList versionEntries(const ModpacksCH::Modpack& pack);
	static QString artUrl(const ModpacksCH::Modpack& pack, const QString& type);

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
	void setLoading(bool loading);
	void setError(const QString& error);
	void startLoading();
	void loadNextBatch();
	void refilter();
	const ModpacksCH::Modpack* packFor(const QString& projectId) const;

  private:
	QList<ModpacksCH::Modpack> m_packs;
	QList<int> m_rows;
	QList<int> m_pending;

	QString m_query;
	QString m_sort = QStringLiteral("plays");
	QVariantList m_sortOptions;
	QString m_gameVersion;
	QString m_loader;
	bool m_loading = false;
	bool m_loaded = false;
	QString m_error;
	NetJob::Ptr m_job;

	ModrinthModpackDetail* m_detail = nullptr;
};
