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

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>
#include <memory>

#include "modplatform/ContentType.h"

class MinecraftInstance;
class ModFolderModel;
class ContentProviderModel;

namespace ModPlatform
{
	struct ContentVersion;
	struct SortingMethod;
	struct IndexedProject;
	struct SearchFilters;
	struct SelectedMod;
} // namespace ModPlatform

namespace Meta
{
	class VersionList;
} // namespace Meta

/*
 * QML-facing bridge to CurseForge/Modrinth content search and install for
 * one instance - the widget-free replacement for DownloadContentDialog +
 * ModFolderPage::installSelection()/reviewAndInstall().
 *
 * Search itself is exactly what the widget dialog does: one
 * ContentProviderModel per provider, configured with this instance's
 * Minecraft version and loader the same way DownloadContentDialog::
 * detectInstanceProfile()/buildPages() does, one model kept (and its
 * search state preserved) per provider/content-type pair actually visited.
 *
 * Installing goes through review(): a ContentInstallReview resolves the
 * pick's required dependencies and lists them to untick (the widget's
 * DownloadSummaryDialog), then downloads what is left with conflicts
 * against what is on disk settled the way the widget settles them
 * (ModInstallConflictAnalyzer: replace in place).
 *
 * Created lazily by InstanceDetails::contentBrowser() and parented to it
 * (see that class), so opening an instance page never talks to the network
 * on its own - only search()/loadVersions()/review() do.
 */
class ContentBrowser : public QObject
{
	Q_OBJECT

