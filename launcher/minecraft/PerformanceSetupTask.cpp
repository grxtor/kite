/* SPDX-FileCopyrightText: 2026 grxtor
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright (C) 2026 grxtor
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

#include "PerformanceSetupTask.h"

#include "core/LauncherContext.h"
#include "meta/Index.h"
#include "meta/VersionList.h"
#include "minecraft/MinecraftInstance.h"
#include "minecraft/PackProfile.h"
#include "minecraft/PerformanceSetupRules.h"
#include "minecraft/mod/ModFolderModel.h"
#include "minecraft/mod/ModMetadataIndex.h"
#include "models/ContentInstallReview.h"
#include "modplatform/BundleCatalog.h"
#include "modplatform/DependencyResolver.h"
#include "modplatform/modrinth/ModrinthApi.h"
#include "settings/SettingsObject.h"
#include "tasks/TaskWatcher.h"

namespace
{
	const QString kPerformanceBundle = QStringLiteral("performance");
	const QString kFabricUid = QStringLiteral("net.fabricmc.fabric-loader");
	constexpr int kPercent = 100;
} // namespace

PerformanceSetupTask::PerformanceSetupTask(
	std::shared_ptr<MinecraftInstance> instance, QObject* parent)
	: Task(parent), m_instance(std::move(instance))
{
}

void PerformanceSetupTask::executeTask()
{
	setStatus(tr("Checking Kite Performance"));
	setProgress(0, 0);

	auto profile = m_instance->getPackProfile();
	m_mcVersion = profile->getComponentVersion(QStringLiteral("net.minecraft"));
	const QString primary = profile->primaryModLoader();
	m_vanilla = PerformanceSetup::needsLoader(primary);
	m_loader = PerformanceSetup::targetLoader(primary);
	m_folder = m_instance->loaderModList();
	if (m_mcVersion.isEmpty() || !m_folder) {
		finish(tr("Kite Performance: up to date"));
		return;
	}

	/* What the catalog says now: the copy last downloaded, else the
	 * built-in one. No network. */
	BundleCatalog catalog;
	catalog.loadCached();
	for (const QVariant& mod : catalog.mods({kPerformanceBundle}, m_loader)) {
		const QVariantMap fields = mod.toMap();
		if (fields.value("isDefault").toBool()) {
			m_defaults.append(fields.value("project").toString());
		}
	}
	m_excluded = PerformanceSetup::parseExcluded(
		m_instance->settings()->get("KitePerformanceExcluded").toString());
	m_installed = m_folder->metadataIndex()->projectIds(ModrinthApi::get().id());
	m_wanted = PerformanceSetup::wantedProjects(m_defaults, m_excluded, m_installed);

	if (m_wanted.isEmpty()) {
		finish(tr("Kite Performance: up to date"));
		return;
	}
	identifyLooseJars();
}

void PerformanceSetupTask::identifyLooseJars()
{
	const QStringList jars =
		PerformanceSetup::looseJars(m_folder->dir(), *m_folder->metadataIndex());
	const QStringList hashes = PerformanceSetup::sha1sOf(m_folder->dir(), jars);
	if (hashes.isEmpty()) {
		resolveWanted();
		return;
	}

	setStatus(tr("Looking for mods you already have"));
	m_lookup.reset(new Net::JsonPost(tr("Identifying mods"),
									 ModrinthApi::versionsByHashesUrl(),
									 PerformanceSetup::hashLookupBody(hashes)));
	connect(m_lookup.get(), &Task::succeeded, this,
			[this] { onJarsIdentified(m_lookup->response()); });
	connect(m_lookup.get(), &Task::failed, this, [this](QString reason) {
		if (!m_aborted) {
			qWarning() << "Kite Performance: could not identify the mods "
						  "already in the folder:"
					   << reason;
		}
		onJarsIdentified(QByteArray());
	});
	m_lookup->start();
}

void PerformanceSetupTask::onJarsIdentified(const QByteArray& reply)
{
	if (m_aborted) {
		return;
	}
	/* A failed lookup leaves the metadata-only set as it was. */
	m_installed.append(PerformanceSetup::projectsInHashReply(reply));
	m_wanted = PerformanceSetup::wantedProjects(m_defaults, m_excluded, m_installed);
	if (m_wanted.isEmpty()) {
		finish(tr("Kite Performance: up to date"));
		return;
	}
	resolveWanted();
}

void PerformanceSetupTask::resolveWanted()
{
	setStatus(tr("Finding performance mods"));
	QList<DependencyResolver::WantedProject> wanted;
	for (const QString& project : m_wanted) {
		wanted.append({project, {}});
	}
	m_review = ContentInstallReview::missingDependencies(
		{}, tr("Kite Performance"), m_mcVersion, m_loader, m_folder, this);
	m_review->setWantedProjects(wanted);
	connect(m_review, &ContentInstallReview::stateChanged, this, [this] {
		if (m_review && m_review->ready()) {
			onResolved();
		}
	});
	m_review->start();
}

