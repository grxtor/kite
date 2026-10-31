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
 * QML-facing CurseForge/Modrinth search and install for one instance. Keeps
 * one ContentProviderModel (and its search state) per provider/content-type
 * pair visited. Created lazily by InstanceDetails and parented to it, so only
 * search()/loadVersions()/review() touch the network.
 */
class ContentBrowser : public QObject
{
	Q_OBJECT

	/// "modrinth" | "curseforge". Defaults to "modrinth", which needs no API key.
	Q_PROPERTY(QString provider READ provider WRITE setProvider NOTIFY
				   providerChanged)
	/// True when no CurseForge API key is compiled in; search() refuses to run against CurseForge then.
	Q_PROPERTY(bool curseForgeKeyMissing READ curseForgeKeyMissing CONSTANT)
	/// "mods" (default) | "resourcepacks" | "shaderpacks" | "datapacks".
	Q_PROPERTY(QString contentType READ contentType WRITE setContentType
				   NOTIFY contentTypeChanged)
	Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
	/// Index into sortOptions() for the current provider; resets to 0 when the provider changes.
	Q_PROPERTY(
		int sortIndex READ sortIndex WRITE setSortIndex NOTIFY sortIndexChanged)
	/// {id: int, label: string} per sort. NOTIFY, not CONSTANT: the sort lists differ by provider.
	Q_PROPERTY(
		QVariantList sortOptions READ sortOptions NOTIFY sortOptionsChanged)
	Q_PROPERTY(bool searching READ searching NOTIFY searchingChanged)
	Q_PROPERTY(
		bool canFetchMore READ canFetchMore NOTIFY canFetchMoreChanged)
	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(QString error READ error NOTIFY errorChanged)
	/// The ContentProviderModel for the current provider/contentType pair; each pair keeps its own results.
	Q_PROPERTY(QObject* results READ results NOTIFY resultsChanged)
	/// {id, name, versionNumber, gameVersions, loaders, datePublished, isCompatible},
	/// newest first, for the row loadVersions() last saw.
	Q_PROPERTY(QVariantList versions READ versions NOTIFY versionsChanged)
	Q_PROPERTY(bool versionsLoading READ versionsLoading NOTIFY
				   versionsLoadingChanged)

	/* Search filters. A change re-runs the search (hideInstalled only re-filters).
	 * Categories are provider- and type-specific, so they clear when either changes. */

	/// {id, label}; empty until loadFilterOptions() has an answer.
	Q_PROPERTY(QVariantList categories READ categories NOTIFY categoriesChanged)
	Q_PROPERTY(QStringList categoryFilter READ categoryFilter WRITE
				   setCategoryFilter NOTIFY filtersChanged)
	/// One Minecraft version, or "" for every version.
	Q_PROPERTY(QString gameVersionFilter READ gameVersionFilter WRITE
				   setGameVersionFilter NOTIFY filtersChanged)
	/// Release versions, newest first; includes the instance's own.
	Q_PROPERTY(QStringList gameVersionOptions READ gameVersionOptions NOTIFY
				   gameVersionOptionsChanged)
	Q_PROPERTY(QString instanceGameVersion READ instanceGameVersion NOTIFY
				   instanceProfileChanged)
	/// Loader names ("fabric", ...); empty means any. Only for loader-specific content.
	Q_PROPERTY(QStringList loaderFilter READ loaderFilter WRITE setLoaderFilter
				   NOTIFY filtersChanged)
	Q_PROPERTY(QVariantList loaderOptions READ loaderOptions NOTIFY
				   instanceProfileChanged)
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
	Q_PROPERTY(bool loaderFilterAvailable READ loaderFilterAvailable NOTIFY
				   contentTypeChanged)
	Q_PROPERTY(bool extendedFilters READ extendedFilters NOTIFY providerChanged)
	Q_PROPERTY(int activeFilterCount READ activeFilterCount NOTIFY
				   filtersChanged)
	/// False while the game runs and the content type is one it holds open (mods); installs are refused then.
	Q_PROPERTY(bool changesAllowed READ changesAllowed NOTIFY
				   changesAllowedChanged)
	/// False for data packs while the global data pack folder is off; review() returns null then.
	Q_PROPERTY(bool installTargetAvailable READ installTargetAvailable NOTIFY
				   installTargetAvailableChanged)

  public:
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

	Q_INVOKABLE void search();
	Q_INVOKABLE void fetchMore();
	Q_INVOKABLE void loadVersions(int row);
	/// Parented to this and already started. Null if @p row/@p versionId name
	/// nothing installable (call loadVersions() first) or the type has no folder.
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

	Q_INVOKABLE void loadFilterOptions();
	Q_INVOKABLE void resetFilters();

	/// The loader filter applies only to loader-specific content; environment and
	/// licence only where the provider indexes them (@p extended).
	static ModPlatform::SearchFilters
	searchFiltersFor(const FilterState& state, ModPlatform::ContentType type,
					 bool extended);
	/// "game-mechanics" -> "Game Mechanics".
	static QString categoryLabel(const QString& name);

	/// A version with no stated game versions or loaders passes that half of the check.
	static bool isVersionCompatible(const ModPlatform::ContentVersion& version,
									const QString& mcVersion,
									const QString& loader);
	/// Newest first, each flagged compatible with @p mcVersion / @p loader.
	static QVariantList
	versionEntries(const QList<ModPlatform::ContentVersion>& versions,
				   const QString& mcVersion, const QString& loader);
	static ModPlatform::SelectedMod
	selectionFrom(const ModPlatform::IndexedProject& project,
				  const ModPlatform::ContentVersion& version,
				  const QString& platform, const QString& mcVersion,
				  const QString& loader);
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
	void applyVersionsFromProject(const ModPlatform::IndexedProject* project);
	void applyFilters();
	void clearCategoryFilter();
	void refreshInstanceProfile();
	void rebuildGameVersionOptions();
	bool selectionFor(int row, const QString& versionId,
					  ModPlatform::SelectedMod& mod) const;

  private:
	MinecraftInstance* m_instance;
	/// Read once at construction; a later loader change is not followed.
	QString m_mcVersion;
	QString m_loaderType;

	QString m_provider = QStringLiteral("modrinth");
	ModPlatform::ContentType m_contentType = ModPlatform::ContentType::Mod;
	QString m_query;
	int m_sortIndex = 0;
	QString m_error;

	/// modelKey(provider, type) -> model; parented to this, kept so flipping providers keeps a search.
	QHash<QString, ContentProviderModel*> m_models;

	QVariantList m_versions;
	bool m_versionsLoading = false;
	/// The model and row `versions` answer for, so a late reply for another row is dropped.
	ContentProviderModel* m_versionsModel = nullptr;
	int m_versionsRow = -1;

	FilterState m_filter;
	bool m_hideInstalled = false;
	QHash<QString, QVariantList> m_categories;
	QStringList m_gameVersionOptions;
	std::shared_ptr<Meta::VersionList> m_mcVersionList;
};
