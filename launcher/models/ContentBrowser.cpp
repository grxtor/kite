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

#include "ContentBrowser.h"

#include <QDateTime>
#include <QDebug>
#include <QModelIndex>
#include <QVariantMap>
#include <algorithm>

#include "BuildConfig.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "modplatform/ContentDownloadTask.h"
#include "modplatform/DependencyResolver.h"
#include "modplatform/ModDownloadTypes.h"
#include "modplatform/ModInstallConflictAnalyzer.h"
#include "modplatform/flame/FlameContentModel.h"
#include "modplatform/modrinth/ModrinthContentModel.h"
#include "tasks/TaskWatcher.h"

namespace
{

	ModPlatform::ContentType typeFromString(const QString& s)
	{
		if (s == QStringLiteral("resourcepacks")) {
			return ModPlatform::ContentType::ResourcePack;
		}
		if (s == QStringLiteral("shaderpacks")) {
			return ModPlatform::ContentType::ShaderPack;
		}
		if (s == QStringLiteral("datapacks")) {
			return ModPlatform::ContentType::DataPack;
		}
		return ModPlatform::ContentType::Mod;
	}

	/* Converts a SelectedMod/DependencyInfo pair of fields into one
	 * DownloadItem - the same field-by-field copy DownloadSummaryDialog's
	 * constructor does for each of its two loops (ui/dialogs/
	 * DownloadSummaryDialog.cpp), pulled out here because this runs the
	 * same conversion with nobody left to tick a box in between. */
	ModPlatform::DownloadItem toDownloadItem(const ModPlatform::SelectedMod& mod)
	{
		ModPlatform::DownloadItem item;
		item.name = mod.name;
		item.fileName = mod.fileName;
		item.downloadUrl = mod.downloadUrl;
		item.sha1 = mod.sha1;
		item.fileSize = mod.fileSize;
		item.isDependency = false;
		item.platform = mod.platform;
		item.projectId = mod.projectId;
		item.versionId = mod.versionId;
		item.slug = mod.slug;
		item.browserDownloadOnly = mod.browserDownloadOnly;
		return item;
	}

	ModPlatform::DownloadItem
	toDownloadItem(const ModPlatform::DependencyInfo& dep)
	{
		ModPlatform::DownloadItem item;
		item.name = dep.name;
		item.fileName = dep.fileName;
		item.downloadUrl = dep.downloadUrl;
		item.sha1 = dep.sha1;
		item.fileSize = dep.fileSize;
		item.isDependency = true;
		item.platform = dep.platform;
		item.projectId = dep.projectId;
		item.versionId = dep.versionId;
		item.slug = dep.slug;
		item.browserDownloadOnly = dep.browserDownloadOnly;
		return item;
	}

} // namespace

/*
 * Runs the same three steps ModFolderPage::installSelection() +
 * reviewAndInstall() run for one mod picked in the widget dialog - resolve
 * dependencies, settle conflicts against what is on disk, download - minus
 * the two dialogs in between, since there is nobody here to answer them.
 *
 * The defaults those dialogs would otherwise ask about:
 *   - Required dependencies: always installed. DependencyResolver already
 *     only ever follows "required" relations (see processCFFileDeps()/
 *     processMRVersionDeps() - optional and embedded ones are either
 *     skipped outright or folded into the parent's own file), so
 *     everything resolvedDependencies() hands back already passed that
 *     bar.
 *   - A dependency whose project is already installed at some other
 *     version (DependencyInfo::maybeInstalled): left alone, exactly the
 *     unticked default DownloadSummaryDialog::appendRow() starts such a
 *     row at ("Unticked because a version of this is already installed").
 *   - Name/file-name conflicts against what is on disk
 *     (ModInstallConflictAnalyzer): resolved exactly as the widget
 *     resolves them once a row survives to the plan - AlreadyInstalled is
 *     dropped, everything else becomes a replace-in-place. Nothing here
 *     asks about it either; the widget's own summary dialog does not, so
 *     there is no missing confirmation step to replicate.
 *
 * A resolver failure or abort (network hiccup, nothing more) does not fail
 * the whole install - the widget's Skip button does not either; whatever
 * was resolved before it is used and the download proceeds.
 */
class ContentBrowserInstallTask : public Task
{
	Q_OBJECT

