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

#include "NewInstanceController.h"

#include <QFileInfo>
#include <QPointer>
#include <QUrl>

#include "BaseVersion.h"
#include "BaseVersionList.h"
#include "InstanceCreationTask.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "core/LauncherContext.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/Component.h"
#include "models/RoleByName.h"
#include "tasks/Task.h"
#include "tasks/TaskWatcher.h"

namespace
{
	/// The uid for @p loader in knownModLoaders(), or empty for "" or an unknown name.
	QString loaderComponentUid(const QString& loader)
	{
		if (loader == QLatin1String("fabric")) {
			return QStringLiteral("net.fabricmc.fabric-loader");
		}
		if (loader == QLatin1String("quilt")) {
			return QStringLiteral("org.quiltmc.quilt-loader");
		}
		if (loader == QLatin1String("forge")) {
			return QStringLiteral("net.minecraftforge");
		}
		if (loader == QLatin1String("neoforge")) {
			return QStringLiteral("net.neoforged");
		}
		return QString();
	}
} // namespace

VersionListLoadingProxy::VersionListLoadingProxy(QObject* parent)
	: QSortFilterProxyModel(parent)
{
	connect(this, &QAbstractItemModel::rowsInserted, this,
			&VersionListLoadingProxy::countChanged);
	connect(this, &QAbstractItemModel::rowsRemoved, this,
			&VersionListLoadingProxy::countChanged);
	connect(this, &QAbstractItemModel::modelReset, this,
			&VersionListLoadingProxy::countChanged);
	connect(this, &QAbstractItemModel::layoutChanged, this,
			&VersionListLoadingProxy::countChanged);

	connect(this, &QAbstractItemModel::rowsInserted, this,
			&VersionListLoadingProxy::refreshDerived);
	connect(this, &QAbstractItemModel::rowsRemoved, this,
			&VersionListLoadingProxy::refreshDerived);
	connect(this, &QAbstractItemModel::modelReset, this,
			&VersionListLoadingProxy::refreshDerived);
	connect(this, &QAbstractItemModel::layoutChanged, this,
			&VersionListLoadingProxy::refreshDerived);
}

QString VersionListLoadingProxy::firstVersionId() const
{
	return m_firstVersionId;
}

void VersionListLoadingProxy::setLoading(bool loading)
{
	if (m_loading == loading) {
		return;
	}
	m_loading = loading;
	emit loadingChanged();
}

void VersionListLoadingProxy::setError(const QString& error)
{
	if (m_error == error) {
		return;
	}
	m_error = error;
	emit errorChanged();
}

void VersionListLoadingProxy::refreshDerived()
{
	const QString firstVersionId =
		(rowCount() > 0 && m_versionIdRole >= 0)
			? index(0, 0).data(m_versionIdRole).toString()
			: QString();
	if (firstVersionId != m_firstVersionId) {
		m_firstVersionId = firstVersionId;
		emit firstVersionIdChanged();
	}
}

void VersionListLoadingProxy::startLoadIfNeeded()
{
	auto* list = qobject_cast<BaseVersionList*>(sourceModel());
	if (!list || list->isLoaded()) {
		return;
	}

	Task::Ptr task = list->getLoadTask();
	if (!task) {
		return;
	}

	setLoading(true);
	/* A result for a list we have since switched away from must not touch the
	 * state of the current one. */
	connect(task.get(), &Task::succeeded, this, [this, list]() {
		if (sourceModel() == list) {
			setLoading(false);
		}
	});
	connect(task.get(), &Task::failed, this,
			[this, list](const QString& reason) {
				if (sourceModel() != list) {
					return;
				}
				setLoading(false);
				setError(reason);
			});
	if (!task->isRunning()) {
		task->start();
	}
}

void VersionListLoadingProxy::setSourceModel(QAbstractItemModel* sourceModel)
{
	/* Roles first: the base class starts filtering as soon as it has a model. */
	m_versionIdRole = roleByName(sourceModel, "versionId", -1);
	m_sortRole = roleByName(sourceModel, "sort", -1);
	resolveRoles(sourceModel);

	QSortFilterProxyModel::setSourceModel(sourceModel);

	setLoading(false);
	setError(QString());

	/* A QSortFilterProxyModel does not sort until something asks it to. */
	sort(0);
	refreshDerived();
	startLoadIfNeeded();
}

