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
 * default mods are installed, adding Fabric first to an instance that has
 * no loader. It runs before every Play of an instance with the mode on
 * (PreparePerformance) and once on its own for "Speed up".
 *
 * What is wanted is the catalog's default performance mods for the
 * instance's loader, minus the ones the player unticked
 * (KitePerformanceExcluded), minus what is already there: mods the folder
 * recorded from Modrinth, and jars added by hand that Modrinth identifies
 * by hash - so a hand-added Sodium never gets a second one beside it.
 * Nothing wanted ends the task at once, without the network.
 *
 * Playing never waits on this succeeding: every failure is logged and the
 * task still succeeds. Its status, once it is over, is summary().
 */
class PerformanceSetupTask : public Task
{
	Q_OBJECT

  public:
	explicit PerformanceSetupTask(std::shared_ptr<MinecraftInstance> instance,
								  QObject* parent = nullptr);
	~PerformanceSetupTask() override = default;

	/// What happened, in a sentence; empty until the task is over.
	QString summary() const
	{
		return m_summary;
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
	void identifyLooseJars();
	void onJarsIdentified(const QByteArray& reply);
	void resolveWanted();
	void onResolved();
	void loadLoaderVersion();
	void onLoaderListReady();
	void install();
	void onInstalled(bool ok);
	void addLoader();
	/// Ends the task: a warning in the log if @p problem, the summary as
	/// the final status, success.
	void finish(const QString& summary, const QString& problem = QString());

	std::shared_ptr<MinecraftInstance> m_instance;
	std::shared_ptr<ModFolderModel> m_folder;
	QString m_mcVersion;
	QString m_loader;
	bool m_vanilla = false;
	QStringList m_defaults;
	QStringList m_excluded;
	QStringList m_installed;
	QStringList m_wanted;
	QString m_loaderVersion;
	int m_toInstall = 0;
	QString m_summary;
	bool m_aborted = false;

	shared_qobject_ptr<Net::JsonPost> m_lookup;
	Task::Ptr m_loaderListLoad;
	QPointer<ContentInstallReview> m_review;
	QPointer<TaskWatcher> m_download;
};
