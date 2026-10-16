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

#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <memory>

#include "modplatform/ContentType.h"
#include "modplatform/ModDownloadTypes.h"
#include "net/NetJob.h"
#include "tasks/Task.h"

class ModMetadataIndex;

/*
 * ModUpdateCheckTask
 *
 * Walks every entry of a ModMetadataIndex that has a remote provenance and
 * asks the source platform for the newest compatible version. Whenever the
 * remote version differs from the recorded one, a ready-to-download
 * DownloadItem is produced (with replaceExisting + replacesFileName already
 * populated).
 *
 * This task is read-only with respect to the index — it never mutates the
 * sidecars itself. The caller is expected to hand the resulting plan to a
 * ContentDownloadTask, which both writes the new files and updates the
 * sidecars on success.
 */
class ModUpdateCheckTask : public Task
{
	Q_OBJECT
  public:
	struct UpdateInfo {
		QString currentFileName;
		QString currentVersionId;
		QString newVersionId;
		QString name;
		QString platform;
		ModPlatform::DownloadItem item;
		/* The two versions as a person reads them ("0.6.5"), for a review
		 * list; the ids above are each platform's own opaque handles.
		 * Either may be empty when nothing readable was recorded or
		 * offered. */
		QString currentVersionNumber;
		QString newVersionNumber;
	};

	/* `loader` is ignored for content that has no loaders (resource
	 * packs, shader packs, data packs) - see the constructor. */
	ModUpdateCheckTask(std::shared_ptr<ModMetadataIndex> index,
					   QString mcVersion, QString loader,
					   ModPlatform::ContentType contentType,
					   QObject* parent = nullptr);

	/* Whatever was found so far. Still meaningful after abort(): the
	 * mods that were checked before the user gave up are checked, and
	 * throwing that away would only make them do it again. */
	QList<UpdateInfo> availableUpdates() const
	{
		return m_updates;
	}

	/* One lookup per tracked mod, so a large instance takes a while and
	 * the progress dialog offers an Abort button for it. */
	bool canAbort() const override
	{
		return true;
	}

  public slots:
	bool abort() override;

  protected:
	void executeTask() override;

  private:
	void onOneDone();

	std::shared_ptr<ModMetadataIndex> m_index;
	QString m_mcVersion;
	QString m_loader;
	ModPlatform::ContentType m_contentType = ModPlatform::ContentType::Mod;
	QList<UpdateInfo> m_updates;
	int m_pending = 0;
	int m_total = 0;
	int m_completed = 0;

	/* Lookups still in flight, so abort() can call them off. Guarded
	 * pointers because a job deletes itself once it has reported. */
	QList<QPointer<NetJob>> m_activeJobs;
	/* Latched by abort(): replies still arrive but are dropped, and no
	 * second verdict is given on top of the aborted one. */
	bool m_aborted = false;
};