  public:
	ContentBrowserInstallTask(const ModPlatform::SelectedMod& mod,
							  const QString& mcVersion, const QString& loader,
							  const QString& targetDir,
							  std::shared_ptr<ModMetadataIndex> metadataIndex,
							  bool resolveDependencies,
							  QObject* parent = nullptr)
		: Task(parent), m_mod(mod), m_mcVersion(mcVersion), m_loader(loader),
		  m_targetDir(targetDir), m_metadataIndex(std::move(metadataIndex)),
		  m_resolveDependencies(resolveDependencies)
	{
	}

	bool canAbort() const override
	{
		if (m_download) {
			return m_download->canAbort();
		}
		if (m_resolver) {
			return m_resolver->canAbort();
		}
		return false;
	}

  public slots:
	bool abort() override
	{
		if (m_aborted) {
			return true;
		}
		m_aborted = true;

		/* Whichever child is currently doing work absorbs the abort
		 * request. Its succeeded()/failed() is routed through the
		 * m_aborted check in onResolved()/the download connections below
		 * rather than straight into our own emitSucceeded()/emitFailed(),
		 * so the abort below is what settles our own final state. */
		if (m_download && m_download->isRunning() && m_download->canAbort()) {
			m_download->abort();
		} else if (m_resolver && m_resolver->isRunning() &&
				  m_resolver->canAbort()) {
			m_resolver->abort();
		}

		if (isRunning()) {
			emitAborted();
		}
		return true;
	}

  protected:
	void executeTask() override
	{
		if (!m_resolveDependencies) {
			onResolved();
			return;
		}

		setStatus(tr("Resolving dependencies..."));
		m_resolver =
			new DependencyResolver({m_mod}, m_mcVersion, m_loader, this);
		m_resolver->setInstalledIndex(m_metadataIndex);
		connect(m_resolver, &Task::status, this, &Task::setStatus);
		connect(m_resolver, &Task::progress, this, &Task::setProgress);
		propagateStepsFrom(m_resolver);
		connect(m_resolver, &Task::succeeded, this,
				&ContentBrowserInstallTask::onResolved);
		connect(m_resolver, &Task::failed, this,
				[this](QString) { onResolved(); });
		m_resolver->start();
	}

  private slots:
	void onResolved()
	{
		if (m_aborted) {
			return;
		}

		QList<ModPlatform::DownloadItem> items;
		items.append(toDownloadItem(m_mod));

		if (m_resolver) {
			for (const auto& dep : m_resolver->resolvedDependencies()) {
				if (dep.maybeInstalled) {
					/* Matches the widget's own default (see the class
					 * comment): left alone rather than replaced. */
					continue;
				}
				items.append(toDownloadItem(dep));
			}
		}

		const auto decisions =
			ModInstallConflictAnalyzer::analyze(items, m_metadataIndex);
		const auto plan = ModInstallConflictAnalyzer::toDownloadPlan(decisions);

		if (plan.isEmpty()) {
			emitSucceeded();
			return;
		}

		setStatus(tr("Downloading %1 file(s)...").arg(plan.size()));
		m_download = new ContentDownloadTask(plan, m_targetDir, this);
		m_download->setMetadataIndex(m_metadataIndex);
		connect(m_download, &Task::status, this, &Task::setStatus);
		connect(m_download, &Task::progress, this, &Task::setProgress);
		propagateStepsFrom(m_download);
		connect(m_download, &Task::succeeded, this, [this] {
			if (!m_aborted) {
				emitSucceeded();
			}
		});
		connect(m_download, &Task::failed, this, [this](QString reason) {
			if (!m_aborted) {
				emitFailed(reason);
			}
		});
		m_download->start();
	}

  private:
	ModPlatform::SelectedMod m_mod;
	QString m_mcVersion;
	QString m_loader;
	QString m_targetDir;
	std::shared_ptr<ModMetadataIndex> m_metadataIndex;
	bool m_resolveDependencies;

	/* Owned via QObject parentage (parent is `this`), not shared_qobject_ptr:
	 * neither is ever swapped out from under itself the way ContentDownloadTask's
	 * own m_netJob is, so there is nothing for a Ptr's reset-then-replace to
	 * protect against here. */
	DependencyResolver* m_resolver = nullptr;
	ContentDownloadTask* m_download = nullptr;
	bool m_aborted = false;
};