void PerformanceSetupTask::onResolved()
{
	if (m_aborted) {
		return;
	}
	if (!m_review->unresolved().isEmpty()) {
		qWarning() << "Kite Performance: nothing found for"
				   << m_review->unresolved();
	}
	m_toInstall = m_review->checkedCount();
	if (m_toInstall == 0) {
		/* E.g. 1.8.9 has no Sodium: the instance stays as it is. */
		m_review->cancel();
		finish(tr("Kite Performance: no matching mods for this version"));
		return;
	}
	if (m_vanilla) {
		loadLoaderVersion();
	} else {
		install();
	}
}

void PerformanceSetupTask::loadLoaderVersion()
{
	setStatus(tr("Looking up Fabric"));
	auto list = LAUNCHER->metadataIndex()->get(kFabricUid);
	if (!list->isLoaded()) {
		m_loaderListLoad = list->getLoadTask();
		if (m_loaderListLoad && !m_loaderListLoad->isFinished()) {
			connect(m_loaderListLoad.get(), &Task::finished, this,
					&PerformanceSetupTask::onLoaderListReady);
			if (!m_loaderListLoad->isRunning()) {
				m_loaderListLoad->start();
			}
			return;
		}
	}
	onLoaderListReady();
}

void PerformanceSetupTask::onLoaderListReady()
{
	if (m_aborted) {
		return;
	}
	auto list = LAUNCHER->metadataIndex()->get(kFabricUid);
	BaseVersionPtr version = list->getRecommended();
	if (!version && list->count() > 0) {
		version = list->at(0);
	}
	if (!version) {
		m_review->cancel();
		finish(tr("Kite Performance: could not reach Modrinth; playing "
				  "without changes"),
			   QStringLiteral("no Fabric loader version available"));
		return;
	}
	m_loaderVersion = version->descriptor();
	install();
}

void PerformanceSetupTask::install()
{
	setStatus(tr("Downloading performance mods"));
	auto* watcher = qobject_cast<TaskWatcher*>(m_review->install());
	if (!watcher) {
		finish(tr("Kite Performance: no matching mods for this version"));
		return;
	}
	m_download = watcher;
	connect(watcher, &TaskWatcher::progressChanged, this, [this, watcher] {
		setProgress(qint64(watcher->progress() * kPercent), kPercent);
	});
	connect(watcher, &TaskWatcher::finished, this,
			&PerformanceSetupTask::onInstalled);
	if (watcher->succeeded() || watcher->failed()) {
		onInstalled(watcher->succeeded());
	}
}

void PerformanceSetupTask::onInstalled(bool ok)
{
	if (m_aborted || isFinished()) {
		return;
	}
	if (!ok) {
		finish(tr("Kite Performance: could not reach Modrinth; playing "
				  "without changes"),
			   QStringLiteral("the download failed: ") + m_download->error());
		return;
	}
	if (m_vanilla) {
		addLoader();
		return;
	}
	finish(tr("Kite Performance: added %n mod(s)", "", m_toInstall));
}

void PerformanceSetupTask::addLoader()
{
	setStatus(tr("Adding Fabric"));
	auto profile = m_instance->getPackProfile();
	if (!profile->setComponentVersion(kFabricUid, m_loaderVersion)) {
		finish(tr("Kite Performance: added %n mod(s)", "", m_toInstall),
			   QStringLiteral("Fabric could not be added"));
		return;
	}
	/* Fabric's own requirements are settled here, before the launch's
	 * update step reloads the components. */
	profile->resolve(Net::Mode::Online);
	profile->saveNow();
	const QString summary =
		tr("Kite Performance: added Fabric and %n mod(s)", "", m_toInstall);
	auto resolving = profile->getCurrentTask();
	if (!resolving || resolving->isFinished()) {
		finish(summary);
		return;
	}
	connect(resolving.get(), &Task::finished, this, [this, summary, resolving] {
		finish(summary, resolving->wasSuccessful()
							? QString()
							: QStringLiteral("Fabric did not resolve: ") +
								  resolving->failReason());
	});
}

void PerformanceSetupTask::finish(const QString& summary, const QString& problem)
{
	if (isFinished()) {
		return;
	}
	if (!problem.isEmpty()) {
		qWarning() << "Kite Performance:" << problem;
	}
	m_summary = summary;
	setStatus(summary);
	emitSucceeded();
}

bool PerformanceSetupTask::abort()
{
	if (isFinished()) {
		return false;
	}
	m_aborted = true;
	if (m_lookup) {
		m_lookup->abort();
	}
	if (m_review) {
		m_review->cancel();
	}
	if (m_download && m_download->task()) {
		m_download->task()->abort();
	}
	emitAborted();
	return true;
}