bool VersionListLoadingProxy::lessThan(const QModelIndex& left,
									   const QModelIndex& right) const
{
	if (m_sortRole >= 0) {
		return left.data(m_sortRole).toLongLong() >
			   right.data(m_sortRole).toLongLong();
	}
	/* No sort role: keep source order (Meta::VersionList already sorts newest-first). */
	return left.row() < right.row();
}

MinecraftVersionListProxy::MinecraftVersionListProxy(QObject* parent)
	: VersionListLoadingProxy(parent)
{
}

void MinecraftVersionListProxy::setShowSnapshots(bool show)
{
	if (m_showSnapshots == show) {
		return;
	}
	m_showSnapshots = show;
	emit showSnapshotsChanged();
	invalidateFilter();
}

void MinecraftVersionListProxy::setShowOldVersions(bool show)
{
	if (m_showOldVersions == show) {
		return;
	}
	m_showOldVersions = show;
	emit showOldVersionsChanged();
	invalidateFilter();
}

void MinecraftVersionListProxy::resolveRoles(QAbstractItemModel* sourceModel)
{
	m_typeRole = roleByName(sourceModel, "type", -1);
}

bool MinecraftVersionListProxy::filterAcceptsRow(
	int sourceRow, const QModelIndex& sourceParent) const
{
	if (m_typeRole < 0) {
		return true;
	}
	const QString type =
		sourceModel()->index(sourceRow, 0, sourceParent).data(m_typeRole).toString();
	if (type == QLatin1String("release")) {
		return true;
	}
	if (type == QLatin1String("snapshot")) {
		return m_showSnapshots;
	}
	if (type == QLatin1String("old_alpha") || type == QLatin1String("old_beta") ||
		type == QLatin1String("old_snapshot")) {
		return m_showOldVersions;
	}
	/* Anything else (an "experiment" build) stays hidden. */
	return false;
}

LoaderVersionListProxy::LoaderVersionListProxy(QObject* parent)
	: VersionListLoadingProxy(parent)
{
}

void LoaderVersionListProxy::setMinecraftVersion(const QString& version)
{
	if (m_minecraftVersion == version) {
		return;
	}
	m_minecraftVersion = version;
	invalidateFilter();
}

void LoaderVersionListProxy::resolveRoles(QAbstractItemModel* sourceModel)
{
	m_parentVersionRole = roleByName(sourceModel, "parentGameVersion", -1);
}

bool LoaderVersionListProxy::filterAcceptsRow(
	int sourceRow, const QModelIndex& sourceParent) const
{
	if (m_parentVersionRole < 0 || m_minecraftVersion.isEmpty()) {
		return true;
	}
	const QString parent = sourceModel()
								->index(sourceRow, 0, sourceParent)
								.data(m_parentVersionRole)
								.toString();
	return parent.isEmpty() || parent == m_minecraftVersion;
}

QString composeSuggestedInstanceName(const QString& minecraftVersion,
									 const QString& loader)
{
	if (minecraftVersion.isEmpty()) {
		return QString();
	}
	QString name = minecraftVersion;
	if (const ModLoaderInfo* info = modLoaderForUid(loaderComponentUid(loader))) {
		name += QLatin1Char(' ') + info->brandName;
	}
	return name;
}

QString suggestedImportName(const QString& source)
{
	QString input = source.trimmed();
	if (input.isEmpty()) {
		return QString();
	}

	QUrl url = QUrl::fromUserInput(input);
	if (url.isLocalFile()) {
		return QFileInfo(url.toLocalFile()).completeBaseName();
	}

	/* CurseForge's download-button links end this way; the real file name is one
	 * path segment further in, at ".../file". */
	if (input.endsWith(QLatin1String("?client=y"))) {
		input.chop(9);
		input.append(QLatin1String("/file"));
		url = QUrl::fromUserInput(input);
	}
	return QFileInfo(url.fileName()).completeBaseName();
}

