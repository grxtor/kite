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

#pragma once

#include <QHash>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <memory>

#include "net/JsonPost.h"
#include "QObjectPtr.h"
#include "tasks/Task.h"

class ContentInstallReview;
class MinecraftInstance;
class ModFolderModel;
class TaskWatcher;

/*
 * Kite Performance for one instance: makes sure the Performance bundle's
 * default mods are installed. It runs before every Play of an instance
 * with the mode on (PreparePerformance), once on its own for "Speed up",
 * and as the last step of PerformanceInstanceTask.
 *
 * An instance without a mod loader is never changed - no loader is added
 * to it: the task ends at once, without the network, saying so.
 *
 * What is wanted is the catalog's default performance mods for the
 * instance's loader, plus the extras the player ticked and keeps
 * (KitePerformanceIncluded), minus the ones the player unticked
 * (KitePerformanceExcluded), plus `extras` -- other catalog mods picked for
 * this one run, e.g. in New instance -- minus what is already there: mods the folder
 * recorded from Modrinth, and jars Modrinth identifies by hash - added by
 * hand, or installed from CurseForge - so a Sodium that is already in the
 * folder never gets a second one beside it. Nothing wanted ends the task at
 * once, without the network.
 *
 * Catalog ids are Modrinth's, so CurseForge is never searched for them. A
 * mod Modrinth answers it has no build of for this Minecraft version and
 * loader is recorded (KitePerformanceMissing, with the match key and the
 * time) and left out of what is wanted for a day, so it costs one lookup a
 * day and not one per Play; a Play that could not reach Modrinth records
 * nothing.
 *
 * The mods are matched to one Minecraft version and loader
 * (KitePerformanceFor). When the instance has moved to another since, the
 * mods of the catalog and the dependencies they pulled in are looked up
 * again and replace their old files, so the game is not started with mods
 * for the version it used to be; a file that still fits costs nothing. The
 * new version is only remembered once Modrinth has answered, so an offline
 * Play tries again. The first setup takes what is there as matched. A mod
 * Modrinth has no build of for the new version stays at its old file, which
 * the game will likely refuse: those are stored (KitePerformanceUnmatched),
 * logged, and named at the end of summary(); QmlShell shows them on screen.
 *
 * One setup runs per instance at a time: one that starts while another is
 * still going waits for it, then checks again from the start - and finds
 * what the other installed.
 *
 * Playing never waits on this succeeding: every failure is logged and the
 * task still succeeds. Its status, once it is over, is summary().
 */
class PerformanceSetupTask : public Task
{
	Q_OBJECT

  public:
	explicit PerformanceSetupTask(std::shared_ptr<MinecraftInstance> instance,
								  const QStringList& extras = {},
								  QObject* parent = nullptr);
	~PerformanceSetupTask() override = default;

	/// What happened, in a sentence; empty until the task is over.
	QString summary() const
	{
		return m_summary;
	}

	/// How many mods the task installed; 0 until it is over.
	int added() const
	{
		return m_added;
	}

	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

  private:
	/// Runs the check unless a setup of the same instance is going: then
	/// waits for it and tries again.
	void startWhenFree();
	void check();
	/// What to install: the defaults, included mods and extras not excluded,
	/// installed or lately missing, and the mods to match to a new version.
	void updateWanted();
	/// The project ids of m_replacements, in order.
	QStringList rematched() const;
	void identifyJars();
	void onJarsIdentified(const QByteArray& reply);
	void resolveWanted();
	void onResolved();
	void install();
	void onInstalled(bool ok);
	/// Stores the version and loader the mods are matched to now.
	void rememberMatch();
	/// Once resolved: stores which wanted mods Modrinth has no build of
	/// (KitePerformanceMissing), and which of the mods to match to a new
	/// version (KitePerformanceUnmatched).
	void rememberNotAvailable();
	/// Lets a setup that waits for this instance go on.
	void release();
	/// Ends the task: a warning in the log if @p problem, the summary as
	/// the final status, success.
	void finish(const QString& summary, const QString& problem = QString());

	std::shared_ptr<MinecraftInstance> m_instance;
	std::shared_ptr<ModFolderModel> m_folder;
	QString m_mcVersion;
	QString m_loader;
	QStringList m_defaults;
	QStringList m_extras;
	QStringList m_excluded;
	QStringList m_included;
	/// Catalog mods Modrinth had no build of lately, for this match key.
	QStringList m_missing;
	QStringList m_installed;
	QStringList m_wanted;
	/// Files to replace by their project id: what an earlier version or
	/// loader left; empty when the mods are matched to the current one.
	QHash<QString, QString> m_replacements;
	QString m_matchKey;
	/// Said at the end of the summary: the mods that follow no version.
	QString m_unmatchedNote;
	int m_toInstall = 0;
	int m_added = 0;
	QString m_summary;
	bool m_aborted = false;
	bool m_started = false;

	shared_qobject_ptr<Net::JsonPost> m_lookup;
	QPointer<ContentInstallReview> m_review;
	QPointer<TaskWatcher> m_download;
};
