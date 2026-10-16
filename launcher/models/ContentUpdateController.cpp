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

#include "ContentUpdateController.h"

#include <QSet>

#include "BuildConfig.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/mod/ModFolderModel.h"
#include "modplatform/ContentApi.h"
#include "modplatform/ContentDownloadTask.h"
#include "modplatform/ContentProviderModel.h"
#include "modplatform/flame/FlameApi.h"
#include "modplatform/flame/FlameContentModel.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "modplatform/modrinth/ModrinthContentModel.h"
#include "models/ContentBrowser.h"
#include "models/ContentFilterModel.h"
#include "models/ContentInstallReview.h"
#include "models/InstanceContentController.h"
#include "tasks/TaskWatcher.h"

// ---------------------------------------------------------------------------
// ContentUpdateList

ContentUpdateList::ContentUpdateList(QObject* parent)
	: QAbstractListModel(parent)
{
}

int ContentUpdateList::checkedCount() const
{
	int n = 0;
	for (const Row& row : m_rows) {
		if (row.checked) {
			++n;
		}
	}
	return n;
}

int ContentUpdateList::rowCount(const QModelIndex& parent) const
{
	return parent.isValid() ? 0 : int(m_rows.size());
}

QString ContentUpdateList::readableVersion(const QString& number,
										   const QString& fileName)
{
	return number.isEmpty() ? fileName : number;
}

QString
ContentUpdateList::changelogUrl(const ModUpdateCheckTask::UpdateInfo& update)
{
	if (update.platform == ModrinthApi::get().id()) {
		/* The version page leads with its changelog. */
		return QStringLiteral("https://modrinth.com/mod/%1/version/%2")
			.arg(update.item.projectId, update.newVersionId);
	}
	if (update.platform == FlameApi::get().id()) {
		return FlameApi::get().projectPageUrl(update.item.projectId).toString();
	}
	return QString();
}

QVariant ContentUpdateList::data(const QModelIndex& index, int role) const
{
	if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
		return QVariant();
	}
	const Row& row = m_rows.at(index.row());
	const auto& u = row.update;
	switch (role) {
		case Qt::DisplayRole:
		case NameRole:
			return u.name;
		case ProviderRole:
			return ContentInstallReview::providerName(u.platform);
		case CurrentVersionRole:
			return readableVersion(u.currentVersionNumber, u.currentFileName);
		case NewVersionRole:
			return readableVersion(u.newVersionNumber, u.item.fileName);
		case FileNameRole:
			return u.currentFileName;
		case ChangelogRole:
			return changelogUrl(u);
		case CheckedRole:
			return row.checked;
		default:
			break;
	}
	return QVariant();
}

QHash<int, QByteArray> ContentUpdateList::roleNames() const
{
	return {
		{NameRole, "name"},
		{ProviderRole, "provider"},
		{CurrentVersionRole, "currentVersion"},
		{NewVersionRole, "newVersion"},
		{FileNameRole, "fileName"},
		{ChangelogRole, "changelog"},
		{CheckedRole, "checked"},
	};
}

void ContentUpdateList::setUpdates(
	const QList<ModUpdateCheckTask::UpdateInfo>& updates)
{
	beginResetModel();
	m_rows.clear();
	for (const auto& update : updates) {
		m_rows.append({update, true});
	}
	/* By name, the order the Content tab lists them in. */
	std::sort(m_rows.begin(), m_rows.end(), [](const Row& a, const Row& b) {
		return a.update.name.compare(b.update.name, Qt::CaseInsensitive) < 0;
	});
	endResetModel();
	emit countChanged();
	emit checkedChanged();
}

void ContentUpdateList::clear()
{
	setUpdates({});
}

QList<ModPlatform::DownloadItem> ContentUpdateList::checkedItems() const
{
	QList<ModPlatform::DownloadItem> items;
	for (const Row& row : m_rows) {
		if (row.checked) {
			items.append(row.update.item);
		}
	}
	return items;
}

