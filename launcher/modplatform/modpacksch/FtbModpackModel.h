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

#include "modplatform/modpacksch/FTBPackManifest.h"

class ModrinthModpackDetail;

/* Feed The Beast (modpacks.ch) modpacks for QML - the widget-free
 * replacement for ui/pages/modplatform/ftb/FtbListModel.h +
 * FtbFilterModel.h, which back FtbPage.
 *
 * The same shape as ModrinthModpackModel and FlameModpackModel (the same
 * properties, invokables and role names), so the Discover page shows any
 * of them through one set of bindings.
 *
 * modpacks.ch has no search endpoint worth the name: like the widget
 * page, this asks once for every public pack id and then for each pack,
 * a batch at a time so rows appear as they arrive; searching and sorting
 * then happen here, over what is loaded (by name, summary and tags; by
 * plays, installs, name or last update). `searching` is true while the
 * catalogue is still coming in. A pack's detail is part of what was
 * already fetched, so loadDetail() needs no request of its own.
 *
 * install() starts ModpacksCH::PackInstallTask, which fetches the chosen
 * version's manifest and files itself.
 */
class FtbModpackModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
	Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY sortChanged)
	Q_PROPERTY(QVariantList sortOptions READ sortOptions CONSTANT)
	/* Kept for the shared bindings; modpacks.ch lists no Minecraft
	 * version per pack without a request per version, so it is not used
	 * to filter. */
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
	/// "markdown": a pack's description is Markdown, as FtbPage read it.
	Q_PROPERTY(QString bodyFormat READ bodyFormat CONSTANT)
	/// Always false; modpacks.ch needs no key. Here for the shared bindings.
	Q_PROPERTY(bool keyMissing READ keyMissing CONSTANT)

  public:
	/* The same role names as ModrinthModpackModel. */
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

	/// Loads the catalogue the first time; filters and sorts what is
	/// loaded every time.
	Q_INVOKABLE void search();
	/// Nothing to page: the whole catalogue loads by itself.
	Q_INVOKABLE void fetchMore() {}
	/// Fills `detail` from the loaded pack; no request.
	Q_INVOKABLE void loadDetail(const QString& projectId);
	/* Starts ModpacksCH::PackInstallTask for version `versionId` (one of
	 * the ids in `detail.versions`) of pack `projectId`, and returns a
	 * TaskWatcher for it (parented to this model); nullptr when either
	 * names nothing loaded. */
	Q_INVOKABLE QObject* install(const QString& projectId,
								 const QString& versionId,
								 const QString& instanceName,
								 const QString& group);

	/* The catalogue index reply -> pack ids. Static, for tests. */
	static QList<int> parseIndex(const QByteArray& bytes);
	/* One pack's reply -> `out`; false when it does not parse or has no
	 * version (the widget list skipped those too). Static, for tests. */
	static bool parsePack(const QByteArray& bytes, ModpacksCH::Modpack& out);
	/* Positions in @p packs that match @p query and @p loader, in @p sort
	 * order ("plays", "installs", "name", "updated"). Static, for tests. */
	static QList<int> filterAndSort(const QList<ModpacksCH::Modpack>& packs,
									const QString& query, const QString& loader,
									const QString& sort);
	/* The `versions` of the detail, newest first, in the shared shape. */
	static QVariantList versionEntries(const ModpacksCH::Modpack& pack);
	/* The art of @p type ("square", "splash", ...), or "". */
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
	QString resolveIconKey(const ModpacksCH::Modpack& pack) const;

  private:
	/// Every pack loaded so far, in arrival order.
	QList<ModpacksCH::Modpack> m_packs;
	/// What the view shows: positions in m_packs.
	QList<int> m_rows;
	/// Pack ids still to fetch.
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