bool importSourceLooksValid(const QString& source)
{
	const QString trimmed = source.trimmed();
	if (trimmed.isEmpty()) {
		return false;
	}

	const QUrl url = QUrl::fromUserInput(trimmed);
	if (!url.isValid() || url.isEmpty()) {
		return false;
	}
	if (!url.isLocalFile()) {
		/* A remote link: only InstanceImportTask can tell whether it is importable. */
		return true;
	}

	/* Extension allow-list; the real format sniff happens in InstanceImportTask. */
	const QFileInfo fi(url.toLocalFile());
	const QString suffix = fi.suffix().toLower();
	const bool looksLikeArchive = suffix == QLatin1String("zip") ||
								  suffix == QLatin1String("mrpack") ||
								  suffix == QLatin1String("jar");
	return fi.exists() && looksLikeArchive;
}

NewInstanceController::NewInstanceController(QObject* parent)
	: QObject(parent),
	  m_minecraftVersions(std::make_unique<MinecraftVersionListProxy>()),
	  m_loaderVersions(std::make_unique<LoaderVersionListProxy>())
{
	auto mcList =
		LAUNCHER->metadataIndex()->get(QStringLiteral("net.minecraft"));
	m_minecraftVersions->setSourceModel(mcList.get());

	connect(m_loaderVersions.get(), &VersionListLoadingProxy::loadingChanged,
			this, &NewInstanceController::loaderLoadingChanged);
	connect(m_loaderVersions.get(),
			&VersionListLoadingProxy::firstVersionIdChanged, this,
			&NewInstanceController::onLoaderListFirstVersionIdChanged);
}

QObject* NewInstanceController::minecraftVersions() const
{
	return m_minecraftVersions.get();
}

QObject* NewInstanceController::loaderVersions() const
{
	return m_loaderVersions.get();
}

bool NewInstanceController::loaderLoading() const
{
	return m_loaderVersions->loading();
}

QStringList NewInstanceController::groups() const
{
	/* Sorted and deduplicated, "" first (no group). */
	auto groups = LAUNCHER->instances()->getGroups();
	groups.removeDuplicates();
	groups.sort(Qt::CaseInsensitive);
	groups.removeOne(QString());
	groups.prepend(QString());
	return groups;
}

void NewInstanceController::setLoader(const QString& loader)
{
	if (m_loader == loader) {
		return;
	}
	m_loader = loader;
	emit loaderChanged();

	if (!m_selectedLoaderVersion.isEmpty()) {
		m_selectedLoaderVersion.clear();
		emit selectedLoaderVersionChanged();
	}

	refreshLoaderSource();
	// A list that is already loaded never announces a "new" first version.
	onLoaderListFirstVersionIdChanged();
}

void NewInstanceController::refreshLoaderSource()
{
	const QString uid = loaderComponentUid(m_loader);
	if (uid.isEmpty()) {
		m_loaderVersions->setSourceModel(nullptr);
		return;
	}

	auto list = LAUNCHER->metadataIndex()->get(uid);
	m_loaderVersions->setMinecraftVersion(m_selectedMinecraftVersion);
	m_loaderVersions->setSourceModel(list.get());
}

void NewInstanceController::selectMinecraftVersion(const QString& version)
{
	if (m_selectedMinecraftVersion == version) {
		return;
	}
	m_selectedMinecraftVersion = version;
	emit selectedMinecraftVersionChanged();

	/* A loader version picked for the previous Minecraft version may not exist for
	 * this one (Forge and NeoForge publish one build per game version): drop it
	 * and re-default. */
	if (!m_selectedLoaderVersion.isEmpty()) {
		m_selectedLoaderVersion.clear();
		emit selectedLoaderVersionChanged();
	}

	m_loaderVersions->setMinecraftVersion(version);
	/* Same list, same first version: nothing would re-default the cleared pick. */
	onLoaderListFirstVersionIdChanged();
}

void NewInstanceController::selectLoaderVersion(const QString& version)
{
	if (m_selectedLoaderVersion == version) {
		return;
	}
	m_selectedLoaderVersion = version;
	emit selectedLoaderVersionChanged();
}