void ContentUpdateList::setChecked(int row, bool checked)
{
	if (row < 0 || row >= m_rows.size() || m_rows[row].checked == checked) {
		return;
	}
	m_rows[row].checked = checked;
	emit dataChanged(index(row), index(row), {CheckedRole});
	emit checkedChanged();
}

void ContentUpdateList::setAllChecked(bool checked)
{
	for (int row = 0; row < m_rows.size(); ++row) {
		setChecked(row, checked);
	}
}

// ---------------------------------------------------------------------------
// ContentUpdateController

ContentUpdateController::ContentUpdateController(
	MinecraftInstance* instance, InstanceContentController* lists,
	QObject* parent)
	: QObject(parent), m_instance(instance), m_lists(lists),
	  m_updates(new ContentUpdateList(this))
{
}

ContentUpdateController::~ContentUpdateController()
{
	if (m_check && m_check->isRunning()) {
		m_check->abort();
	}
}

QObject* ContentUpdateController::updates() const
{
	return m_updates;
}

void ContentUpdateController::setLastError(const QString& error)
{
	if (m_lastError == error) {
		return;
	}
	m_lastError = error;
	emit lastErrorChanged();
}

QString ContentUpdateController::mcVersion() const
{
	auto profile = m_instance ? m_instance->getPackProfile() : nullptr;
	return profile ? profile->getComponentVersion("net.minecraft") : QString();
}

QString ContentUpdateController::loader() const
{
	auto profile = m_instance ? m_instance->getPackProfile() : nullptr;
	return profile ? profile->primaryModLoader() : QString();
}

bool ContentUpdateController::contentTypeFor(const QString& kind,
											 ModPlatform::ContentType& type)
{
	if (kind == QStringLiteral("mods")) {
		type = ModPlatform::ContentType::Mod;
	} else if (kind == QStringLiteral("resourcepacks")) {
		type = ModPlatform::ContentType::ResourcePack;
	} else if (kind == QStringLiteral("shaderpacks")) {
		type = ModPlatform::ContentType::ShaderPack;
	} else if (kind == QStringLiteral("datapacks")) {
		type = ModPlatform::ContentType::DataPack;
	} else {
		/* Core mods and texture packs: no provider hosts them. */
		return false;
	}
	return true;
}

bool ContentUpdateController::supportsUpdates(const QString& kind) const
{
	ModPlatform::ContentType type;
	return contentTypeFor(kind, type);
}

QList<ModUpdateCheckTask::UpdateInfo> ContentUpdateController::onlyFor(
	const QList<ModUpdateCheckTask::UpdateInfo>& updates,
	const QStringList& keys)
{
	if (keys.isEmpty()) {
		return updates;
	}
	const QSet<QString> wanted(keys.begin(), keys.end());
	QList<ModUpdateCheckTask::UpdateInfo> out;
	for (const auto& update : updates) {
		if (wanted.contains(
				ModMetadataIndex::canonicalFileName(update.currentFileName))) {
			out.append(update);
		}
	}
	return out;
}

QList<ModPlatform::SelectedMod> ContentUpdateController::resolvableEntries(
	const QList<ModMetadataIndex::Entry>& entries, const QString& mcVersion)
{
	QList<ModPlatform::SelectedMod> mods;
	for (const auto& entry : entries) {
		if (!entry.hasPlatformOrigin()) {
			continue;
		}
		/* Either identifier will do: a file installed from an mrpack has
		 * no version id recorded, and the resolver can look a version up
		 * by its hash instead (ModFolderPage::verifyDependencies()). */
		if (entry.versionId.isEmpty() && entry.sha1.isEmpty()) {
			continue;
		}
		ModPlatform::SelectedMod mod;
		mod.name = entry.name;
		mod.projectId = entry.projectId;
		mod.versionId = entry.versionId;
		mod.sha1 = entry.sha1;
		mod.slug = entry.slug;
		mod.fileName = entry.fileName;
		mod.platform = entry.platform;
		mod.mcVersion = mcVersion;
		mods.append(mod);
	}
	return mods;
}

