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
	/* A project to resolve as if a selected mod required it, known only by
	 * the mod id a jar asks for - which is all the mod health check can say
	 * about what is missing. `requiredBy` names the files that need it, for
	 * the review's "Required by" line. */
	struct WantedProject {
		QString id;
		QStringList requiredBy;
	};

	explicit DependencyResolver(
		const QList<ModPlatform::SelectedMod>& selectedMods,
		const QString& mcVersion, const QString& loader,
		QObject* parent = nullptr);

	/* Optional: hand the resolver the persistent install index for the
	 * destination mods folder. When set, any project/version already
	 * present on disk is treated as already-resolved, so transitive
	 * dependencies that are already installed are no longer re-fetched. */
	void setInstalledIndex(std::shared_ptr<ModMetadataIndex> index);

	/* Projects to resolve as if a selected mod required them. Call before
	 * start(); there may be no selected mods at all.
	 *
	 * Each is looked up on Modrinth with the id as the project slug (its
	 * project endpoints take a slug as well as an id): the newest version
	 * for this Minecraft version and loader, the way a required dependency
	 * with no version id is. When Modrinth has no such project, or nothing
	 * of it that runs here, CurseForge is tried the way a cross-resolved
	 * dependency is, with the id as both name and slug - so only an exact
	 * slug or an exact title counts. What neither finds is listed in
	 * unresolvedDependencies() under the id. Their own required
	 * dependencies follow as usual.
	 *
	 * The answer is recorded under the project's own id, not the slug it
	 * was asked for, so a project something else already pulled in is not
	 * fetched twice. Its `slug` is set to the wanted id (when the provider
	 * did not name one) - how a caller tells which wanted id a row answers. */
	void addWantedProjects(const QList<WantedProject>& wanted);

	QList<ModPlatform::DependencyInfo> resolvedDependencies() const
	{
		return m_dependencies;
	}
	QList<ModPlatform::UnresolvedDep> unresolvedDependencies() const
	{
		return m_unresolvedDeps;
	}

	/* Resolution is a fan-out of network lookups, so it can be given up
	 * on at any point - which the progress dialog's Skip button does.
	 * Whatever was resolved before that is kept and returned. */
	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

  private:
	/* Fires one lookup and routes the answer.
	 *
	 * Every lookup in here used to spell the same thing out: allocate a
	 * buffer, new up a job, bump the pending counter, then remember to
	 * free the buffer and decrement in both handlers. Thirteen copies of
	 * that meant thirteen chances to leak a buffer, lose count, or - now
	 * that the task can be aborted - write into a resolver that is no
	 * longer running.
	 *
	 * `onDone` gets the reply, or an empty array if the request failed,
	 * and runs only while the task is still going. The pending counter is
	 * decremented after it returns, so a handler that starts further
	 * lookups of its own cannot let the count touch zero in between and
	 * report the whole job finished early. */
	void request(const QString& name, const QUrl& url,
				 std::function<void(const QByteArray&)> onDone);

	void resolveNextMod();
	void resolveWantedProjects();
	void onWantedProjectResolved(const WantedProject& wanted,
								 const QByteArray& data);
	/// Renames the accepted dependency of Modrinth project @p projectId
	/// after the project's title.
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
	/* `requiredBy` travels down the whole chain so that a library
	 * pulled in three levels deep can still say what asked for it,
	 * which is what the review dialog shows on its "Required by" line. */
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
	 * Modrinth by name. A wanted project started on Modrinth, so for it
	 * there is nowhere left to ask and it is listed as unresolved. */
	void noUsableCurseForgeFile(const QString& projectId,
								const QStringList& requiredBy,
								const QString& wantedId);

	/* Takes a fully resolved dependency, or drops it when the very
	 * version it asks for is already on disk. Marks it as possibly
	 * installed when some other version of it is. */
	void acceptDependency(ModPlatform::DependencyInfo dep);
	/* A dependency two things need is downloaded once, so the second
	 * thing to ask for it only adds its name to the existing entry - and
	 * @p slug, when it is the wanted id that asked. The entry may not exist
	 * yet: a project is claimed when its lookup is made and recorded when
	 * the answer comes back, so what is said in between is kept in
	 * m_lateNotes and joins the entry as acceptDependency() records it. */
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
	/* platform:projectId -> what noteAlsoRequiredBy() was told about a
	 * dependency not recorded yet. Only `requiredBy` and `slug` are used. */
	QHash<QString, ModPlatform::DependencyInfo> m_lateNotes;
	std::shared_ptr<ModMetadataIndex> m_installed;
	QString m_mcVersion;
	QString m_loader;
	int m_currentModIndex = 0;
	int m_pendingRequests = 0;

	/* Lookups still in flight, so abort() can call them off. Guarded
	 * pointers because a job deletes itself once it has reported. */
	QList<QPointer<NetJob>> m_activeJobs;
	/* Latched by abort(): no further lookups are started and no reply is
	 * acted on, but replies already queued are allowed to arrive and be
	 * discarded rather than racing the destructor. */
	bool m_aborted = false;
};