ContentBrowser::ContentBrowser(MinecraftInstance* instance, QObject* parent)
	: QObject(parent), m_instance(instance)
{
	/* Same detection DownloadContentDialog::detectInstanceProfile() does. */
	if (auto profile = m_instance ? m_instance->getPackProfile() : nullptr) {
		m_mcVersion = profile->getComponentVersion("net.minecraft");
		m_loaderType = profile->primaryModLoader();
	}
}

ContentBrowser::~ContentBrowser() = default;

void ContentBrowser::setProvider(const QString& provider)
{
	const QString normalized = provider == QStringLiteral("curseforge")
									? provider
									: QStringLiteral("modrinth");
	if (m_provider == normalized) {
		return;
	}
	m_provider = normalized;
	emit providerChanged();
	emit resultsChanged();
	emit sortOptionsChanged();
	m_sortIndex = 0;
	emit sortIndexChanged();
	emit searchingChanged();
	emit canFetchMoreChanged();
	emit countChanged();
	setError(QString());
}

bool ContentBrowser::curseForgeKeyMissing() const
{
	return BuildConfig.CURSEFORGE_API_KEY.isEmpty();
}

QString ContentBrowser::contentType() const
{
	return ModPlatform::contentTypeFolderName(m_contentType);
}

void ContentBrowser::setContentType(const QString& contentType)
{
	const auto type = typeFromString(contentType);
	if (m_contentType == type) {
		return;
	}
	m_contentType = type;
	emit contentTypeChanged();
	emit resultsChanged();
	emit sortOptionsChanged();
	m_sortIndex = 0;
	emit sortIndexChanged();
	emit searchingChanged();
	emit canFetchMoreChanged();
	emit countChanged();
	setError(QString());
}

void ContentBrowser::setQuery(const QString& query)
{
	if (m_query == query) {
		return;
	}
	m_query = query;
	emit queryChanged();
}

void ContentBrowser::setSortIndex(int index)
{
	if (m_sortIndex == index) {
		return;
	}
	m_sortIndex = index;
	emit sortIndexChanged();
}

QVariantList
ContentBrowser::sortOptionsFor(const QList<ModPlatform::SortingMethod>& methods)
{
	QVariantList result;
	for (int i = 0; i < methods.size(); ++i) {
		QVariantMap entry;
		entry[QStringLiteral("id")] = i;
		entry[QStringLiteral("label")] = methods.at(i).readableName;
		result.append(entry);
	}
	return result;
}

QVariantList ContentBrowser::sortOptions() const
{
	auto* model = currentModel();
	return model ? sortOptionsFor(model->sortingMethods()) : QVariantList();
}

bool ContentBrowser::searching() const
{
	auto* model = currentModel();
	return model && model->isSearching();
}

bool ContentBrowser::canFetchMore() const
{
	auto* model = currentModel();
	return model && model->canFetchMore(QModelIndex());
}

int ContentBrowser::count() const
{
	auto* model = currentModel();
	return model ? model->rowCount(QModelIndex()) : 0;
}

void ContentBrowser::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}

QObject* ContentBrowser::results() const
{
	return currentModel();
}

QString ContentBrowser::modelKey(const QString& provider,
								 ModPlatform::ContentType type) const
{
	return provider + QStringLiteral(":") +
		   ModPlatform::contentTypeFolderName(type);
}

std::shared_ptr<ModFolderModel>
ContentBrowser::folderModelFor(ModPlatform::ContentType type) const
{
	if (!m_instance) {
		return nullptr;
	}
	switch (type) {
		case ModPlatform::ContentType::Mod:
			return m_instance->loaderModList();
		case ModPlatform::ContentType::ResourcePack:
			return m_instance->resourcePackList();
		case ModPlatform::ContentType::ShaderPack:
			return m_instance->shaderPackList();
		case ModPlatform::ContentType::DataPack:
			return m_instance->dataPackList();
	}
	return nullptr;
}