void ContentUpdateController::fillCurrentVersions(
	QList<ModUpdateCheckTask::UpdateInfo>& updates, const ModFolderModel& folder)
{
	for (auto& update : updates) {
		if (!update.currentVersionNumber.isEmpty()) {
			continue;
		}
		const QString key =
			ModMetadataIndex::canonicalFileName(update.currentFileName);
		for (size_t row = 0; row < folder.size(); ++row) {
			const Mod& mod = folder.at(row);
			if (ContentFilterModel::keyFor(mod) == key) {
				update.currentVersionNumber = mod.version();
				break;
			}
		}
	}
}

int ContentUpdateController::forgetOrigins(ModFolderModel& folder,
										   const QStringList& keys)
{
	auto index = folder.metadataIndex();
	if (!index || keys.isEmpty()) {
		return 0;
	}
	const QSet<QString> wanted(keys.begin(), keys.end());
	int removed = 0;
	for (size_t row = 0; row < folder.size(); ++row) {
		const QString key = ContentFilterModel::keyFor(folder.at(row));
		if (wanted.contains(key) && index->contains(key) && index->remove(key)) {
			++removed;
		}
	}
	return removed;
}

QObject* ContentUpdateController::watchDownload(
	Task* task, const QString& title, std::shared_ptr<ModFolderModel> folder)
{
	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(title);
	/* Re-read the folder whatever happened: some files may have landed
	 * before the rest failed. */
	connect(watcher, &TaskWatcher::finished, this, [folder](bool) {
		if (folder) {
			folder->update();
		}
	});
	task->start();
	return watcher;
}

QObject* ContentUpdateController::checkForUpdates(const QString& kind,
												  const QStringList& keys)
{
	if (m_check) {
		setLastError(tr("Already checking for updates."));
		return nullptr;
	}
	ModPlatform::ContentType type;
	auto folder = m_lists ? m_lists->folderFor(kind) : nullptr;
	if (!contentTypeFor(kind, type) || !folder) {
		setLastError(tr("These cannot be updated from Modrinth or CurseForge."));
		return nullptr;
	}
	if (!m_lists->changesAllowed(kind)) {
		setLastError(tr("The game is running. Check again once it has closed."));
		return nullptr;
	}
	const QString mc = mcVersion();
	if (mc.isEmpty()) {
		setLastError(tr("This instance has no Minecraft version set."));
		return nullptr;
	}

	/* Something to ask about at all: a file with a recorded origin among
	 * the ones asked for. Without one the check would only report that
	 * there was nothing to check. */
	auto index = folder->metadataIndex();
	const QSet<QString> wanted(keys.begin(), keys.end());
	bool anyTracked = false;
	for (const auto& entry : index ? index->all() : QList<ModMetadataIndex::Entry>()) {
		if (entry.hasPlatformOrigin() &&
			(wanted.isEmpty() ||
			 wanted.contains(ModMetadataIndex::canonicalFileName(entry.fileName)))) {
			anyTracked = true;
			break;
		}
	}
	if (!anyTracked) {
		setLastError(tr("None of these were installed from Modrinth or "
						"CurseForge, so there is nothing to check. Files "
						"added from Add content can be checked."));
		return nullptr;
	}

	setLastError(QString());
	m_updates->clear();
	m_updatesKind = kind;
	m_updatesFolder = folder;

	auto* task = new ModUpdateCheckTask(index, mc, loader(), type);
	m_check = task;
	auto* watcher = new TaskWatcher(Task::Ptr(task), this);
	watcher->setTitle(tr("Checking for updates"));
	QPointer<ModUpdateCheckTask> guard(task);
	connect(watcher, &TaskWatcher::finished, this, [this, guard, keys](bool) {
		/* Kept after an abort too: what was checked is checked. */
		if (guard) {
			auto found = onlyFor(guard->availableUpdates(), keys);
			if (m_updatesFolder) {
				fillCurrentVersions(found, *m_updatesFolder);
			}
			m_updates->setUpdates(found);
		}
		m_check = nullptr;
		emit checkingChanged();
	});
	emit checkingChanged();
	task->start();
	return watcher;
}

