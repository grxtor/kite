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
#include <QUrl>
#include <QVariant>

#include <net/NetJob.h>

class ModrinthModpackDetail;

/* Technic modpacks for QML. Same shape as the other modpack models.
 * Technic searches on its own end (trending when nothing is typed) and
 * sends one page; sorts, loaders and versions exist only for the shared
 * bindings. A pack's download comes from its metadata on loadDetail():
 * a Solder server (SolderPackInstallTask installs its recommended build)
 * or a single zip (SingleZipPackInstallTask). The detail has one version. */
class TechnicModpackModel : public QAbstractListModel
{
	Q_OBJECT
	Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
	Q_PROPERTY(QString sort READ sort WRITE setSort NOTIFY sortChanged)
	Q_PROPERTY(QVariantList sortOptions READ sortOptions CONSTANT)
	Q_PROPERTY(QString gameVersion READ gameVersion WRITE setGameVersion
				   NOTIFY gameVersionChanged)
	Q_PROPERTY(
		QString loader READ loader WRITE setLoader NOTIFY loaderChanged)
	Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
	Q_PROPERTY(
		bool canFetchMore READ canFetchMore NOTIFY canFetchMoreChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	Q_PROPERTY(QObject* detail READ detail CONSTANT)
	Q_PROPERTY(QString providerName READ providerName CONSTANT)
	/// "html": descriptions are HTML (or plain text).
	Q_PROPERTY(QString bodyFormat READ bodyFormat CONSTANT)
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

	struct Pack {
		QString slug;
		QString name;
		QString logoUrl;
		QString logoName;
		bool metadataLoaded = false;
		QString url;
		bool isSolder = false;
		QString minecraftVersion;
		QString version;
		QString websiteUrl;
		QString author;
		QString description;
		QString backgroundUrl;
		qint64 downloads = 0;
		qint64 runs = 0;
	};

	explicit TechnicModpackModel(QObject* parent = nullptr);
	~TechnicModpackModel() override;

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
		return QStringLiteral("relevance");
	}
	void setSort(const QString&) {}
	QVariantList sortOptions() const;
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
		return false;
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
	QString providerName() const
	{
		return QStringLiteral("Technic");
	}
	QString bodyFormat() const
	{
		return QStringLiteral("html");
	}
	bool keyMissing() const
	{
		return false;
	}

	Q_INVOKABLE void search();
	Q_INVOKABLE void fetchMore() {}
	Q_INVOKABLE void loadDetail(const QString& projectId);
	/* Starts the Solder or single-zip install for pack `projectId` (its slug)
	 * once loadDetail() has its metadata; returns a TaskWatcher (parented to
	 * this model), or nullptr when that is missing. `versionId` is the one
	 * entry of `detail.versions`. */
	Q_INVOKABLE QObject* install(const QString& projectId,
								 const QString& versionId,
								 const QString& instanceName,
								 const QString& group);

	static QUrl searchUrl(const QString& term);
	static QUrl metadataUrl(const QString& slug);
	static QList<Pack> parseSearch(const QByteArray& bytes);
	/* Fills @p pack's metadata from its reply; false when it names neither a
	 * zip nor a Solder server. */
	static bool parseMetadata(const QByteArray& bytes, Pack& pack);
	static QString solderPackUrl(const Pack& pack);
	static QVariantList versionEntries(const Pack& pack);

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
	void setError(const QString& error);
	void setSearching(bool searching);
	void startSearch();
	Pack* packFor(const QString& slug);
	void applyDetail(const Pack& pack);

  private:
	QList<Pack> m_packs;
	QString m_query;
	QString m_gameVersion;
	QString m_loader;
	QString m_error;
	NetJob::Ptr m_searchJob;
	QByteArray m_searchResponse;
	bool m_restartPending = false;
	bool m_searching = false;

	ModrinthModpackDetail* m_detail = nullptr;
	quint64 m_detailGeneration = 0;
	NetJob::Ptr m_detailJob;
};