ContentProviderModel* ContentBrowser::ensureModel(const QString& provider,
												  ModPlatform::ContentType type)
{
	const QString key = modelKey(provider, type);
	auto it = m_models.constFind(key);
	if (it != m_models.constEnd()) {
		return it.value();
	}

	/* Same filters DownloadContentDialog::buildPages() builds: this
	 * instance's own Minecraft version, and its loader for content that is
	 * loader-specific. */
	ModPlatform::SearchFilters filters;
	filters.mcVersions = ModPlatform::singleVersionList(m_mcVersion);
	if (!m_loaderType.isEmpty() && ModPlatform::contentTypeUsesLoader(type)) {
		filters.loaders = QStringList{m_loaderType};
	}

	ContentProviderModel* model = nullptr;
	if (provider == QStringLiteral("curseforge")) {
		model = new FlameContentModel(type, filters, this);
	} else {
		model = new ModrinthContentModel(type, filters, this);
	}

	if (auto folder = folderModelFor(type)) {
		model->setInstalledIndex(folder->metadataIndex());
	}

	connectModel(model);
	m_models.insert(key, model);
	return model;
}

ContentProviderModel* ContentBrowser::currentModel() const
{
	return const_cast<ContentBrowser*>(this)->ensureModel(m_provider,
														   m_contentType);
}

void ContentBrowser::connectModel(ContentProviderModel* model)
{
	connect(model, &ContentProviderModel::searchStateChanged, this,
			&ContentBrowser::onModelSearchStateChanged);
	connect(model, &ContentProviderModel::entryUpdated, this,
			&ContentBrowser::onEntryUpdated);
}

void ContentBrowser::onModelSearchStateChanged()
{
	auto* model = qobject_cast<ContentProviderModel*>(sender());
	if (!model || model != currentModel()) {
		/* Some other (provider, contentType) pair, visited earlier and
		 * still finishing up in the background - nothing currently shown
		 * needs to move for it. */
		return;
	}

	emit searchingChanged();
	emit canFetchMoreChanged();
	emit countChanged();
	if (!model->isSearching()) {
		setError(model->lastError());
	}
}

void ContentBrowser::search()
{
	if (m_provider == QStringLiteral("curseforge") && curseForgeKeyMissing()) {
		setError(tr("This build has no CurseForge API key configured, so "
					"CurseForge cannot be searched."));
		return;
	}

	auto* model = currentModel();
	if (!model) {
		return;
	}
	/* Cleared here rather than left for onModelSearchStateChanged() to
	 * pick up once this concludes - a stale error from a previous search
	 * should not still be on screen while a new one is in flight. */
	setError(QString());
	model->search(m_query, m_sortIndex);
}

void ContentBrowser::fetchMore()
{
	auto* model = currentModel();
	if (!model || !model->canFetchMore(QModelIndex())) {
		return;
	}
	model->fetchMore(QModelIndex());
}

bool ContentBrowser::isVersionCompatible(
	const ModPlatform::ContentVersion& version, const QString& mcVersion,
	const QString& loader)
{
	/* A version that does not state a game version/loader of its own is
	 * accepted for that half of the check rather than flagged incompatible
	 * - plenty of provider replies simply do not say (see the field
	 * comments on ContentVersion), and this is already only a client-side
	 * second opinion on filtering the provider's own query already asked
	 * for. */
	const bool mcOk = mcVersion.isEmpty() || version.gameVersions.isEmpty() ||
					  version.gameVersions.contains(mcVersion);
	const bool loaderOk = loader.isEmpty() || version.loaders.isEmpty() ||
						  version.loaders.contains(loader,
												   Qt::CaseInsensitive);
	return mcOk && loaderOk;
}

void ContentBrowser::applyVersionsFromProject(
	const ModPlatform::IndexedProject* project)
{
	QVariantList versions;

	if (project) {
		auto sorted = project->versions;
		/* Newest first. Modrinth's own reply already comes this way, but
		 * CurseForge's does not promise any order at all - the same reason
		 * ModPlatform::newestCurseForgeFile() cannot just take the first
		 * entry either - so this is sorted explicitly rather than trusted
		 * from either provider. */
		std::stable_sort(
			sorted.begin(), sorted.end(),
			[](const ModPlatform::ContentVersion& a,
			   const ModPlatform::ContentVersion& b) {
				const QDateTime dateA =
					QDateTime::fromString(a.datePublished, Qt::ISODate);
				const QDateTime dateB =
					QDateTime::fromString(b.datePublished, Qt::ISODate);
				if (dateA.isValid() && dateB.isValid()) {
					return dateA > dateB;
				}
				/* An unknown date sorts after a known one; two unknown
				 * dates keep whatever order they arrived in. */
				return dateA.isValid();
			});

		for (const auto& version : sorted) {
			QVariantMap entry;
			entry[QStringLiteral("id")] = version.versionId;
			entry[QStringLiteral("name")] = version.name;
			entry[QStringLiteral("versionNumber")] = version.versionNumber;
			entry[QStringLiteral("gameVersions")] =
				QVariant::fromValue(version.gameVersions);
			entry[QStringLiteral("loaders")] =
				QVariant::fromValue(version.loaders);
			entry[QStringLiteral("datePublished")] = version.datePublished;
			entry[QStringLiteral("isCompatible")] =
				isVersionCompatible(version, m_mcVersion, m_loaderType);
			versions.append(entry);
		}
	}

	m_versions = versions;
	m_versionsLoading = false;
	emit versionsChanged();
	emit versionsLoadingChanged();
}

