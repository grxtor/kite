/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
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
#include <QIcon>
#include <QList>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <memory>

#include <net/NetJob.h>

#include "modplatform/ContentApi.h"
#include "modplatform/ContentType.h"

class ModMetadataIndex;

namespace ModPlatform
{

	/* One downloadable file of a project, in provider-neutral terms. */
	struct ContentVersion {
		/* Platform version id (Modrinth) or file id (CurseForge). */
		QString versionId;
		/* Human-readable version, without decorations. */
		QString name;
		/* The bare version number, e.g. "1.2.3"; empty when the provider has
		 * none (CurseForge). `name` may carry it in parentheses (Modrinth). */
		QString versionNumber;
		/* "release" / "beta" / "alpha"; empty when the provider does not say. */
		QString versionType;
		QString fileName;
		QString downloadUrl;
		QString sha1;
		int fileSize = 0;
		/* The URL is the provider's website, not its API: the author forbids
		 * third-party downloads. */
		bool browserDownloadOnly = false;

		/* Minecraft versions and loaders this version declares. CurseForge
		 * mixes both in gameVersions (see ModPlatform::isKnownLoaderName());
		 * Modrinth has separate fields. Provider filtering is not fully trusted,
		 * so the picker checks again client-side. */
		QStringList gameVersions;
		QStringList loaders;
		/* RFC 3339 publish date, exactly as the provider states it; empty
		 * when unknown. */
		QString datePublished;
	};

	/* One search result, filled in from either provider's JSON. */
	struct IndexedProject {
		QString projectId;
		QString slug;
		QString name;
		QString description;
		QString author;
		QString websiteUrl;

		/* Key under which the icon is cached, and where to fetch it. */
		QString logoKey;
		QString logoUrl;

		/* Derived from `name` once at parse time: data() reads it on every
		 * repaint. */
		QString normalizedName;
		/* Already present in the folder this search targets. */
		bool installed = false;

		/* Filled in lazily, when the row is selected. */
		QList<ContentVersion> versions;
		bool versionsLoaded = false;
		bool versionsLoading = false;

		/* Long description, already converted to HTML by the provider. */
		QString bodyHtml;
		bool bodyLoaded = false;
		bool bodyLoading = false;
	};

} // namespace ModPlatform

/* Extra roles data() produces for ProjectItemDelegate, past Qt::UserRole
 * (the project id). DisplayRole stays the plain name. */
namespace ProjectItemRole
{
	enum Role {
		/* QString: project name. */
		Title = Qt::UserRole + 1,
		/* QString: short summary. */
		Description,
		/* bool: already in the target folder; the row is dimmed and tagged. */
		Installed,
	};
}

/* Search results for one content provider. A subclass supplies the
 * parse functions and a cache name. Versions and the long description
 * live here, as per-row state that survives clicking around the list. */
class ContentProviderModel : public QAbstractListModel
{
	Q_OBJECT

  public:
	/* Continues past ProjectItemRole. */
	enum ModelRoles {
		LogoKeyRole = Qt::UserRole + 4,
		// For QML delegates, which load the logo themselves.
		LogoUrlRole,
		AuthorRole
	};

	~ContentProviderModel() override;

	int rowCount(const QModelIndex& parent) const override;
	int columnCount(const QModelIndex& parent) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	bool canFetchMore(const QModelIndex& parent) const override;
	void fetchMore(const QModelIndex& parent) override;

	/* "curseforge" / "modrinth", as stored in sidecars and SelectedMod. */
	QString platformId() const;
	QString platformDisplayName() const;
	QList<ModPlatform::SortingMethod> sortingMethods() const;
	bool supportsExtendedFilters() const;

	/* Null when the row does not exist. `row` is a view row, not a position
	 * in the result list (installed projects may be hidden). Invalidated by
	 * the next search. */
	const ModPlatform::IndexedProject* projectAt(int row) const;

	/* Starts a fresh search; repeating the current query is ignored. */
	void search(const QString& term, int sortIndex);

	/* Replaces the results with the one project of this id, from the
	 * project endpoint: a name search could offer a same-named mod. Runs
	 * through the same state machine as search(). */
	void searchProject(const QString& projectId);
	/* Fetches versions and long description of a row; reports with
	 * entryUpdated(). */
	void loadEntry(int row);

	/* Fetches the category list once; answers with categoriesLoaded(). A
	 * failure is only logged. */
	void loadCategories();

	/* Provider-side filters; changing them reruns the current query. */
	void setSearchFilters(const ModPlatform::SearchFilters& filters);

	/* Leaves installed projects out of the rows; no round trip. */
	void setHideInstalled(bool hide);

	/* Index of what is installed; matching rows are flagged. */
	void setInstalledIndex(std::shared_ptr<ModMetadataIndex> index);
	/* Normalized names of queued projects; matching rows draw a ticked
	 * checkbox. */
	void setSelectedNames(const QSet<QString>& names);

