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

#include <QDateTime>

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
	/* Every bundle of the catalog: its mods are the ones to match to a new
	 * version, not only the performance defaults. */
	const QStringList kCatalogBundles = {kPerformanceBundle,
										 QStringLiteral("comfort")};
	const QString kExcludedSetting = QStringLiteral("KitePerformanceExcluded");
	const QString kIncludedSetting = QStringLiteral("KitePerformanceIncluded");
	const QString kMatchedSetting = QStringLiteral("KitePerformanceFor");
	const QString kMissingSetting = QStringLiteral("KitePerformanceMissing");
	const QString kUnmatchedSetting = QStringLiteral("KitePerformanceUnmatched");
	constexpr int kPercent = 100;

	/* The setup going for each instance, by instance id: a second one for
	 * the same instance waits for it instead of downloading the same files
	 * beside it. */
	QHash<QString, QPointer<PerformanceSetupTask>>& runningSetups()
	{
		static QHash<QString, QPointer<PerformanceSetupTask>> running;
		return running;
	}
} // namespace

PerformanceSetupTask::PerformanceSetupTask(
	std::shared_ptr<MinecraftInstance> instance, const QStringList& extras,
	QObject* parent)
	: Task(parent), m_instance(std::move(instance)), m_extras(extras)
{
}

void PerformanceSetupTask::executeTask()
{
	setProgress(0, 0);
	startWhenFree();
}

void PerformanceSetupTask::startWhenFree()
{
	/* Also what the end of a wait calls: a wake-up that comes after this
	 * task has begun, or been stopped, has nothing left to do. */
	if (m_aborted || m_started || isFinished()) {
		return;
	}
	const QString id = m_instance->id();
	const QPointer<PerformanceSetupTask> other = runningSetups().value(id);
	if (other && other != this) {
		setStatus(tr("Waiting for Kite Performance"));
		/* The other task ends by finishing or by being deleted. Either
		 * way this one asks again, since a third task waiting for the
		 * same instance may have taken it first. */
		connect(other.data(), &Task::finished, this,
				&PerformanceSetupTask::startWhenFree, Qt::SingleShotConnection);
		connect(other.data(), &QObject::destroyed, this,
				&PerformanceSetupTask::startWhenFree, Qt::SingleShotConnection);
		return;
	}
	m_started = true;
	runningSetups().insert(id, this);
	check();
}

void PerformanceSetupTask::check()
{
	setStatus(tr("Checking Kite Performance"));

	auto profile = m_instance->getPackProfile();
	m_mcVersion = profile->getComponentVersion(QStringLiteral("net.minecraft"));
	m_loader = profile->primaryModLoader();
	if (!PerformanceSetup::hasLoader(m_loader)) {
		// A vanilla instance is never changed: Performance makes a separate
		// modded one.
		finish(tr("Kite Performance needs a mod loader; this instance is "
				  "left as it is"));
		return;
	}
	m_folder = m_instance->loaderModList();
	if (m_mcVersion.isEmpty() || !m_folder) {
		finish(tr("Kite Performance: up to date"));
		return;
	}

	/* What the catalog says now: the copy last downloaded, else the
	 * built-in one. No network. */
	BundleCatalog catalog;
	catalog.loadCached();
	m_defaults = PerformanceSetup::defaultProjects(
		catalog.mods({kPerformanceBundle}, m_loader));
	m_excluded = PerformanceSetup::parseProjects(
		m_instance->settings()->get(kExcludedSetting).toString());
	m_included = PerformanceSetup::parseProjects(
		m_instance->settings()->get(kIncludedSetting).toString());
	m_installed = m_folder->metadataIndex()->projectIds(ModrinthApi::get().id());

	/* Mods matched to another version or loader are looked up again. Never
	 * matched before (a setting that is empty) is taken as matched to what
	 * is there - and stored at once, whatever this run finds: on a version
	 * some default does not exist for, no run ends "up to date", and a
	 * later change of version would otherwise go unnoticed. */
	m_matchKey = PerformanceSetup::matchKey(m_mcVersion, m_loader);
	m_missing = PerformanceSetup::recentlyMissing(
		m_instance->settings()->get(kMissingSetting).toString(), m_matchKey,
		QDateTime::currentDateTimeUtc());
	const QString matched =
		m_instance->settings()->get(kMatchedSetting).toString();
	if (matched.isEmpty()) {
		rememberMatch();
	} else if (matched != m_matchKey) {
		/* This run finds out which mods have no build of the new version; the
		 * list of the last one is no longer true. Cleared first so that the
		 * same mods found again are news again. */
		m_instance->settings()->set(kUnmatchedSetting, QString());
		QStringList catalogProjects;
		for (const QVariant& mod : catalog.mods(kCatalogBundles, m_loader)) {
			catalogProjects.append(mod.toMap().value("project").toString());
		}
		m_replacements = PerformanceSetup::filesToRematch(
			m_folder->metadataIndex()->all(), catalogProjects);
	}
	updateWanted();

	if (m_wanted.isEmpty()) {
		rememberMatch();
		finish(tr("Kite Performance: up to date"));
		return;
	}
	identifyJars();
}

