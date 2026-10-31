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

#include <QObject>
#include <QHash>
#include <QList>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <functional>
#include <memory>

#include "modplatform/ModDownloadTypes.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

class ModMetadataIndex;

class DependencyResolver : public Task
{
	Q_OBJECT

  public:
	/* A project to resolve as if a selected mod required it, known only by the
	 * mod id a jar asks for; `requiredBy` names the files that need it. */
	struct WantedProject {
		QString id;
		QStringList requiredBy;
	};

	explicit DependencyResolver(
		const QList<ModPlatform::SelectedMod>& selectedMods,
		const QString& mcVersion, const QString& loader,
		QObject* parent = nullptr);

	/* Optional: the install index of the destination mods folder. A
	 * dependency at the very version on disk is dropped; one present at
	 * another version is offered unticked. */
	void setInstalledIndex(std::shared_ptr<ModMetadataIndex> index);

	/* Projects to resolve as if a selected mod required them. Call before
	 * start(); there may be no selected mods.
	 *
	 * Each is looked up on Modrinth with the id as slug (newest version for
	 * this Minecraft version and loader), else on CurseForge with the id as
	 * name and slug (exact matches only). What neither finds is listed in
	 * unresolvedDependencies() under the id. The answer is recorded under the
	 * project's own id so it is not fetched twice, with `slug` set to the
	 * wanted id when the provider names none, so a caller can tell which
	 * wanted id a row answers. */
	void addWantedProjects(const QList<WantedProject>& wanted);

	QList<ModPlatform::DependencyInfo> resolvedDependencies() const
	{
		return m_dependencies;
	}
	QList<ModPlatform::UnresolvedDep> unresolvedDependencies() const
	{
		return m_unresolvedDeps;
	}

	/* Resolution is a fan-out of lookups that can be given up at any point
	 * (the cancel button); what was resolved is kept. */
	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

  private:
	/* Fires one lookup and routes the answer. `onDone` gets the reply, or an
	 * empty array if the request failed, and runs only while the task is
	 * going. The pending counter drops after it returns, so a handler that
	 * starts further lookups cannot let the count touch zero early. */
	void request(const QString& name, const QUrl& url,
				 std::function<void(const QByteArray&)> onDone);

	void resolveNextMod();
	void resolveWantedProjects();
	void onWantedProjectResolved(const WantedProject& wanted,
								 const QByteArray& data);
	void nameAfterProject(const QString& projectId);
	void resolveCurseForgeDependencies(const ModPlatform::SelectedMod& mod);
	void resolveModrinthDependencies(const ModPlatform::SelectedMod& mod);
	void onCurseForgeVersionResolved(const ModPlatform::SelectedMod& mod,
									 const QByteArray& data);
	void onModrinthVersionResolved(const ModPlatform::SelectedMod& mod,
								   const QByteArray& data);
	/* @p wantedId: the wanted project this answers, or empty for a
	 * dependency some file named. */
	void onDependencyProjectResolved(const QString& platform,
									 const QString& projectId,
									 const QByteArray& data,
									 const QStringList& requiredBy,
									 const QString& wantedId = QString());
	void checkCompletion();

  private:
	/* `requiredBy` travels down the chain so a deep library can still say
	 * what asked for it. */
	void processCFFileDeps(const QJsonObject& fileObj,
						   const QStringList& requiredBy);
	void processMRVersionDeps(const QJsonObject& versionObj,
							  const QStringList& requiredBy);
	void crossResolveFromCurseForge(const QString& projectId,
									const QStringList& requiredBy);
	void crossResolveFromModrinth(const QString& projectId,
								  const QStringList& requiredBy);
	void executeCrossResolve(const QString& targetPlatform,
							 const QString& projectName,
							 const QString& sourceSlug,
							 const QStringList& requiredBy,
							 const QString& wantedId = QString());
	/* A CurseForge dependency with no file that runs here: asked for on
	 * Modrinth by name. A wanted project started on Modrinth, so it is listed
	 * as unresolved instead. */
	void noUsableCurseForgeFile(const QString& projectId,
								const QStringList& requiredBy,
								const QString& wantedId);

	/* Takes a resolved dependency, or drops it when that very version is on
	 * disk. Marks it maybe-installed when another version is. */
	void acceptDependency(ModPlatform::DependencyInfo dep);
	/* A dependency two things need is downloaded once: later requesters only
	 * add their name (and @p slug, if the wanted id asked). The entry may not
	 * exist yet, so notes in between wait in m_lateNotes for
	 * acceptDependency(). */
	void noteAlsoRequiredBy(const QString& platform, const QString& projectId,
							const QStringList& requiredBy,
							const QString& slug = QString());

	bool isVersionInstalled(const QString& platform, const QString& projectId,
							const QString& versionId) const;
	bool isProjectInstalled(const QString& platform, const QString& projectId,
							const QString& name) const;

	static QString normalizeName(const QString& name);

  private:
	QList<ModPlatform::SelectedMod> m_selectedMods;
	QList<WantedProject> m_wanted;
	QList<ModPlatform::DependencyInfo> m_dependencies;
	QList<ModPlatform::UnresolvedDep> m_unresolvedDeps;
	QSet<QString> m_resolvedProjectIds; // avoid duplicates (platform:projectId)
	QSet<QString>
		m_resolvedNames; // avoid cross-platform duplicates (normalized name)
	/* platform:projectId -> notes for a dependency not recorded yet. Only
	 * `requiredBy` and `slug` are used. */
	QHash<QString, ModPlatform::DependencyInfo> m_lateNotes;
	std::shared_ptr<ModMetadataIndex> m_installed;
	QString m_mcVersion;
	QString m_loader;
	int m_currentModIndex = 0;
	int m_pendingRequests = 0;

	/* Lookups in flight, for abort(). Guarded: a job deletes itself after
	 * reporting. */
	QList<QPointer<NetJob>> m_activeJobs;
	/* Latched by abort(): no lookups start and no reply is acted on; queued
	 * replies still arrive and are discarded instead of racing the
	 * destructor. */
	bool m_aborted = false;
};
