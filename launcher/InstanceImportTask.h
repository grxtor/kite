/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
 * SPDX-FileContributor: grxtor
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2013-2021 MultiMC Contributors
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

#include "InstanceTask.h"
#include "net/HttpMetaCache.h"
#include "net/NetJob.h"
#include <QUrl>
#include <QVector>
#include <QFuture>
#include <QFutureWatcher>
#include "settings/SettingsObject.h"
#include "QObjectPtr.h"

#include <memory>

namespace Flame
{
	class FileResolvingTask;
	struct Manifest;
	struct File;
} // namespace Flame

namespace Modrinth
{
	struct File;
}

class BaseInstance;
class MinecraftInstance;

class InstanceImportTask : public InstanceTask
{
	Q_OBJECT
  public:
	/* Catalogue ids for a pack picked in the launcher's own
	 * Modrinth/CurseForge browser, handed over instead of recovered from the
	 * manifest. Drag-drop imports leave it empty and
	 * processFlame/processModrinth fill the same fields from the manifest. */
	struct PackSourceHint {
		QString provider;	  /* "modrinth" / "curseforge" */
		QString packId;		  /* numeric project id as string */
		QString packSlug;	  /* Modrinth slug, empty for CF */
		QString packName;	  /* pack title as the catalogue spells it */
		QString versionId;	  /* version id (Modrinth) / file id (CF) */
		QString versionLabel; /* human "1.2.3" */
		QString iconUrl;	  /* upstream icon */
		QString sourceUrl;	  /* canonical pack page */
		bool isEmpty() const
		{
			return provider.isEmpty();
		}
	};

	/* Where an update lands when an import replaces an existing instance: the
	 * staging step merges the result over `instanceId`, keeping its id,
	 * directory and files the pack does not ship. `versionId`/`versionLabel`
	 * are what it should claim afterwards; empty for a local file or
	 * user-supplied URL. */
	struct UpdateTarget {
		QString instanceId;
		QString versionId;
		QString versionLabel;
		bool isEmpty() const
		{
			return instanceId.isEmpty();
		}
	};

	void setUpdateTarget(const UpdateTarget& target)
	{
		m_updateTarget = target;
		/* Recorded when the caller says so, not when the task runs: a wrapper
		 * asking whether the task overrides must not get "no" until execution,
		 * or it stages a brand-new instance. */
		setOverrideInstance(target.instanceId);
	}

	/* Whether the archive came from somewhere we vouch for. A modpack is code
	 * to execute: a pack chosen in the launcher's browser downloads from its
	 * catalogue's CDN, but a typed or on-disk archive can name any host or
	 * carry jars, so those are listed and confirmed first. Untrusted by
	 * default. */
	void setTrustedSource(bool trusted)
	{
		m_trustedSource = trusted;
	}

	explicit InstanceImportTask(const QUrl sourceUrl);

	/* Optional; set by the browser page before NewInstanceDialog adopts the
	 * task. Recorded into the new instance.cfg so PackUpdater can read it
	 * back. */
	void setPackSourceHint(const PackSourceHint& hint)
	{
		m_packHint = hint;
	}

	/* An import is always abortable: nothing is written outside the staging
	 * directory until the pack is in place, so stopping leaves nothing to
	 * clean up. */
	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  protected:
	virtual void executeTask() override;

  private:
	void processZipPack();
	void processKite();
	void processFlame();
	void configureFlameInstance(Flame::Manifest& pack);
	void onFlameFileResolutionSucceeded();
	void processModrinth();
	void processTechnic();

  private slots:
	void downloadSucceeded();
	void downloadFailed(QString reason);
	void downloadProgressChanged(qint64 current, qint64 total);
	void detectFinished();
	void extractFinished();
	/* Decides whether the archive is ours to discard before reporting. */
	void extractFailed();

  private:
	NetJob::Ptr m_filesNetJob;
	shared_qobject_ptr<Flame::FileResolvingTask> m_modIdResolver;
	QUrl m_sourceUrl;
	QString m_archivePath;
	/* The cache slot a remote archive was downloaded into; null for a local
	 * file, which must never be deleted. A failed extraction discards the
	 * file, or every retry would unpack the same broken bytes. */
	MetaEntryPtr m_archiveEntry;
	bool m_downloadRequired = false;

	/* The extraction while it runs. MMCZip::ExtractZipTask reports entry by
	 * entry and can be stopped. */
	Task::Ptr m_extractTask;

	/* Latched by abort(), so the step in flight reports the user's decision
	 * rather than the failure that stopping looked like. */
	bool m_aborted = false;
	enum class ModpackType {
		Unknown,
		Kite,
		Flame,
		Modrinth,
		Technic
	} m_modpackType = ModpackType::Unknown;