void ContentBrowser::onEntryUpdated(int row)
{
	auto* model = qobject_cast<ContentProviderModel*>(sender());
	if (!model || model != m_versionsModel || row != m_versionsRow) {
		/* An answer for a row (or a model) the caller has since moved on
		 * from - see ContentProviderModel::applyVersions() for the same
		 * idea on the model's own side. */
		return;
	}
	applyVersionsFromProject(model->projectAt(row));
}

void ContentBrowser::loadVersions(int row)
{
	auto* model = currentModel();
	const auto* project = model ? model->projectAt(row) : nullptr;
	if (!project) {
		m_versionsModel = nullptr;
		m_versionsRow = -1;
		applyVersionsFromProject(nullptr);
		return;
	}

	m_versionsModel = model;
	m_versionsRow = row;

	if (project->versionsLoaded) {
		applyVersionsFromProject(project);
		return;
	}

	m_versions = QVariantList();
	m_versionsLoading = true;
	emit versionsChanged();
	emit versionsLoadingChanged();

	model->loadEntry(row);
}

QObject* ContentBrowser::install(int row, const QString& versionId)
{
	auto* model = currentModel();
	const auto* project = model ? model->projectAt(row) : nullptr;
	if (!project) {
		qWarning() << "ContentBrowser::install: no such row" << row;
		return nullptr;
	}

	const ModPlatform::ContentVersion* version = nullptr;
	for (const auto& candidate : project->versions) {
		if (candidate.versionId == versionId) {
			version = &candidate;
			break;
		}
	}
	if (!version) {
		qWarning() << "ContentBrowser::install: version" << versionId
				   << "not known for" << project->name
				   << "- call loadVersions() first";
		return nullptr;
	}

	auto folder = folderModelFor(m_contentType);
	if (!folder) {
		qWarning() << "ContentBrowser::install: no folder for this content "
					  "type on this instance";
		return nullptr;
	}

	ModPlatform::SelectedMod mod;
	mod.name = project->name;
	mod.projectId = project->projectId;
	mod.versionId = version->versionId;
	mod.slug = project->slug;
	mod.fileName = version->fileName;
	mod.downloadUrl = version->downloadUrl;
	mod.sha1 = version->sha1;
	mod.fileSize = version->fileSize;
	mod.platform = model->platformId();
	mod.mcVersion = m_mcVersion;
	mod.loaders = m_loaderType;
	mod.versionType = version->versionType;
	mod.browserDownloadOnly = version->browserDownloadOnly;

	/* Dependencies are a mod-only idea - see ModFolderPage::
	 * installSelection()'s own ContentType::Mod check. */
	const bool resolveDependencies =
		m_contentType == ModPlatform::ContentType::Mod;
	const QString loaderForResolver =
		ModPlatform::contentTypeUsesLoader(m_contentType) ? m_loaderType
														  : QString();

	auto* task = new ContentBrowserInstallTask(
		mod, m_mcVersion, loaderForResolver, folder->dir().absolutePath(),
		folder->metadataIndex(), resolveDependencies, this);

	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(project->name);

	/* Refresh the instance's content list afterwards, exactly the way
	 * ModFolderPage::reviewAndInstall() calls m_mods->update() once the
	 * download task is done - on success, on failure (some files may have
	 * landed before the rest failed) and on abort alike. */
	connect(watcher, &TaskWatcher::finished, this,
			[folder](bool) { folder->update(); });

	task->start();
	return watcher;
}

#include "ContentBrowser.moc"
