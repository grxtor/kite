/* SPDX-FileCopyrightText: 2026 Project Tick
 * SPDX-FileContributor: Project Tick
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
	/* Catalogue identifiers for a pack the user picked through the
	 * launcher's own Modrinth / CurseForge browser. The browser
	 * already knows the slug + version, so we hand them over here
	 * instead of trying to recover them from the manifest after
	 * import. Drag-drop imports leave this empty and let
	 * processFlame / processModrinth populate the same fields from
	 * the manifest. */
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

	/* Where an update should land, when this import is replacing an
	 * existing instance rather than creating a new one.
	 *
	 * Set by ManagedPackPage. With this set the task stages the new pack
	 * exactly as it would for a fresh install, and the staging step
	 * merges the result over `instanceId` instead of creating a second
	 * instance - so the instance keeps its id, its directory and
	 * everything on disk that the pack does not itself ship.
	 *
	 * `versionId` / `versionLabel` are what the instance should claim to
	 * have installed afterwards. Both are empty for an update from a
	 * local file or a user-supplied URL, where we have no trustworthy
	 * version to record and would rather record nothing than a guess. */
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
		/* Recorded on the task the moment the caller says so, not when
		 * the task later runs. Anything that wraps this task for staging
		 * is entitled to ask whether it overrides, and a task that
		 * answers "no" until it starts executing is a task that gets
		 * staged as a brand new instance. */
		setOverrideInstance(target.instanceId);
	}

	/* Parent for the dialogs the task may need to raise (confirmations,
	 * warnings). Null is allowed and simply means the dialog is
	 * parentless; it is not a reason to skip asking. */
	/* Whether the archive came from somewhere we vouch for.
	 *
	 * A modpack is a list of code to execute. When the launcher itself
	 * chose the download - the user picked a pack in the Modrinth or
	 * CurseForge browser, or a version in the pack page - the mod files
	 * come from that catalogue's own CDN and the user has already
	 * decided to trust it. When the address came from the user instead,
	 * whether typed in or picked off disk, the archive is free to point
	 * its mod downloads at any host at all, and to carry jars directly
	 * inside its overrides. Those get listed and confirmed before
	 * anything is fetched.
	 *
	 * Untrusted is the default on purpose: a new call site that forgets
	 * to say anything gets the careful path, not the silent one. */
	void setTrustedSource(bool trusted)
	{
		m_trustedSource = trusted;
	}

	explicit InstanceImportTask(const QUrl sourceUrl);

	/* Optional — set by the browser page right before NewInstanceDialog
	 * adopts the task. The task records these fields into the freshly
	 * created instance's instance.cfg so PackUpdater can read them
	 * back through `instance_setting_get` without sniffing manifests. */
	void setPackSourceHint(const PackSourceHint& hint)
	{
		m_packHint = hint;
	}

	/*
	 * An import is always abortable.
	 *
	 * Every step of it is either a download, an extraction or a lookup,
	 * and none of those is a step that has to be seen through: nothing
	 * is written outside the staging directory until the whole pack is
	 * in place, so stopping halfway leaves nothing behind to clean up
	 * by hand.
	 *
	 * This used to answer false - inherited, never overridden - which
	 * greyed out the progress dialog's own abort button and left a
	 * multi-gigabyte extraction with no way out but killing the
	 * launcher.
	 */
	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  protected:
	//! Entry point for tasks.
	virtual void executeTask() override;

  private:
	void processZipPack();
	void processMeshMC();
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
	/* The archive could not be unpacked. Decides whether the file is
	 * ours to discard before reporting. */
	void extractFailed();

  private: /* data */
	NetJob::Ptr m_filesNetJob;
	shared_qobject_ptr<Flame::FileResolvingTask> m_modIdResolver;
	QUrl m_sourceUrl;
	QString m_archivePath;
	/* The cache slot a remote archive was downloaded into; null for a
	 * local file, which we must never delete. Kept so that a failed
	 * extraction can discard the file - an archive that will not unpack
	 * is worthless, and while it stays cached and fresh every retry
	 * unpacks the same broken bytes. */
	MetaEntryPtr m_archiveEntry;
	bool m_downloadRequired = false;

	/* The extraction, while it runs.
	 *
	 * Used to be a QFuture over MMCZip::extractSubDir(): the work did
	 * happen off the UI thread, but nothing came back until it was over,
	 * so a large pack showed a frozen "Extracting modpack" with a bar
	 * that never moved. Worse, the future QtConcurrent::run() hands back
	 * cannot be cancelled, so the watcher's canceled() signal - which
	 * this class connected to an abort handler - could never fire.
	 * MMCZip::ExtractZipTask reports entry by entry and can be told to
	 * stop. */
	Task::Ptr m_extractTask;

	/* Latched by abort(), so the handler of whichever step was in flight
	 * reports the user's decision rather than the failure that stopping
	 * looked like from inside. */
	bool m_aborted = false;
	enum class ModpackType {
		Unknown,
		MeshMC,
		Flame,
		Modrinth,
		Technic
	} m_modpackType = ModpackType::Unknown;

	// Holds the raw detection results from the background scan.
	struct DetectResult {
		QString mmcRoot;	  // non-null → MeshMC pack
		QString flameRoot;	  // non-null → Flame/CurseForge pack
		QString modrinthRoot; // non-null → Modrinth pack
		bool technicFound = false;
		QString extractTarget; // dir to pass to extractSubDir
	};
	QFuture<DetectResult> m_detectFuture;
	QFutureWatcher<DetectResult> m_detectFutureWatcher;
	PackSourceHint m_packHint;
	UpdateTarget m_updateTarget;
	/* The staged instance, reopened once its files are all in place so
	 * that the game's own files can be fetched for it.
	 *
	 * A member because that download is asynchronous and runs against
	 * this object; the instance each process*() path builds while
	 * configuring the pack is a local, and is deliberately let go before
	 * this is opened - two live settings objects over one instance.cfg
	 * means whichever writes last wins. */
	std::shared_ptr<MinecraftInstance> m_gameFilesInstance;
	bool m_trustedSource = false;

	/* Helper: persist the pack source hint into the freshly created
	 * instance's instance.cfg. Called by every process*() path right
	 * after it finishes setting up the instance. Centralised here so the
	 * key names stay in one place; BaseInstance pre-registers the same
	 * keys so the values survive a save+reload cycle.
	 *
	 * Takes a BaseInstance because that is all it needs - the settings
	 * object - and because the two archive formats that carry their own
	 * instance.cfg are staged through a NullInstance rather than a
	 * MinecraftInstance. */
	void writePackSourceToInstance(BaseInstance& instance,
								   const PackSourceHint& hint);

	/* Copy the settings that belong to the *user* rather than to the
	 * pack from the instance being replaced onto the freshly staged one.
	 *
	 * An update stages a brand new instance directory, instance.cfg
	 * included, and the staging step then merges it over the live
	 * instance - which means the new config file replaces the old one.
	 * Without this, updating a pack would silently discard the notes the
	 * user wrote, their play time, their per-instance Java and window
	 * settings, and the shortcuts they made. None of that is information
	 * the pack has any opinion about, so none of it should be lost to a
	 * pack update.
	 *
	 * No-op when this is not an update. */
	void carryOverUserSettings(BaseInstance& instance);

	/* Reopen the staged instance so the game's own files can be fetched
	 * for it, and keep it alive for as long as that takes.
	 *
	 * Returns null when the staging directory does not describe an
	 * instance we can drive, which the caller treats as "nothing to
	 * pre-download" rather than as an error - the pack itself is already
	 * installed by then.
	 */
	MinecraftInstance* openStagedInstance();

	/* Ask the user before installing code from places we cannot vouch
	 * for.
	 *
	 * @p suspectPaths is what the caller found questionable, as
	 * instance-relative paths. Returns true when installation may
	 * proceed - always true for a trusted source, and always true when
	 * the list came back empty, so callers can hand over whatever they
	 * found without checking first.
	 *
	 * Returning false means the user declined; the caller must stop and
	 * say so rather than carry on with a subset. */
	bool confirmUntrustedFiles(const QStringList& suspectPaths);

	/* Offer to update the instance this pack is already installed in,
	 * when there is one and the caller has not already said what to
	 * replace.
	 *
	 * Installing a pack the user already has is ambiguous: they may want
	 * a second copy to experiment in, or they may be trying to update the
	 * one they play. Without asking, the launcher always chose the first
	 * reading and produced a duplicate instance - including when the user
	 * came to the browser precisely because they wanted a newer version.
	 *
	 * Only asked when the pack was picked from a catalogue, which is the
	 * only case where "the same pack" means anything precise: a
	 * drag-dropped archive carries no project id, and matching on the
	 * pack's name would be guessing at which instance to overwrite.
	 *
	 * Returns false when the user cancelled the installation outright.
	 * Must run before anything derived from the update target - the game
	 * directory's name, the settings carried over, the file list diff -
	 * is read, which in practice means first thing in the per-format
	 * processing. */
	bool resolveUpdateTargetFromCatalogue();

	/* The name of the game directory inside the staged instance.
	 *
	 * "minecraft" for a fresh install, but an update has to use whatever
	 * the instance it is replacing calls it - instances made by other
	 * launchers, and by this one outside the modpack importer, use
	 * ".minecraft". The commit step merges the staged root over the live
	 * one, so a game directory staged under the other name lands *beside*
	 * the real one instead of on top of it: the mods go into a folder
	 * nothing reads, gameRoot() keeps answering with the old directory,
	 * and the update looks like it simply did nothing.
	 *
	 * Resolved on first use rather than when the update target is set,
	 * so that it reflects the instance as it is when the task actually
	 * runs, and cached so that one import cannot stage half its files
	 * under each name. */
	QString gameDirName();
	QString m_gameDirName;

	/* The game directory of the staged instance, found by looking rather
	 * than by naming.
	 *
	 * For the formats that bring their own instance layout instead of
	 * having us build one: a MeshMC pack carries whatever directory name
	 * its exporter used, and a Technic pack's layout is decided inside
	 * the Technic processor. Neither can be made to agree with
	 * gameDirName(), so the question is answered the same way the
	 * finished instance will answer it. */
	QString stagedGameDir();

	/* Record what this pack version installs, and work out what the
	 * version being replaced leaves behind.
	 *
	 * @p gameRelativePaths is every file this install is responsible for,
	 * relative to the staged instance's game directory: the manifest's
	 * downloads plus whatever the pack carried in its overrides.
	 *
	 * On a fresh install this only writes the list, which is what makes
	 * the *next* update able to clean up after this one. On an update it
	 * also compares against the list the installed version left, and
	 * schedules whatever that version installed and this one does not for
	 * removal after the merge.
	 *
	 * Returns false when the user declined to go on, in which case the
	 * caller must stop and say so; a "no" here is about the whole update,
	 * not about the file list. */
	bool recordPackContents(const QStringList& gameRelativePaths);

	/* Whether an update may delete files under saves/.
	 *
	 * A pack can ship a world, and dropping it from a later version then
	 * makes the copy the user has been playing on "stale" - except that
	 * their world is not a file the launcher gets to throw away on a
	 * technicality. So this one folder is asked about, once, and only if
	 * something in it is actually going to be deleted. */
	enum class SavesDeletion {
		NotAsked,
		Allowed,
		Refused
	} m_savesDeletion = SavesDeletion::NotAsked;

	/* Files the pack carried in its overrides, relative to the staged
	 * game directory.
	 *
	 * Collected while they are still the only thing in that directory -
	 * before any mod is downloaded into it - because afterwards there is
	 * no way to tell an override apart from a download. */
	QStringList m_packOverridePaths;

	/* Jars sitting in the pack's own folders once overrides have been
	 * applied, as paths relative to @p minecraftDir.
	 *
	 * A manifest's file list is checkable - every entry names the host
	 * it will be fetched from - but a pack can also just carry mods
	 * inside its overrides, where there is no URL to inspect and no
	 * catalogue entry behind them. For an archive we do not vouch for,
	 * those are precisely the files worth naming. */
	QStringList findBundledCode(const QString& minecraftDir) const;

	/* Mod-metadata sidecar writers (declared as free helpers in
	 * the cpp — see writeModrinthModSidecars / writeFlameModSidecars
	 * — to keep this header free of full Flame/Modrinth manifest
	 * type definitions). */
};
