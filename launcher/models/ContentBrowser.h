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
} // namespace ModPlatform

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
 * install() collapses the widget's three-dialog "resolve dependencies ->
 * review -> confirm conflicts" pipeline into one automatic decision, since
 * there is no dialog here to ask through: required dependencies are always
 * installed, a dependency whose project is already present at some other
 * version is left alone (the same default the review dialog's tick boxes
 * start at), and a name/file-name conflict against what is already on disk
 * is resolved exactly as the conflict analyzer already decides for the
 * widget (replace in place). See ContentBrowserInstallTask in the .cpp.
 *
 * Created lazily by InstanceDetails::contentBrowser() and parented to it
 * (see that class), so opening an instance page never talks to the network
 * on its own - only search()/loadVersions()/install() do.
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

  public:
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
	/// Resolves required dependencies, settles conflicts against what is
	/// already installed and downloads everything, exactly the way the
	/// widget dialog does once its own dialogs are out of the way. Returns
	/// a TaskWatcher (parented to this browser - InstanceDetails/QmlShell
	/// pin it for QML the same way they do every other child object), or
	/// null if `row` or `versionId` do not name anything installable
	/// (call loadVersions() first).
	Q_INVOKABLE QObject* install(int row, const QString& versionId);

	/// Whether `version` may run on `mcVersion`/`loader`. A version with no
	/// stated game versions or no stated loaders is accepted for that half
	/// of the check - plenty of provider replies simply do not say, and
	/// refusing them would flag entries the provider's own search filter
	/// already let through as "incompatible" for no good reason. Public and
	/// static so it can be unit tested without a live search.
	static bool isVersionCompatible(const ModPlatform::ContentVersion& version,
									const QString& mcVersion,
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
};