	bool isSearching() const
	{
		return m_searchJob != nullptr;
	}
	/* The running search, or null; not owned by the caller. */
	Task* activeSearchJob() const
	{
		return m_searchJob.get();
	}

	/* Why the last search or page fetch failed; empty on success or before
	 * any search. */
	QString lastError() const
	{
		return m_lastError;
	}

  signals:
	/* A search started, finished or failed. */
	void searchStateChanged();
	/* Versions and/or the description of `row` arrived. */
	void entryUpdated(int row);
	/* The provider's category list arrived. Emitted at most once. */
	void categoriesLoaded(const QList<ModPlatform::Category>& categories);

  protected:
	ContentProviderModel(const ModPlatform::ContentApi& api,
						 ModPlatform::ContentType contentType,
						 ModPlatform::SearchFilters filters, QObject* parent);

	/* Turns one search reply into rows. `totalHits` should be set to the
	 * total the provider reports, or to -1 when it does not say. */
	virtual QList<ModPlatform::IndexedProject>
	parseSearchResponse(const QByteArray& bytes, int& totalHits) const = 0;

	/* Turns a single-project reply into one result for searchProject(); an
	 * empty projectId means nothing usable. Parsed like one search hit:
	 * wrapped in "data" on CurseForge, bare on Modrinth. */
	virtual ModPlatform::IndexedProject
	parseProjectResponse(const QByteArray& bytes) const = 0;

	/* Turns one version-list reply into versions, newest first. */
	virtual QList<ModPlatform::ContentVersion>
	parseVersionsResponse(const QByteArray& bytes,
						  const ModPlatform::IndexedProject& project) const = 0;

	/* Turns one description reply into HTML, converting Markdown if needed. */
	virtual QString parseBodyResponse(const QByteArray& bytes) const = 0;

	/* Turns the category reply into filter entries, in display order. */
	virtual QList<ModPlatform::Category>
	parseCategoriesResponse(const QByteArray& bytes) const = 0;

	/* Metacache bucket for this provider's icons. */
	virtual QString iconCacheName() const = 0;

	const ModPlatform::ContentApi& api() const
	{
		return m_api;
	}
	ModPlatform::ContentType contentType() const
	{
		return m_contentType;
	}

  private:
	/* Throws the results away and asks again from offset zero; the one
	 * reset path for the generation counter and paging state. */
	void restartSearch();
	void performPaginatedSearch();
	void searchRequestFinished();
	void searchRequestFailed();

	/* Lands a late reply on its row, if it still exists in the same search. */
	void applyVersions(quint64 generation, const QString& projectId,
					   const QByteArray& bytes);
	void applyBody(quint64 generation, const QString& projectId,
				   const QByteArray& bytes);

	void requestLogo(const QString& key, const QString& url);
	void logoLoaded(const QString& key, const QIcon& icon);
	void logoFailed(const QString& key);

	void applyInstalledState(ModPlatform::IndexedProject& project) const;
	void emitRowStateChanged();

	/* Position in m_projects of this id, or -1; results may have been
	 * appended or hidden since the request. */
	int storageIndexOf(const QString& projectId) const;
	/* Row the view shows this result at, or -1 when it is filtered out. */
	int viewRowOfStorage(int storageIndex) const;
	bool passesRowFilter(const ModPlatform::IndexedProject& project) const;
	/* Recomputes the visible rows from scratch, as a model reset. */
	void rebuildVisibleRows();

  private:
	const ModPlatform::ContentApi& m_api;
	ModPlatform::ContentType m_contentType;
	ModPlatform::SearchFilters m_filters;

	/* Every result received, in the order the provider sent them. */
	QList<ModPlatform::IndexedProject> m_projects;
	/* Row -> position in m_projects. Equal to 0,1,2... unless installed
	 * projects are being hidden. */
	QList<int> m_visibleRows;
	bool m_hideInstalled = false;

	std::shared_ptr<ModMetadataIndex> m_installedIndex;
	QSet<QString> m_selectedNames;

	/* Stops a failed request from retrying on every keystroke. */
	bool m_categoriesRequested = false;

	QMap<QString, QIcon> m_logoMap;
	QStringList m_loadingLogos;
	QStringList m_failedLogos;

	QString m_searchTerm;
	/* Non-empty while the rows are the answer to searchProject() rather
	 * than to a search term. */
	QString m_projectLookupId;
	int m_sortIndex = 0;
	bool m_searched = false;
	int m_nextSearchOffset = 0;

	enum SearchState {
		None,
		CanPossiblyFetchMore,
		ResetRequested,
		Finished
	} m_searchState = None;

	NetJob::Ptr m_searchJob;
	QByteArray m_searchResponse;
	QString m_lastError;

	/* Bumped on every reset. Replies tagged with an older generation
	 * belong to a search whose results are already gone. */
	quint64 m_generation = 0;
};