	struct DetectResult {
		QString mmcRoot;	  // non-null → Kite pack
		QString flameRoot;	  // non-null → Flame/CurseForge pack
		QString modrinthRoot; // non-null → Modrinth pack
		bool technicFound = false;
		QString extractTarget; // dir to pass to extractSubDir
	};
	QFuture<DetectResult> m_detectFuture;
	QFutureWatcher<DetectResult> m_detectFutureWatcher;
	PackSourceHint m_packHint;
	UpdateTarget m_updateTarget;
	/* The staged instance, reopened once its files are in place so the game's
	 * own files can be fetched for it. A member because that download is
	 * asynchronous; the instance each process*() path builds is a local and is
	 * let go first, since two live settings objects over one instance.cfg
	 * means the last writer wins. */
	std::shared_ptr<MinecraftInstance> m_gameFilesInstance;
	bool m_trustedSource = false;

	/* Persists the pack source hint into the new instance's instance.cfg;
	 * called by every process*() path. Takes a BaseInstance because only the
	 * settings object is needed, and the formats that bring their own
	 * instance.cfg are staged through a NullInstance. BaseInstance
	 * pre-registers the same keys so values survive a save and reload. */
	void writePackSourceToInstance(BaseInstance& instance,
								   const PackSourceHint& hint);

	/* Copies the user's settings (notes, play time, per-instance Java and
	 * window settings, shortcuts) from the instance being replaced onto the
	 * staged one, since the staged instance.cfg replaces the live one on
	 * merge. No-op when this is not an update. */
	void carryOverUserSettings(BaseInstance& instance);

	/* Reopens the staged instance so the game's own files can be fetched for
	 * it, and keeps it alive meanwhile. Null when the staging directory is not
	 * an instance we can drive; the caller then skips the pre-download. */
	MinecraftInstance* openStagedInstance();

	/* Asks the user before installing code from places we cannot vouch for. @p
	 * suspectPaths are instance-relative. True when installation may proceed:
	 * always for a trusted source or an empty list. False means the user
	 * declined and the caller must stop. */
	bool confirmUntrustedFiles(const QStringList& suspectPaths);

	/* Offers to update the instance this catalogue pack is already installed
	 * in, when the caller has not said what to replace (a drag-dropped archive
	 * has no project id, and matching on its name would guess). Returns false
	 * when the user cancelled. Must run first in the per-format processing,
	 * before anything derived from the update target is read. */
	bool resolveUpdateTargetFromCatalogue();

	/* The game directory name inside the staged instance: "minecraft" for a
	 * fresh install, but an update must use whatever the replaced instance
	 * calls it (".minecraft" for other launchers), or the staged directory
	 * lands beside the real one. Resolved on first use and cached. */
	QString gameDirName();
	QString m_gameDirName;

	/* The game directory of the staged instance, found by looking: a Kite pack
	 * keeps its exporter's directory name and a Technic pack's layout is
	 * decided in its processor, so neither can match gameDirName(). */
	QString stagedGameDir();

	/* Records what this pack version installs and works out what the replaced
	 * version leaves behind. @p gameRelativePaths is every file this install
	 * is responsible for (manifest downloads plus overrides), relative to the
	 * game directory. An update also schedules what the old version installed
	 * and this one does not for removal after the merge. Returns false when
	 * the user declined the whole update. */
	bool recordPackContents(const QStringList& gameRelativePaths);

	/* Whether an update may delete files under saves/. A pack can ship a
	 * world, and the copy the user plays on must not be thrown away on a
	 * technicality, so only this folder is asked about, once, and only if
	 * something in it would be deleted. */
	enum class SavesDeletion {
		NotAsked,
		Allowed,
		Refused
	} m_savesDeletion = SavesDeletion::NotAsked;

	/* Files the pack carried in its overrides, relative to the staged game
	 * directory. Collected before any mod is downloaded into it, after which
	 * an override cannot be told from a download. */
	QStringList m_packOverridePaths;

	/* Jars in the pack's own folders once overrides are applied, relative to
	 * @p minecraftDir. A manifest's file list names its hosts, but a pack can
	 * carry mods in its overrides with no URL to inspect; for an untrusted
	 * archive those are the files worth naming. */
	QStringList findBundledCode(const QString& minecraftDir) const;

	/* Mod-metadata sidecar writers are free helpers in the cpp
	 * (writeModrinthModSidecars, writeFlameModSidecars) to keep the
	 * Flame/Modrinth manifest types out of this header. */
};
