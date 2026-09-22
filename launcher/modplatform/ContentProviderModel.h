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
		/* "release" / "beta" / "alpha", empty when the provider does not
		 * say. Shown in brackets after the name, like the reference
		 * launcher does. */
		QString versionType;
		QString fileName;
		QString downloadUrl;
		QString sha1;
		int fileSize = 0;
		/* The address above is the provider's website rather than its
		 * API, because the author forbade third-party downloads. Carried
		 * so the review dialog can say so, and so the caller can offer
		 * to fetch the file by hand if the download is refused. */
		bool browserDownloadOnly = false;
	};

	/* One search result. Everything the list, the description pane and
	 * the version box need lives here; the two providers fill it in from
	 * their own JSON. */
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

		/* Comparison key derived from `name`, computed once when the
		 * result is parsed: the delegate asks a row for its state on
		 * every repaint, so this must not be recomputed on demand. */
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

/* Search results for one content provider.
 *
 * There used to be two of these, one per provider, ninety percent
 * identical - same roles, same pagination, same icon cache, same
 * installed/selected bookkeeping - and they drifted apart every time
 * one of them was touched. What genuinely differs between CurseForge
 * and Modrinth is the shape of the JSON they return, so that is all a
 * subclass has to supply: three parse functions and a cache name.
 *
 * Fetching versions and the long description belongs here rather than
 * in the dialog, because they are per-row state that has to survive the
 * user clicking around the list. */
class ContentProviderModel : public QAbstractListModel
{
	Q_OBJECT

  public:
	/* Continues past ProjectItemRole (Qt::UserRole+1..+3, defined in
	 * ProjectItemDelegate.h), which the QtWidgets delegate already reads
	 * from this model's data(). */
	enum ModelRoles { LogoKeyRole = Qt::UserRole + 4 };

	~ContentProviderModel() override;

	int rowCount(const QModelIndex& parent) const override;
	int columnCount(const QModelIndex& parent) const override;
	QVariant data(const QModelIndex& index, int role) const override;
	QHash<int, QByteArray> roleNames() const override;
	Qt::ItemFlags flags(const QModelIndex& index) const override;
	bool canFetchMore(const QModelIndex& parent) const override;
	void fetchMore(const QModelIndex& parent) override;

	/* "curseforge" / "modrinth" - matches the platform field stored in
	 * sidecars and in SelectedMod. */
	QString platformId() const;
	QString platformDisplayName() const;
	QList<ModPlatform::SortingMethod> sortingMethods() const;
	/* See ContentApi::supportsExtendedFilters(). */
	bool supportsExtendedFilters() const;

	/* Null when the row does not exist. `row` is a row as the view sees
	 * it, which is not the same as a position in the result list once
	 * installed projects are being hidden. The pointer is invalidated
	 * by the next search, so do not hold on to it. */
	const ModPlatform::IndexedProject* projectAt(int row) const;

	/* Starts a fresh search. Repeating the current query is ignored, so
	 * this is cheap to call when a page is re-opened. */
	void search(const QString& term, int sortIndex);

	/* Replaces the results with the single project of this id, fetched
	 * from the provider's project endpoint instead of its search.
	 *
	 * This is how the version of something already installed is changed:
	 * a search for the mod's name is not good enough, because two mods
	 * can share a name and the wrong one would be offered. Everything
	 * else - the generation counter, aborting a request that is still in
	 * flight, the inline progress bar - works exactly as it does for a
	 * search, which is why this goes through the same state machine
	 * rather than fetching on the side. */
	void searchProject(const QString& projectId);
	/* Fetches the version list and the long description for one row, if
	 * they are not already in flight. Reports back with entryUpdated(). */
	void loadEntry(int row);

	/* Fetches this provider's category list, once. Answers with
	 * categoriesLoaded(); a failure is logged and nothing is emitted, so
	 * the panel simply has no category group. */
	void loadCategories();

	/* What the provider is asked to narrow the search to. Changing any
	 * of it runs the current query again: the answer comes from the
	 * provider, not from what is already loaded. */
	void setSearchFilters(const ModPlatform::SearchFilters& filters);

	/* Leave installed projects out of the rows. Needs no round trip -
	 * the results are already here, they are just not shown. */
	void setHideInstalled(bool hide);

	/* Index of what is already installed in the target folder. Rows
	 * matching it are flagged so the view can dim them. */
	void setInstalledIndex(std::shared_ptr<ModMetadataIndex> index);
	/* Normalized names of the projects the user has queued for
	 * download. Matching rows draw a ticked checkbox. */
	void setSelectedNames(const QSet<QString>& names);

	bool isSearching() const
	{
		return m_searchJob != nullptr;
	}
	/* The running search, or null. Lets a page hang a progress widget
	 * off the job without taking ownership of it. */
	Task* activeSearchJob() const
	{
		return m_searchJob.get();
	}

  signals:
	/* A search started, finished or failed - drives the inline progress
	 * bar on the page. */
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

	/* Turns a single-project reply into one result, as used by
	 * searchProject(). An empty projectId in the returned value means
	 * the reply held nothing usable.
	 *
	 * Both providers answer this endpoint with the same field names they
	 * use for a search hit - wrapped in "data" on CurseForge, bare on
	 * Modrinth - so each subclass parses it with the very code that
	 * handles one hit of a search. */
	virtual ModPlatform::IndexedProject
	parseProjectResponse(const QByteArray& bytes) const = 0;

	/* Turns one version-list reply into versions, newest first. */
	virtual QList<ModPlatform::ContentVersion>
	parseVersionsResponse(const QByteArray& bytes,
						  const ModPlatform::IndexedProject& project) const = 0;

	/* Turns one description reply into HTML, converting from Markdown
	 * where the provider serves Markdown. */
	virtual QString parseBodyResponse(const QByteArray& bytes) const = 0;

	/* Turns the category reply into entries for the filter panel, in the
	 * order they should be offered. */
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
	/* Throws the current results away and asks again from offset zero.
	 * Every caller that changes what is being asked for ends here, so
	 * that the generation counter and the paging state cannot be reset
	 * in three slightly different ways. */
	void restartSearch();
	void performPaginatedSearch();
	void searchRequestFinished();
	void searchRequestFailed();

	/* Land a late version / description reply on the right row, if that
	 * row still exists and still belongs to the same search. */
	void applyVersions(quint64 generation, const QString& projectId,
					   const QByteArray& bytes);
	void applyBody(quint64 generation, const QString& projectId,
				   const QByteArray& bytes);

	void requestLogo(const QString& key, const QString& url);
	void logoLoaded(const QString& key, const QIcon& icon);
	void logoFailed(const QString& key);

	void applyInstalledState(ModPlatform::IndexedProject& project) const;
	void emitRowStateChanged();

	/* Position in m_projects of the project with this id, or -1. Used to
	 * place a late reply, since results may have been appended - or
	 * hidden - in the meantime. */
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

	/* Guards against a second fetch: the panel is built once, but a
	 * failed request must not turn into a retry on every keystroke. */
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

	/* Bumped on every reset. Replies tagged with an older generation
	 * belong to a search whose results are already gone. */
	quint64 m_generation = 0;
};