void ContentUpdateController::cancelCheck()
{
	if (m_check && m_check->isRunning()) {
		m_check->abort();
	}
}

QObject* ContentUpdateController::applyUpdates()
{
	const auto items = m_updates->checkedItems();
	if (items.isEmpty() || !m_updatesFolder) {
		setLastError(tr("Nothing is ticked to update."));
		return nullptr;
	}
	if (m_lists && !m_lists->changesAllowed(m_updatesKind)) {
		setLastError(tr("The game is running. Update once it has closed."));
		return nullptr;
	}
	setLastError(QString());

	auto* task =
		new ContentDownloadTask(items, m_updatesFolder->dir().absolutePath());
	task->setMetadataIndex(m_updatesFolder->metadataIndex());
	QObject* watcher = watchDownload(
		task, tr("Updating %n file(s)", "", int(items.size())), m_updatesFolder);
	/* Applied: the same list must not be offered twice. */
	m_updates->clear();
	return watcher;
}

bool ContentUpdateController::loadVersions(const QString& kind,
										   const QString& key)
{
	ModPlatform::ContentType type;
	auto folder = m_lists ? m_lists->folderFor(kind) : nullptr;
	if (!contentTypeFor(kind, type) || !folder) {
		setLastError(tr("These cannot be changed to another version here."));
		return false;
	}
	auto index = folder->metadataIndex();
	const auto entry = index ? index->get(key) : ModMetadataIndex::Entry();
	QString name = entry.name;
	for (size_t row = 0; row < folder->size() && name.isEmpty(); ++row) {
		if (ContentFilterModel::keyFor(folder->at(row)) == key) {
			name = folder->at(row).name();
		}
	}
	if (!entry.hasPlatformOrigin()) {
		setLastError(tr("There is no record of where “%1” came from, so its "
						"other versions cannot be listed. Install it from "
						"Add content once and its version can be changed "
						"from then on.")
						 .arg(name.isEmpty() ? key : name));
		return false;
	}
	const bool modrinth = entry.platform == ModrinthApi::get().id();
	const bool curseforge = entry.platform == FlameApi::get().id();
	if (!modrinth && !curseforge) {
		setLastError(tr("“%1” came from a source that cannot be browsed here.")
						 .arg(name));
		return false;
	}
	if (curseforge && BuildConfig.CURSEFORGE_API_KEY.isEmpty()) {
		setLastError(tr("This build has no CurseForge API key configured, so "
						"CurseForge cannot be asked for versions."));
		return false;
	}
	setLastError(QString());

	/* The same query Add content makes: this instance's version, and its
	 * loader for content that has one. */
	const QString mc = mcVersion();
	ModPlatform::SearchFilters filters;
	filters.mcVersions = ModPlatform::singleVersionList(mc);
	if (ModPlatform::contentTypeUsesLoader(type)) {
		filters.loaders = ModPlatform::singleLoaderList(loader());
	}

	if (m_versionsModel) {
		m_versionsModel->deleteLater();
	}
	ContentProviderModel* model = nullptr;
	if (curseforge) {
		model = new FlameContentModel(type, filters, this);
	} else {
		model = new ModrinthContentModel(type, filters, this);
	}
	model->setInstalledIndex(index);
	connect(model, &ContentProviderModel::searchStateChanged, this,
			&ContentUpdateController::onVersionsSearchState);
	connect(model, &ContentProviderModel::entryUpdated, this,
			&ContentUpdateController::onVersionsEntryUpdated);
	m_versionsModel = model;

	m_versionsKind = kind;
	m_versionsPlatform = entry.platform;
	m_versionsFolder = folder;
	m_versionsTitle = name;
	m_installedVersionId = entry.versionId;
	m_versions = QVariantList();
	m_versionsError.clear();
	m_versionsLoading = true;
	emit versionsChanged();

	model->searchProject(entry.projectId);
	return true;
}