void PerformanceSetupTask::updateWanted()
{
	/* A mod Modrinth had no build of lately is left out like one that is
	 * there, so it costs a lookup a day and not one per Play. */
	m_wanted = PerformanceSetup::wantedProjects(
		m_defaults + m_included + m_extras, m_excluded, m_installed + m_missing);
	/* Installed, yes - for the version it used to be. Asked about whatever
	 * was missing: the version is not the one that was recorded. */
	for (const QString& project : rematched()) {
		if (!m_excluded.contains(project) && !m_wanted.contains(project)) {
			m_wanted.append(project);
		}
	}
}

QStringList PerformanceSetupTask::rematched() const
{
	QStringList projects = m_replacements.keys();
	projects.sort();
	return projects;
}

void PerformanceSetupTask::identifyJars()
{
	const QStringList jars = PerformanceSetup::jarsNotFromModrinth(
		m_folder->dir(), *m_folder->metadataIndex());
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
	updateWanted();
	if (m_wanted.isEmpty()) {
		rememberMatch();
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
		/* Catalog ids, which CurseForge does not know. */
		wanted.append({project, {}, /*modrinthOnly=*/true});
	}
	m_review = ContentInstallReview::missingDependencies(
		{}, tr("Kite Performance"), m_mcVersion, m_loader, m_folder, this);
	m_review->setWantedProjects(wanted);
	m_review->setReplacements(m_replacements);
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
	/* Before install() or cancel() ends the review. */
	rememberNotAvailable();
	if (!m_review->unresolved().isEmpty()) {
		qWarning() << "Kite Performance: nothing found for"
				   << m_review->unresolved();
	}
	m_toInstall = m_review->checkedCount();
	if (m_toInstall == 0) {
		/* Rows that are all unticked are mods that are already there;
		 * none at all is e.g. 1.8.9, which has no Sodium: either way the
		 * instance stays as it is. What is unresolved because Modrinth said
		 * it has no build is an answer too; only what it could not be asked
		 * about (offline) leaves the match to be tried again. */
		const bool found = m_review->count() > 0;
		const QStringList unresolved = m_review->unresolved();
		const bool answered =
			PerformanceSetup::commonProjects(unresolved,
											 m_review->notAvailable())
				.size() == unresolved.size();
		if (found || answered) {
			rememberMatch();
		}
		m_review->cancel();
		finish(found
				   ? tr("Kite Performance: up to date")
				   : tr("Kite Performance: no matching mods for this version"));
		return;
	}
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
	m_added = m_toInstall;
	rememberMatch();
	finish(tr("Kite Performance: added %n mod(s)", "", m_toInstall));
}

void PerformanceSetupTask::rememberMatch()
{
	m_instance->settings()->set(kMatchedSetting, m_matchKey);
}

void PerformanceSetupTask::rememberNotAvailable()
{
	const QStringList notAvailable = m_review->notAvailable();
	auto settings = m_instance->settings();

	/* A run that learned nothing new leaves a record still in force alone:
	 * what it says was not asked about this time. One that did adds to it,
	 * so the mods it left out are not asked about again at once. */
	const QStringList found =
		PerformanceSetup::commonProjects(m_wanted, notAvailable);
	if (!found.isEmpty() || m_missing.isEmpty()) {
		QStringList missing = m_missing + found;
		missing.removeDuplicates();
		settings->set(kMissingSetting,
					  missing.isEmpty() ? QString()
										: PerformanceSetup::missingRecord(
											  m_matchKey,
											  QDateTime::currentDateTimeUtc(),
											  missing));
	}

	/* The mods that were to follow the version and have no build of it stay
	 * at their old file, and the game will likely refuse them. */
	if (m_replacements.isEmpty()) {
		return;
	}
	const QStringList unmatched =
		PerformanceSetup::commonProjects(rematched(), notAvailable);
	settings->set(kUnmatchedSetting, PerformanceSetup::joinProjects(unmatched));
	if (unmatched.isEmpty()) {
		return;
	}
	qWarning() << "Kite Performance: no version for Minecraft" << m_mcVersion
			   << "and" << m_loader << "of" << unmatched;
	const QStringList names = PerformanceSetup::installedNames(
		*m_folder->metadataIndex(), unmatched);
	if (!names.isEmpty()) {
		m_unmatchedNote = tr("No version for Minecraft %1: %2.")
							  .arg(m_mcVersion, names.join(QStringLiteral(", ")));
	}
}

void PerformanceSetupTask::release()
{
	auto& running = runningSetups();
	const QString id = m_instance->id();
	if (running.value(id) == this) {
		running.remove(id);
	}
}

void PerformanceSetupTask::finish(const QString& summary, const QString& problem)
{
	if (isFinished()) {
		return;
	}
	if (!problem.isEmpty()) {
		qWarning() << "Kite Performance:" << problem;
	}
	m_summary = m_unmatchedNote.isEmpty()
					? summary
					: summary + QLatin1Char(' ') + m_unmatchedNote;
	setStatus(m_summary);
	release();
	emitSucceeded();
}

bool PerformanceSetupTask::abort()
{
	if (isFinished()) {
		return false;
	}
	m_aborted = true;
	release();
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