	/// "modrinth" | "curseforge". Defaults to "modrinth" - the provider the
	/// widget dialog puts first, and the one that never needs an API key.
	Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY
				   providerChanged)
	/// Whether this build has no CurseForge API key compiled in
	/// (BuildConfig::CURSEFORGE_API_KEY) - every CurseForge request 403s
	/// without one. search() refuses to run against CurseForge while this
	/// is true rather than start a request that cannot succeed; QML can
	/// use it to grey the provider out ahead of time instead of waiting to
	/// be told.
	Q_PROPERTY(bool curseForgeKeyMissing READ curseForgeKeyMissing CONSTANT)
	/// "mods" (default) | "resourcepacks" | "shaderpacks" | "datapacks".
	Q_PROPERTY(QString contentType READ contentType WRITE setContentType
				   NOTIFY contentTypeChanged)
	Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
	/// Index into sortOptions() for the *current* provider - not a fixed
	/// meaning across both, since CurseForge and Modrinth do not offer the
	/// same sorts (see ModPlatform::ContentApi::sortingMethods()). Changing
	/// `provider` resets this back to 0 and emits sortOptionsChanged().
	Q_PROPERTY(
		int sortIndex READ sortIndex WRITE setSortIndex NOTIFY sortIndexChanged)
	/// QVariantList of {id: int, label: string}, for the current provider.
	/// Deliberately NOTIFY rather than CONSTANT: CurseForge offers eight
	/// sorts, Modrinth five, in different orders with different meanings,
	/// so a single fixed list would either misrepresent one of them or
	/// send the wrong sort - see the .cpp for the fuller reasoning.
	Q_PROPERTY(
		QVariantList sortOptions READ sortOptions NOTIFY sortOptionsChanged)
	Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
	Q_PROPERTY(
		bool canFetchMore READ canFetchMore NOTIFY canFetchMoreChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	/// Empty on success, or while nothing has searched yet.
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	/// The ContentProviderModel for the current provider/contentType pair -
	/// see ProjectItemRole (ContentProviderModel.h) plus "projectId" and
	/// "logoKey" for its roleNames(). Swaps to a different (already
	/// parented, so still safe once exposed to QML - see
	/// InstanceDetails::contentBrowser()) model instance when provider or
	/// contentType changes; each provider/contentType pair keeps its own
	/// model (and search results) for as long as this browser lives.
	Q_PROPERTY(QObject* results READ results NOTIFY resultsChanged)
	/// QVariantList of {id, name, versionNumber, gameVersions, loaders,
	/// datePublished, isCompatible}, newest first, for the row loadVersions()
	/// was last called with.
	Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
	Q_PROPERTY(bool versionsLoading READ versionsLoading NOTIFY
				   versionsLoadingChanged)

	/* The search filters ContentFilterWidget offers in the widget dialog,
	 * as properties. A change re-runs the current search (or, for
	 * hideInstalled, only re-filters what is already loaded - see
	 * ContentFilterWidget's two signals). They start on this instance's
	 * own Minecraft version and loader, the search the browser has always
	 * made; categories are provider- and type-specific, so they are
	 * cleared whenever either changes. */

	/// QVariantList of {id, label} for the current provider/contentType;
	/// empty until loadFilterOptions() has had an answer.
	Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)
	/// Category ids to narrow to; empty means every category.
	Q_PROPERTY(QStringList categoryFilter READ categoryFilter WRITE
				   setCategoryFilter NOTIFY filtersChanged)
	/// One Minecraft version, or "" for every version.
	Q_PROPERTY(QString gameVersionFilter READ gameVersionFilter WRITE
				   setGameVersionFilter NOTIFY filtersChanged)
	/// Release versions, newest first, with this instance's own version
	/// always among them; filled by loadFilterOptions().
	Q_PROPERTY(QStringList gameVersionOptions READ gameVersionOptions NOTIFY
				   gameVersionOptionsChanged)
	Q_PROPERTY(QString instanceGameVersion READ instanceGameVersion NOTIFY
				   instanceProfileChanged)
	/// Loader names to narrow to ("fabric", ...); empty means any loader.
	/// Only used for content that is loader-specific.
	Q_PROPERTY(QStringList loaderFilter READ loaderFilter WRITE setLoaderFilter
				   NOTIFY filtersChanged)
	/// QVariantList of {id, label}: the loaders the widget panel offers
	/// up front, plus this instance's own if it is another one.
	Q_PROPERTY(QVariantList loaderOptions READ loaderOptions NOTIFY
				   instanceProfileChanged)
	/// This instance's primary mod loader ("" when it has none).
	Q_PROPERTY(QString instanceLoader READ instanceLoader NOTIFY
				   instanceProfileChanged)
	Q_PROPERTY(bool hasModLoader READ hasModLoader NOTIFY instanceProfileChanged)
	/// "" (any) | "client" | "server" | "both". Only Modrinth indexes it.
	Q_PROPERTY(QString environmentFilter READ environmentFilter WRITE
				   setEnvironmentFilter NOTIFY filtersChanged)
	/// Only Modrinth indexes licences.
	Q_PROPERTY(bool openSourceOnly READ openSourceOnly WRITE setOpenSourceOnly
				   NOTIFY filtersChanged)
	/// Leave out results already in this instance; needs no new search.
	Q_PROPERTY(bool hideInstalled READ hideInstalled WRITE setHideInstalled
				   NOTIFY filtersChanged)
	/// Whether the loader filter means anything for the current
	/// contentType (ModPlatform::contentTypeUsesLoader()).
	Q_PROPERTY(bool loaderFilterAvailable READ loaderFilterAvailable NOTIFY
				   contentTypeChanged)
	/// Whether the current provider can filter by environment and
	/// licence (ContentApi::supportsExtendedFilters()).
	Q_PROPERTY(bool extendedFilters READ extendedFilters NOTIFY providerChanged)
	/// How many filters differ from where they started, for a badge.
	Q_PROPERTY(int activeFilterCount READ activeFilterCount NOTIFY
				   filtersChanged)
	/// False while the game runs and the current contentType is one it
	/// holds open (mods) - InstanceContentController::changesAllowed()'s
	/// rule. Installing is refused then, the way the widget page refuses
	/// its Download action.
	Q_PROPERTY(bool changesAllowed READ changesAllowed NOTIFY
				   changesAllowedChanged)
	/// Whether the current contentType has a folder to install into.
	/// False for data packs while the instance-wide data pack folder is
	/// switched off (GlobalDataPacksEnabled): there is nowhere for them
	/// to go, so review() returns null.
	Q_PROPERTY(bool installTargetAvailable READ installTargetAvailable NOTIFY
				   installTargetAvailableChanged)

  public:
	/// Everything the filter properties above hold, in one value - what
	/// searchFiltersFor() turns into a provider query.
	struct FilterState {
		QStringList categories;
		QString gameVersion;
		QStringList loaders;
		QString environment;
		bool openSourceOnly = false;
	};
	explicit ContentBrowser(MinecraftInstance* instance,
							QObject* parent = nullptr);
	~ContentBrowser() override;

	QString provider() const
	{
		return m_provider;
	}
	void setProvider(const QString& provider);
	bool curseForgeKeyMissing() const;

	QString contentType() const;
	void setContentType(const QString& contentType);

	QString query() const
	{
		return m_query;
	}
	void setQuery(const QString& query);

	int sortIndex() const
	{
		return m_sortIndex;
	}
	void setSortIndex(int index);
	QVariantList sortOptions() const;

	bool searching() const;
	bool canFetchMore() const;
	int count() const;
	QString error() const
	{
		return m_error;
	}

	QObject* results() const;

	QVariantList versions() const
	{
		return m_versions;
	}
	bool versionsLoading() const
	{
		return m_versionsLoading;
	}

	/// Starts a fresh search from query()/sortIndex() against the current
	/// provider/contentType. Repeating the current query is cheap - see
	/// ContentProviderModel::search().
	Q_INVOKABLE void search();
	/// Fetches the next page of the current search; a no-op while already
	/// searching or when canFetchMore() is false.
	Q_INVOKABLE void fetchMore();
	/// Fetches (or re-shows, if already loaded) the compatible versions of
	/// results() row `row`, filling `versions`.
	Q_INVOKABLE void loadVersions(int row);
	/// Installing version `versionId` of results() row `row`: a
	/// ContentInstallReview (parented to this browser, already started)
	/// that resolves the dependencies first and lists them, so some can be
	/// unticked before its own install() downloads anything. Null if `row`
	/// or `versionId` do not name anything installable (call
	/// loadVersions() first), or there is no folder for this content type.
	Q_INVOKABLE QObject* review(int row, const QString& versionId);

	QVariantList categories() const;
	QStringList categoryFilter() const
	{
		return m_filter.categories;
	}
	void setCategoryFilter(const QStringList& ids);
	QString gameVersionFilter() const
	{
		return m_filter.gameVersion;
	}
	void setGameVersionFilter(const QString& version);
	QStringList gameVersionOptions() const
	{
		return m_gameVersionOptions;
	}
	QString instanceGameVersion() const
	{
		return m_mcVersion;
	}
	QStringList loaderFilter() const
	{
		return m_filter.loaders;
	}
	void setLoaderFilter(const QStringList& loaders);
	QVariantList loaderOptions() const;
	QString instanceLoader() const
	{
		return m_loaderType;
	}
	bool hasModLoader() const
	{
		return !m_loaderType.isEmpty();
	}
	QString environmentFilter() const
	{
		return m_filter.environment;
	}
	void setEnvironmentFilter(const QString& environment);
	bool openSourceOnly() const
	{
		return m_filter.openSourceOnly;
	}
	void setOpenSourceOnly(bool only);
	bool hideInstalled() const
	{
		return m_hideInstalled;
	}
	void setHideInstalled(bool hide);
	bool loaderFilterAvailable() const;
	bool extendedFilters() const;
	int activeFilterCount() const;
	bool changesAllowed() const;
	bool installTargetAvailable() const;

	/// Asks for what the filter panel offers - the current provider's
	/// categories and Minecraft's version list - once each; cheap to call
	/// every time the panel opens.
	Q_INVOKABLE void loadFilterOptions();
	/// Back to this instance's own version and loader, nothing else.
	Q_INVOKABLE void resetFilters();

	/// The provider query for @p state on @p type: the loader filter only
	/// for loader-specific content, environment and licence only where
	/// the provider indexes them (@p extended). Public and static so it
	/// can be unit tested without a live search.
	static ModPlatform::SearchFilters
	searchFiltersFor(const FilterState& state, ModPlatform::ContentType type,
					 bool extended);
	/// "game-mechanics" -> "Game Mechanics": providers hand category
	/// names out slugged.
	static QString categoryLabel(const QString& name);

	/// Whether `version` may run on `mcVersion`/`loader`. A version with no
	/// stated game versions or no stated loaders is accepted for that half
	/// of the check - plenty of provider replies simply do not say, and
	/// refusing them would flag entries the provider's own search filter
	/// already let through as "incompatible" for no good reason. Public and
	/// static so it can be unit tested without a live search.
	static bool isVersionCompatible(const ModPlatform::ContentVersion& version,
									const QString& mcVersion,
									const QString& loader);
	/// What `versions` holds for a project with @p versions: newest
	/// first, each flagged compatible with @p mcVersion / @p loader or
	/// not. Public and static so "Change version…"
	/// (ContentUpdateController) lists versions exactly as this does.
	static QVariantList
	versionEntries(const QList<ModPlatform::ContentVersion>& versions,
				   const QString& mcVersion, const QString& loader);
	/// What installing @p version of @p project from @p platform means,
	/// for a review or an install. Public and static for the same reason.
	static ModPlatform::SelectedMod
	selectionFrom(const ModPlatform::IndexedProject& project,
				  const ModPlatform::ContentVersion& version,
				  const QString& platform, const QString& mcVersion,
				  const QString& loader);
	/// ModPlatform::SortingMethod -> {id: <index>, label: <readableName>}.
	/// Public and static for the same reason as isVersionCompatible().
	static QVariantList
	sortOptionsFor(const QList<ModPlatform::SortingMethod>& methods);

  signals:
	void providerChanged();
	void contentTypeChanged();
	void queryChanged();
	void sortIndexChanged();
	void sortOptionsChanged();
	void searchingChanged();
	void canFetchMoreChanged();
	void countChanged();
	void errorChanged();
	void resultsChanged();
	void versionsChanged();
	void versionsLoadingChanged();
	void categoriesChanged();
	void filtersChanged();
	void gameVersionOptionsChanged();
	void instanceProfileChanged();
	void changesAllowedChanged();
	void installTargetAvailableChanged();

  private slots:
	void onModelSearchStateChanged();
	void onEntryUpdated(int row);

  private:
	QString modelKey(const QString& provider,
					 ModPlatform::ContentType type) const;
	ContentProviderModel* currentModel() const;
	ContentProviderModel* ensureModel(const QString& provider,
									  ModPlatform::ContentType type);
	void connectModel(ContentProviderModel* model);
	std::shared_ptr<ModFolderModel>
	folderModelFor(ModPlatform::ContentType type) const;
	void setError(const QString& error);
	/// Rebuilds `versions` from `project` (null clears it) and settles
	/// `versionsLoading` - shared by loadVersions() itself, for a project
	/// whose versions were already loaded, and by onEntryUpdated(), for one
	/// that just finished loading.
	void applyVersionsFromProject(const ModPlatform::IndexedProject* project);
	/// Hands the filter properties to the current model.
	void applyFilters();
	/// Clears the category filter (provider- and type-specific ids).
	void clearCategoryFilter();
	/// Re-reads the instance's Minecraft version and loader after its
	/// components change, moving filters still on the old defaults along.
	void refreshInstanceProfile();
	void rebuildGameVersionOptions();
	/// For review(): what row @p row's version
	/// @p versionId would install, or false if either names nothing.
	bool selectionFor(int row, const QString& versionId,
					  ModPlatform::SelectedMod& mod) const;

  private:
	MinecraftInstance* m_instance;
	/// This instance's Minecraft version / primary loader, read once at
	/// construction the same way DownloadContentDialog::
	/// detectInstanceProfile() does - an instance's loader can change while
	/// the page is open, but so can it while the widget dialog is open, and
	/// neither reacts to that either.
	QString m_mcVersion;
	QString m_loaderType;

	QString m_provider = QStringLiteral("modrinth");
	ModPlatform::ContentType m_contentType = ModPlatform::ContentType::Mod;
	QString m_query;
	int m_sortIndex = 0;
	QString m_error;

	/// modelKey(provider, type) -> model. Owned via QObject parentage
	/// (parent is `this`); kept for as long as this browser lives so
	/// flipping providers does not lose a search already in progress.
	QHash<QString, ContentProviderModel*> m_models;

	QVariantList m_versions;
	bool m_versionsLoading = false;
	/// The model and row `versions`/`versionsLoading` answer for, so a
	/// late reply for a row the user has since moved away from (or for a
	/// model that is no longer current) is not applied.
	ContentProviderModel* m_versionsModel = nullptr;
	int m_versionsRow = -1;

	FilterState m_filter;
	bool m_hideInstalled = false;
	/// modelKey(provider, type) -> {id, label} list, as each provider
	/// answers loadFilterOptions().
	QHash<QString, QVariantList> m_categories;
	QStringList m_gameVersionOptions;
	/// Held so a refreshed list can be read again when it arrives.
	std::shared_ptr<Meta::VersionList> m_mcVersionList;
};