void ContentUpdateController::onVersionsSearchState()
{
	auto* model = qobject_cast<ContentProviderModel*>(sender());
	if (!model || model != m_versionsModel || model->isSearching()) {
		return;
	}
	if (model->rowCount(QModelIndex()) == 0) {
		m_versionsLoading = false;
		m_versionsError = model->lastError().isEmpty()
							  ? tr("The project could not be found.")
							  : model->lastError();
		emit versionsChanged();
		return;
	}
	model->loadEntry(0);
}

void ContentUpdateController::onVersionsEntryUpdated(int row)
{
	auto* model = qobject_cast<ContentProviderModel*>(sender());
	if (!model || model != m_versionsModel || row != 0) {
		return;
	}
	const auto* project = model->projectAt(0);
	if (!project || !project->versionsLoaded) {
		/* The description arrived first; the versions are still on
		 * their way. */
		return;
	}
	ModPlatform::ContentType type = ModPlatform::ContentType::Mod;
	contentTypeFor(m_versionsKind, type);
	const QString loaderName =
		ModPlatform::contentTypeUsesLoader(type) ? loader() : QString();
	m_versions =
		ContentBrowser::versionEntries(project->versions, mcVersion(), loaderName);
	m_versionsLoading = false;
	if (m_versions.isEmpty()) {
		m_versionsError = tr("No version of it runs on this instance.");
	}
	emit versionsChanged();
}

QObject* ContentUpdateController::reviewVersion(const QString& versionId)
{
	const auto* project = m_versionsModel ? m_versionsModel->projectAt(0) : nullptr;
	if (!project || !m_versionsFolder) {
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
		return nullptr;
	}
	if (m_lists && !m_lists->changesAllowed(m_versionsKind)) {
		setLastError(tr("The game is running. Change it once it has closed."));
		return nullptr;
	}

	ModPlatform::ContentType type = ModPlatform::ContentType::Mod;
	contentTypeFor(m_versionsKind, type);
	const QString loaderName =
		ModPlatform::contentTypeUsesLoader(type) ? loader() : QString();
	const auto mod = ContentBrowser::selectionFrom(
		*project, *version, m_versionsModel->platformId(), mcVersion(),
		loaderName);
	/* The conflict analyzer turns a pick of something already on disk
	 * into an in-place replacement, so this is the whole of "change
	 * version": ModFolderPage::on_actionChangeVersion_triggered() reaches
	 * the same install path. */
	auto* review = new ContentInstallReview(
		mod, mcVersion(), loaderName, m_versionsFolder,
		type == ModPlatform::ContentType::Mod, this);
	review->start();
	return review;
}

QObject* ContentUpdateController::verifyDependencies()
{
	auto folder = m_lists ? m_lists->folderFor(QStringLiteral("mods")) : nullptr;
	auto index = folder ? folder->metadataIndex() : nullptr;
	if (!folder || !index) {
		return nullptr;
	}
	if (!m_lists->changesAllowed(QStringLiteral("mods"))) {
		setLastError(tr("The game is running. Check again once it has closed."));
		return nullptr;
	}
	const QString mc = mcVersion();
	if (mc.isEmpty()) {
		setLastError(tr("This instance has no Minecraft version set."));
		return nullptr;
	}
	const auto installed = resolvableEntries(index->all(), mc);
	if (installed.isEmpty()) {
		setLastError(tr("None of the mods were installed from Modrinth or "
						"CurseForge, so there is nothing to look up. Mods "
						"added from Add content can be checked."));
		return nullptr;
	}
	setLastError(QString());

	auto* review = ContentInstallReview::missingDependencies(
		installed, tr("Missing dependencies"), mc, loader(), folder, this);
	review->start();
	return review;
}

int ContentUpdateController::resetMetadata(const QString& kind,
										   const QStringList& keys)
{
	auto folder = m_lists ? m_lists->folderFor(kind) : nullptr;
	if (!folder) {
		return 0;
	}
	const int removed = forgetOrigins(*folder, keys);
	if (removed > 0 && m_lists) {
		/* Where a row came from is one of its roles. */
		m_lists->refreshRows(kind, keys);
	}
	return removed;
}