void NewInstanceController::onLoaderListFirstVersionIdChanged()
{
	if (!m_selectedLoaderVersion.isEmpty()) {
		return;
	}
	const QString first = m_loaderVersions->firstVersionId();
	if (first.isEmpty()) {
		return;
	}
	m_selectedLoaderVersion = first;
	emit selectedLoaderVersionChanged();
}

QString NewInstanceController::suggestedName() const
{
	return composeSuggestedInstanceName(m_selectedMinecraftVersion, m_loader);
}

QObject* NewInstanceController::create(const QString& name,
									   const QString& group,
									   const QString& iconKey,
									   bool gameFiles)
{
	if (m_selectedMinecraftVersion.isEmpty()) {
		return nullptr;
	}

	auto mcList =
		LAUNCHER->metadataIndex()->get(QStringLiteral("net.minecraft"));
	BaseVersionPtr version =
		mcList ? mcList->findVersion(m_selectedMinecraftVersion) : nullptr;
	if (!version) {
		return nullptr;
	}

	QString loaderUid;
	QString loaderVersion;
	if (!m_loader.isEmpty() && !m_selectedLoaderVersion.isEmpty()) {
		loaderUid = loaderComponentUid(m_loader);
		loaderVersion = m_selectedLoaderVersion;
	}

	auto* creationTask =
		new InstanceCreationTask(version, loaderUid, loaderVersion);
	creationTask->setName(name);
	creationTask->setGroup(group);
	creationTask->setIcon(iconKey);
	creationTask->setDownloadGameFiles(gameFiles);
	return startInstanceCreation(creationTask, this);
}

TaskWatcher* startInstanceCreation(InstanceTask* creationTask, QObject* parent)
{
	creationTask->setTargetDir(LAUNCHER->instances()->primaryDir());
	const QString name = creationTask->name();
	Task* wrapped = LAUNCHER->instances()->wrapInstanceTask(creationTask);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), parent);
	watcher->setTitle(name);
	/* The final id is settled when InstanceList commits the staged instance, just
	 * before the wrapped task reports success, so the watcher has it by
	 * finished(true). */
	QPointer<InstanceTask> guard(creationTask);
	const auto selected = QObject::connect(
		LAUNCHER->instances().get(), &InstanceList::instanceSelectRequest,
		watcher, [watcher, guard](const QString& id) {
			/* Any commit announces itself here (imports, pack updates); only
			 * the instance we are creating counts. */
			auto instance = LAUNCHER->instances()->getInstanceById(id);
			if (guard && instance && instance->name() == guard->name()) {
				watcher->setInstanceId(id);
			}
		});
	QObject::connect(watcher, &TaskWatcher::finished, watcher,
					 [selected](bool) { QObject::disconnect(selected); });
	wrapped->start();
	return watcher;
}

QString NewInstanceController::suggestedNameForImportSource(
	const QString& source) const
{
	return ::suggestedImportName(source);
}

bool NewInstanceController::isImportSourceValid(const QString& source) const
{
	return ::importSourceLooksValid(source);
}

QObject* NewInstanceController::importFrom(const QString& source,
										   const QString& name,
										   const QString& group,
										   const QString& iconKey)
{
	const QString trimmed = source.trimmed();
	if (trimmed.isEmpty()) {
		return nullptr;
	}

	/* Accepts a bare local path, a "file://" URL or a typed http(s) address alike. */
	const QUrl url = QUrl::fromUserInput(trimmed);
	if (!url.isValid() || url.isEmpty()) {
		return nullptr;
	}

	auto* importTask = new InstanceImportTask(url);
	importTask->setName(name);
	importTask->setGroup(group);
	importTask->setIcon(iconKey);
	importTask->setTargetDir(LAUNCHER->instances()->primaryDir());

	Task* wrapped = LAUNCHER->instances()->wrapInstanceTask(importTask);
	auto* watcher = new TaskWatcher(Task::Ptr(wrapped), this);
	watcher->setTitle(name.isEmpty() ? trimmed : name);
	wrapped->start();
	return watcher;
}
